#include "pch.h"
#include "SenderSocket.h"

#pragma comment(lib, "Ws2_32.lib")


UINT statsRunThread(LPVOID pParam) {
	SenderSocket* p = ((SenderSocket*)pParam);
	p->StatsRun();
	return 0;
}

UINT workerRunThread(LPVOID pParam) {
	SenderSocket* p = ((SenderSocket*)pParam);
	p->WorkerRun();
	return 0;
}

int SenderSocket::Write(int attempts, struct timeval timeout, int packet_size) {

    int count = 0;
    clock_t start_;
    clock_t end_;
    double elapsed_time;
    while (count++ < attempts) {
        start_ = clock();
        
        if (sendto (sock, packet, packet_size, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR) {
            end_ = clock();
            elapsed_time = ((double)(end_ - start) / CLOCKS_PER_SEC);
            printf("[%6.3f] --> failed sendto with %d\n", elapsed_time, WSAGetLastError());
            return FAILED_SEND;
        }

        fd_set fd;   // file descriptor
        FD_ZERO(&fd);
        FD_SET(sock, &fd);

        int ret = select(0, &fd, NULL, NULL, &timeout);
        if (ret > 0) {
            struct sockaddr_in response;
            int size_of_sockaddr = sizeof(sockaddr);

            ReceiverHeader rh;
            int bytes;
            if ((bytes = recvfrom(sock, (char*)&rh, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size_of_sockaddr)) == SOCKET_ERROR) {
                end_ = clock();
                elapsed_time = ((double)(end_ - start) / CLOCKS_PER_SEC);
                printf("[%6.3f] <-- failed recvfrom with %d\n", elapsed_time, WSAGetLastError());
                return FAILED_RECV;
            }

            // check if this packet came from the server to which we sent the query earlier
            if (response.sin_addr.S_un.S_addr != remote.sin_addr.S_un.S_addr || response.sin_port != remote.sin_port) {
                printf("bogus reply\n");
                continue;
            }

            // end = clock();
            // double elapsed_time2 = ((double)(end - start) / CLOCKS_PER_SEC);

            end_ = clock();
            double sampleRTT = ((double)(end_ - start_) / CLOCKS_PER_SEC);

            if (attempts == 3) {
                estRTT = sampleRTT; // EstimatedRTT(0) = SampleRTT(0)
                devRTT = 0;
            }
            else {
                estRTT = (1 - ALPHA)*estRTT + (ALPHA)*sampleRTT;
                devRTT = (1 - BETA)*devRTT + (BETA)*abs(sampleRTT - estRTT);
            }
            estRTT_reader = estRTT; // assigned after operations for atomicity
            rto = estRTT + 4 * max(devRTT, 0.010); 


            recvWnd = rh.recvWnd;
            // effective_window_size = min(effective_window_size, rh.recvWnd);
            InterlockedExchange(&effective_window_size, min(W, rh.recvWnd));


            // if (attempts == 3) { //open
            //     printf("[%6.3f] <-- SYN-ACK %d window %d; setting initial RTO to %.3f\n",
            //     elapsed_time2,
            //     seq,
            //     rh.recvWnd,
            //     rto
            //     );
            // }
            // else if (attempts == 5) { //close
            //     printf("[%6.3f] <-- SEND & FIN-ACK %d window %d\n", elapsed_time2, seq, rh.recvWnd);
            // }
            // printf("sampleRTT = %f, estRTT = %f, devRTT = %f\n", sampleRTT, estRTT, devRTT);
            // printf("ack value = next expected sequence = %d\n", rh.ackSeq);

            // if (seq < rh.ackSeq) {
            //     // seq = rh.ackSeq; // may need to change
            //     InterlockedExchange(&seq, rh.ackSeq); // atomic write
            // }
            // else {
            //     // printf("!GOT %d, expected %d\n\n", rh.ackSeq - 1, seq);
            // }

            // after the SYN-ACK, inside ss.Open()
            // printf("%lu\n", recvWnd);
            lastReleased = min (W, recvWnd);
            ReleaseSemaphore (empty, lastReleased, NULL);
            
            return STATUS_OK;
        }
        else if (ret < 0) {
            printf("select failed with error: %d\n", WSAGetLastError());
        }
        InterlockedAdd(&timeouts, 1);
    }

    return TIMEOUT;
}


int SenderSocket::Open(char *targetHost, int port, int senderWindow, LinkProperties *linkProperties) {

    if (sock != -1) {
        return ALREADY_CONNECTED;
    }

    // ***** Bind sock
    sock = socket (AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        return -1;
    }

    // bind localsock to port 0
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(0);
    if (bind (sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        return -1;
    }

    // set remote to input port
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);

    struct hostent* remote_server;
    // structure for connecting to server
    // first assume that the string is an IP address
    DWORD IP = inet_addr(targetHost);
    if (IP == INADDR_NONE) {
        // if not a valid IP, then do a DNS lookup
        if ((remote_server = gethostbyname(targetHost)) == NULL) {
            clock_t end = clock();
            double elapsed_time = ((double)(end - start) / CLOCKS_PER_SEC);
            printf("[%6.3f] --> target %s is invalid\n", elapsed_time, targetHost);
            return INVALID_NAME;
        }
        // take the first IP address and copy into sin_addr
        memcpy((char*)&(remote.sin_addr), remote_server->h_addr, remote_server->h_length);
        targetIP = inet_ntoa(remote.sin_addr);
    }
    else {
        // if a valid IP, directly drop its binary version into sin_addr
        remote.sin_addr.S_un.S_addr = IP;
        targetIP = targetHost;
    }


    seq = 0;
    nextToSend = 0;
    sndBase = 0;
    total_bytes = 0;
    timeouts = 0;
    fast_rtxs = 0;
    effective_window_size = 1;
    W = senderWindow;
    closeACK = -1;
    pending_pkts = new Packet[senderWindow];

    empty = CreateSemaphore (NULL, 0, W, NULL) ;
    full = CreateSemaphore (NULL, 0, W, NULL) ;

    int attempts = 3;

    Flags f;
    f.SYN = 1;

    SenderDataHeader sdh;
    sdh.flags = f;
    sdh.seq = seq;

    SenderSynHeader* ssh = (SenderSynHeader*) packet;
    ssh->sdh = sdh;
    linkProperties->bufferSize = senderWindow + attempts;
    ssh->lp = *linkProperties;

    struct timeval timeout;
    timeout.tv_sec = 0;
    // get max and convert to microseconds
    rto = max(1, 2*linkProperties->RTT);
    timeout.tv_usec = long(rto * 1e6);

    int ret = Write(attempts, timeout, sizeof(SenderSynHeader));

    // after the SYN-ACK, inside ss.Open()
    // printf("%lu\n", recvWnd);
    // lastReleased = min (W, recvWnd);
    // ReleaseSemaphore (empty, lastReleased, NULL);

    WSAEventSelect(sock, socketReceiveReady, FD_READ);

    // timerExpire = INFINITE;
    statsHandle = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)statsRunThread, this, 0, NULL);
    workerHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)workerRunThread, this, 0, NULL);
    
    return ret;
}


int SenderSocket::Close() {

    if (sock == -1) {
        return NOT_CONNECTED;
    }

    DWORD status = WaitForSingleObject(statsHandle, 0);
    if (status == WAIT_TIMEOUT) {
        quitStatsRun();
        WaitForSingleObject (statsHandle, INFINITE);
    }

    // int attempts = 5;

    // Flags f_;
    // f_.FIN = 1;

    // SenderDataHeader* sdh_ = (SenderDataHeader*) packet;
    // sdh_->flags = f_;
    // sdh_->seq = seq;

    // struct timeval timeout;
    // timeout.tv_sec = 0;
    // timeout.tv_usec = long(rto * 1e6);

    closeACK = seq;

    HANDLE arr[] = {eventQuit, empty};

    WaitForMultipleObjects (2, arr, false, INFINITE);
    
    int slot = seq % W;
    Packet *p = pending_pkts + slot; // pointer to packet struct

    SenderDataHeader* sdh = (SenderDataHeader*) p->pkt;
    Flags f;
    f.FIN = 1;
    sdh->flags = f;
    sdh->seq = seq;

    p->size = sizeof(SenderDataHeader);
    p->txTime = -1; // not set
    p->total_rtx = 0;

    // memcpy (sdh + 1, buf, bytes);

    // if (!close) {
    //     seq++;
    // }
    
    ReleaseSemaphore(full, 1, NULL);
    
    // while (true) {
    //     if (sndBase == closeACK)
    //     {
    //         break;
    //     } 
    // }
    WaitForSingleObject (workerHandle, INFINITE);
    // Sleep(10000);
    

    // Send(packet, 0, true);

    // printf("HERE\n");

    // int ret = Write(attempts, timeout, sizeof(SenderDataHeader));

    clock_t end = clock();
    double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("[%.2f] <-- FIN-ACK %lu window %X\n", elapsed_time, seq, recvWnd);

    closesocket(sock);
    sock = -1; // make sock usable by Open()

    delete pending_pkts;

    return STATUS_OK;
}


int SenderSocket::Send(char* buf, int bytes) {

    if (sock == -1) {
        return NOT_CONNECTED;
    }

    HANDLE arr[] = {eventQuit, empty};

    // printf("SEND wait\n");
    WaitForMultipleObjects (2, arr, false, INFINITE);
    // printf("SEND go\n");
    
    int slot = seq % W;
    Packet *p = pending_pkts + slot; // pointer to packet struct

    SenderDataHeader* sdh = (SenderDataHeader*) p->pkt;
    Flags f;
    // if (close)
    // {
    // f.FIN = 1;
    // }
    
    sdh->flags = f;
    sdh->seq = seq;

    p->size = bytes + sizeof(SenderDataHeader);
    p->txTime = -1; // not set
    p->total_rtx = 0;

    // if (!close) {
    memcpy (sdh + 1, buf, bytes);
    seq++;
    // }
    
    ReleaseSemaphore(full, 1, NULL);

    total_bytes += bytes;
    // ***********

    // struct timeval timeout;
    // timeout.tv_sec = 0;
    // timeout.tv_usec = long(rto * 1e6);

    // int attempts = 5;

    // int ret = Write(attempts, timeout, sizeof(SenderDataHeader) + bytes);

    // InterlockedAdd(&total_bytes, bytes);

    return STATUS_OK;
}


// int SenderSocket::Send (char *data, int size)
// {
//     HANDLE arr[] = {eventQuit, empty};
//     WaitForMultipleObjects (2, arr, false, INFINITE);
//     // no need for mutex as no shared variables are modified
//     int slot = seq % W;
//     Packet *p = pending_pkts + slot; // pointer to packet struct
//     SenderDataHeader *sdh = (SenderDataHeader*) p->pkt;
//     sdh->seq = seq;
//     // set up remaining fields in sdh and p
//     memcpy (sdh + 1, data, size);
//     seq++;
//     ReleaseSemaphore(full, 1, NULL);
// } 


void SenderSocket::StatsRun() {
    start = clock();
    clock_t end;
    double elapsed_time;
    DWORD seq_reader;
    DWORD prev_base = 0;
    double speed;

    while (WaitForSingleObject (eventQuit, 2000) == WAIT_TIMEOUT)
    {
        end = clock();
        elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
        seq_reader = seq;
        speed = (seq_reader-1 - prev_base) * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) / 2.0 * 1e-6;        

        printf("[%2d] B %6lu (%5.1f MB) N %6lu T %ld F %ld W %lu S %.3f Mbps RTT %.3f\n",
        static_cast<int>(std::round(elapsed_time)),
        seq_reader-1,
        total_bytes * 1e-6,
        seq_reader,
        timeouts,
        fast_rtxs,
        effective_window_size,
        speed,
        estRTT_reader
        );

        prev_base = seq_reader-1;   
    }
};

 
void SenderSocket::quitStatsRun() { SetEvent (eventQuit); };


SenderSocket::~SenderSocket() {
    DWORD status = WaitForSingleObject(statsHandle, 0);
    if (status == WAIT_TIMEOUT) {
        quitStatsRun();
        WaitForSingleObject (statsHandle, INFINITE);
    }
}


void SenderSocket::WorkerRun (void) {
    HANDLE events [] = {socketReceiveReady, full};
    DWORD timeout;
    dupACK = 0;
    dupRTX = 0;

    // DWORD dupACK = 0;
    // DWORD rtx = 0;
    DWORD oldSndBase;
    bool first_packet;
    bool got_fin = false;


    int kernelBuffer = (int)20e6; // 20 meg
    if (setsockopt (sock, SOL_SOCKET, SO_RCVBUF, (const char*)&kernelBuffer, sizeof (int)) == SOCKET_ERROR) return;
    kernelBuffer = (int)20e6; // 20 meg
    if (setsockopt (sock, SOL_SOCKET, SO_SNDBUF, (const char*)&kernelBuffer, sizeof (int)) == SOCKET_ERROR) return;
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    //debug
    SenderDataHeader *sdh;

    while (true) {
        oldSndBase = sndBase;
        first_packet = false;

        // printf("nextToSend = %lu\n", nextToSend);

        if (sndBase < nextToSend){
            // printf("here\n");
            // printf("timerExpire 3 %f\n", timerExpire - rto);
            // printf("curr time 3 %f\n", (double)clock() / CLOCKS_PER_SEC);
        
            timeout = (DWORD) ((timerExpire - (double)clock() / CLOCKS_PER_SEC) * 1e3); // (timerExpire - cur_time) * 1e3     
        }
        else {
            // printf("Inf\n");
            timeout = INFINITE;

        }

        // printf("waiting\n");
        int ret = WaitForMultipleObjects (2, events, false, timeout);
        // printf("done\n");

        
        switch (ret) {
        case WAIT_TIMEOUT:
            // printf("timeout\n");

            // (b) timeout:
            // retransmit pending segment with smallest sequence number (i.e., SendBase); restart timer


            sendto (sock, pending_pkts[sndBase % W].pkt, pending_pkts[sndBase % W].size, 0, (struct sockaddr*)&remote, sizeof(remote)); // retx
            pending_pkts[sndBase % W].txTime = clock();

            // pending_pkts[sndBase % W].total_rtx++;
            // if (pending_pkts[sndBase % W].total_rtx >= 50)
            // {
            //     printf("MAX RTX REACHED\n");
            //     max_rtx = true;
            // }
            


            dupRTX++;
            timeouts++;
            break;
        case WAIT_OBJECT_0: // move senderBase; update RTT; handle fast retx; do flow control
            ReceiveACK (got_fin);
            
            break;
        case WAIT_OBJECT_0 + 1:
            // printf("new send %lu\n", nextToSend);

            sdh = (SenderDataHeader*) pending_pkts[nextToSend % W].pkt;   
            // printf("packseq %lu\n", sdh->seq);
            // printf("syn %lu\n", sdh->flags.SYN);
            // printf("fin %lu\n", sdh->flags.FIN);
            // printf("reserved %lu\n", sdh->flags.reserved);
            // printf("magic %lu\n", sdh->flags.magic);
            // printf("ACK %lu\n", sdh->flags.ACK);
            
            // if (sdh->seq == closeACK)
            // {
            //     printf("*****\n");
            //     printf("%lu %lu %lu %lu\n", sdh->seq, sdh->flags.FIN, nextToSend, nextToSend % W);
            //     // printf("%lu %lu\n", sdh->flags.ACK, sdh->flags.SYN);
            //     // printf("%lu %lu\n", sdh->flags.ACK, sdh->flags.reserved);
            //     printf("size %d\n", pending_pkts[nextToSend % W].size);
            // }
            
            
            // (a) data received from application above (assuming it fits into window):
            // create TCP segment with sequence number NextSeqNum
            // pass segment to IP
            
            if ( sendto (sock, pending_pkts[nextToSend % W].pkt, pending_pkts[nextToSend % W].size, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR)
            {
                printf("ERROR\n");
            } 

            // printf("curr time 1 %lu\n", pending_pkts[nextToSend % W].txTime);


            // NextSeqNum = NextSeqNum + length(data)
            nextToSend++;

            // if (timer currently not running)
            // start timer
            if (pending_pkts[nextToSend % W].txTime == -1)
            {
                pending_pkts[nextToSend % W].txTime = clock();
            }
            
            // printf("curr time 1 %f\n", (double)clock() / CLOCKS_PER_SEC);

            
            if (nextToSend == sndBase)
            {
                // printf("firstpkt? %lu %lu\n", nextToSend, sndBase);
                first_packet = true;
            }


            
            break;
        default:
            // handle failed wait;
            printf("FAILED WAIT %lu\n", GetLastError());
            break;
        }

        if (got_fin)
        {
            break;
        }
        

        bool retransmission = (dupACK == 3) || (ret == WAIT_TIMEOUT);
        // if (first packet of window || just did a retx (timeout / 3-dup ACK) || senderBase moved forward)
        // printf("check recalc %d\n", retransmission);
        if ( first_packet || retransmission || (oldSndBase < sndBase) ) {
            // printf("RECALC\n");
            timerExpire = (double)clock() / CLOCKS_PER_SEC + rto;
        }
        

        // if (closeACK == sndBase)
        // {
        //     break;
        // }

        // printf("\n\n");


    }
}


void SenderSocket::ReceiveACK(bool &got_fin) {

    struct sockaddr_in response;
    int size_of_sockaddr = sizeof(sockaddr);

    ReceiverHeader rh;
    int bytes;
    if ((bytes = recvfrom(sock, (char*)&rh, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size_of_sockaddr)) == SOCKET_ERROR) {
        printf("Error\n");
        return;
    }

    // check if this packet came from the server to which we sent the query earlier
    if (response.sin_addr.S_un.S_addr != remote.sin_addr.S_un.S_addr || response.sin_port != remote.sin_port) {
        printf("bogus reply\n");
        return;
    }




    // (c) event: ACK received, with ACK field value of y
    // if (y > SendBase) {
    //     SendBase = y; dupACK = 0;
    //     if (SendBase != NextSeqNum)
    //          restart timer with latest RTO;
    //      else
    //          cancel timer // last pkt in window
    // }
    // else if (y == SendBase) {
    //     dupACK++;
    //     if (dupACK == 3)
    //          { resend segment with sequence y; restart timer}
    // {

    DWORD seq_ = rh.ackSeq;
    // printf("recieveACK %lu \n", seq_);
    // printf("base %lu \n", sndBase);


    if (seq_ > sndBase)
    {
        //  upon receiving an ACK that moves the base from x to x + y, an RTT sample is
        //  computed only based on packet x + y – 1 and only if there were no prior retransmissions of base x
        if (dupRTX == 0) // Only recompute rto if there are no prior retransmissions
        {
            // printf("No rtx\n");

            // printf("curr time 2 %f\n", (double)clock() / CLOCKS_PER_SEC);
            
            // double start_ = timerExpire - rto;
            // double end_ = (double)clock() / CLOCKS_PER_SEC;
            // double sampleRTT = end_ - start_;

            
            clock_t start_ = pending_pkts[(seq_-1) % W].txTime;
            clock_t end_ = clock();
            double sampleRTT = ((double)(end_ - start_)) / CLOCKS_PER_SEC;

            if (estRTT == -1) {
                estRTT = sampleRTT; // EstimatedRTT(0) = SampleRTT(0)
                devRTT = 0;
            }
            else {
                estRTT = (1 - ALPHA)*estRTT + (ALPHA)*sampleRTT;
                devRTT = (1 - BETA)*devRTT + (BETA)*abs(sampleRTT - estRTT);
            }
            estRTT_reader = estRTT; // assigned after operations for atomicity
            rto = estRTT + 4 * max(devRTT, 0.010);
        }

        dupACK = 0;
        // printf("rtx %lu\n\n", rtx);
		dupRTX = 0;
        recvWnd = rh.recvWnd;

        sndBase = seq_;

        // flow control
		effective_window_size = min(W, rh.recvWnd);
        // how much we can advance the semaphore 
		DWORD newReleased = sndBase + effective_window_size - lastReleased;
		ReleaseSemaphore(empty, newReleased, NULL);
		lastReleased += newReleased;



        // printf("newReleased %d\n", newReleased);

		// printf("ReleaseSemaphore getlasterror %lu\n", GetLastError());

    }
    else if (seq_ == sndBase) {
        if (rh.flags.FIN == 1) {
            recvWnd = rh.recvWnd;
            got_fin = true;
        }

        // printf("DUP ACK\n");
		dupACK++;

        // if (dupACK == 3)  { resend segment with sequence y; restart timer}
		if (dupACK == 3) {

            // printf("TRIPLE ACK\n");
            sendto (sock, pending_pkts[seq_ % W].pkt, pending_pkts[seq_ % W].size, 0, (struct sockaddr*)&remote, sizeof(remote));
            pending_pkts[seq_ % W].txTime = clock();

            // pending_pkts[seq_ % W].total_rtx++;
            // if (pending_pkts[seq_ % W].total_rtx >= 50)
            // {
            //     printf("MAX RTX REACHED\n");
            //     max_rtx = true;
            // }

            InterlockedAdd(&fast_rtxs, 1);
			dupRTX++;
            dupACK = 0;
		}
	}
    
     // in the worker thread
    // while (not end of transfer) {
    //     get ACK with sequence y, receiver window R
    //     if (y > sndBase) {
    //         sndBase = y
    //         effectiveWin = min (W, ack->window)

    //         // how much we can advance the semaphore
    //         newReleased = sndBase + effectiveWin - lastReleased
    //         ReleaseSemaphore (empty, newReleased)
    //         lastReleased += newReleased
    //     }
    // } 

}