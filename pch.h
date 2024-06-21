// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel
#define MAGIC_PROTOCOL 0x8311AA 

#define FORWARD_PATH 0
#define RETURN_PATH 1 

#define ALPHA (1.0/8) /* recursion available */ 
#define BETA (1.0/4) /* recursion available */ 
#define MAX_PKT_SIZE (1500-28) // maximum UDP packet size accepted by receiver
#define MAX_RTX 1e9

enum handleEnum { SENDER_BASE, ACKED_DATA, NEXT_SEQ, TIMEOUTS, FAST_RTXS, EFFECTIVE_WINDOW, GOODPUT, EST_RTT };

#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <string>



#pragma pack(push,1)

struct Stats {
    // LONG volatile sender_base;
    LONG volatile acked_data;
    // LONG volatile next_seq;
    LONG volatile timeouts;
    LONG volatile fast_rtxs;
    // LONG volatile effective_window;
    LONG volatile goodput;
    LONG volatile est_rtt;
    Stats () { memset(this, 0, sizeof(*this)); }
};

struct LinkProperties {
    // transfer parameters
    float RTT; // propagation RTT (in sec)
    float speed; // bottleneck bandwidth (in bits/sec)
    float pLoss [2]; // probability of loss in each direction
    DWORD bufferSize; // buffer size of emulated routers (in packets)
    LinkProperties () { memset(this, 0, sizeof(*this)); }
}; 

struct Flags {
    DWORD reserved:5; // must be zero
    DWORD SYN:1;
    DWORD ACK:1;
    DWORD FIN:1;
    DWORD magic:24;
    Flags () { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};

struct SenderDataHeader {
    Flags flags;
    DWORD seq; // must begin from 0
};

struct SenderSynHeader {
    SenderDataHeader sdh;
    LinkProperties lp;
};

struct ReceiverHeader {
    Flags flags;
    DWORD recvWnd; // receiver window for flow control (in pkts)
    DWORD ackSeq; // ack value = next expected sequence
};

struct Packet {
    int total_rtx; // SYN, FIN, data
    int size; // bytes in packet data
    clock_t txTime; // transmission time
    char pkt[MAX_PKT_SIZE]; // packet with header
};

#pragma pack(pop) 

struct SenderSocketInfo {
    char flags[4];
    int rto; // receiver window for flow control (in pkts)
    DWORD ackSeq; // ack value = next expected sequence
};

#endif //PCH_H
