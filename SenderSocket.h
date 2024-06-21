#pragma once

#ifndef SENDER_SOCKET_H
#define SENDER_SOCKET_H

#include "pch.h"

#define MAGIC_PORT 22345 // receiver listens on this port



class SenderSocket {
    // TCP vars
    SOCKET sock;
    struct sockaddr_in remote;
    char packet[MAX_PKT_SIZE];
    volatile DWORD seq;
    volatile DWORD sndBase;
    volatile DWORD nextToSend;
    volatile DWORD lastReleased;
    char *targetIP;
    clock_t start;
    double timerExpire;
    double rto;
    double estRTT;
    double devRTT;
    DWORD dupACK;
    DWORD dupRTX;
    bool max_rtx;

    
    DWORD W;
    DWORD openrecvWnd;
    DWORD recvWnd;
    Packet* pending_pkts;;
    DWORD closeACK;

    // Stats vars
    volatile LONG total_bytes;
    volatile LONG timeouts;
    volatile LONG fast_rtxs;
    volatile DWORD effective_window_size;
    volatile double estRTT_reader;
    
    HANDLE  statsHandle;
    HANDLE  workerHandle;
	HANDLE	eventQuit;
	HANDLE	socketReceiveReady;
	HANDLE	empty;
	HANDLE	full;

    // helper functions
    int Write(int attempts, struct timeval timeout, int packet_size);
    void quitStatsRun();
    void ReceiveACK(bool &got_fin);

public:
    SenderSocket() : sock(-1), statsHandle(NULL) {
        start = clock();
        eventQuit = CreateEvent(NULL, true, false, NULL);
        socketReceiveReady = CreateEvent(NULL, false, false, NULL);
    };

    ~SenderSocket();
    int Open(char *targetHost, int port, int senderWindow, LinkProperties *linkProperties);
    int Close();
    int Send(char* buf, int bytes);
    double get_estRTT() { return estRTT_reader; };

    void StatsRun();
    void WorkerRun();
    
};

#endif //SENDER_SOCKET_H