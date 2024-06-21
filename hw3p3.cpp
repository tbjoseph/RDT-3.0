// hw3p3.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "SenderSocket.h"
#include "Checksum.h"

int main (int argc, char **argv)
{
    if (argc != 8) {
        printf("Usage: hw3pw1.exe [1] [2] [3] [4] [5] [6] [7]\n\n");
        printf("[1]\tdestination server\n");
        printf("[2]\tinput buffer of size power\n");
        printf("[3]\tsender window\n");
        printf("[4]\tround-trip propagation delay\n");
        printf("[5]\tprobability of loss\n");
        printf("[6]\tprobability of loss\n");
        printf("[7]\tspeed of the bottleneck link\n");
        return -1;
    }
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0)
    {
        printf("WSAStartup error %d\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    // rdt.exe s3.irl.cs.tamu.edu 24 50000 0.2 0.00001 0.0001 100

    // parse command-line parameters
    char *targetHost = argv[1];
    int power = atoi(argv[2]); // command-line specified integer
    int senderWindow = atoi (argv[3]); // command-line specified integer

    LinkProperties lp;
    lp.RTT = (float) atof (argv[4]);
    lp.speed = (float) ( 1e6 * atof (argv[7])) ; // convert to megabits
    lp.pLoss [FORWARD_PATH] = (float) atof (argv[5]);
    lp.pLoss [RETURN_PATH] = (float) atof (argv[6]);

    printf("Main:\t sender W = %d, RTT %.3f sec, loss %g / %g, link %s Mbps\n",
    senderWindow,
    lp.RTT,
    lp.pLoss[FORWARD_PATH],
    lp.pLoss[RETURN_PATH],
    argv[7]
    );


    printf("Main:\t initializing DWORD array with 2^%d elements... ", power);
    clock_t start = clock();

    UINT64 dwordBufSize = (UINT64) 1 << power;
    DWORD *dwordBuf = new DWORD [dwordBufSize]; // user-requested buffer
    for (UINT64 i = 0; i < dwordBufSize; i++) {// required initialization
        dwordBuf[i] = (DWORD)i;
    }

    clock_t end = clock();
    int elapsed_time = (int) ((double)(end - start) / CLOCKS_PER_SEC * 1000);
    printf("done in %d ms \n", elapsed_time);
    

    SenderSocket ss;
    int status;
    start = clock();
    if ((status = ss.Open (targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK) {
        // error handling: print status and quit
        printf("Main:\t connect failed with status %d\n", status);
        return -1;
    }


    
    end = clock();
    double elapsed_time2 = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Main:\t connected to %s in %.3f sec, pkt size %d bytes\n", targetHost, elapsed_time2, MAX_PKT_SIZE);

    start = clock();

    char *charBuf = (char*) dwordBuf; // this buffer goes into socket
    UINT64 byteBufferSize = dwordBufSize << 2; // convert to bytes
    UINT64 off = 0; // current position in buffer
    
    while (off < byteBufferSize)
    {
        // decide the size of next chunk
        int bytes = (int) min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
        // send chunk into socket
        if ((status = ss.Send (charBuf + off, bytes)) != STATUS_OK) {
            printf("Main:\t connect failed with status %d\n", status);
            return -1;
        }

        // error handing: print status and quit
        off += bytes;
    }
    
    end = clock();
    elapsed_time2 = (double)(end - start) / CLOCKS_PER_SEC;
    double estRTT = ss.get_estRTT();

    if ((status = ss.Close ()) != STATUS_OK) {
        // error handing: print status and quit
        printf("Main:\t close failed with status %d\n", status);
        return -1;
    }

    Checksum cs;
    DWORD check = cs.CRC32((unsigned char *)charBuf, byteBufferSize);
    double transfer_rate = 8 * byteBufferSize / elapsed_time2 * 1e-3;
    double ideal_rate = senderWindow * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) / estRTT * 1e-3;

    printf("Main:\t transfer finished in %.3f sec, %.2f Kbps, checksum %X\n", elapsed_time2, transfer_rate, check);
    printf("Main:\t estRTT %.3f, ideal rate %.2f Kbps\n", estRTT, ideal_rate);


    WSACleanup();
}