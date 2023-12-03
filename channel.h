#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef int BOOL;

#define TRUE 1
#define FALSE 0

struct Packet
{
    struct Packet* next_;
    time_t ts_;
    int delay_;
    int len_;
    char buffer_[];
};

enum AllocPolicy
{
    REALLOC,
    NO_REALLOC
};

struct Channel
{
    BOOL ready_;
    int max_packets_;
    int packet_count_;
    int packet_sent_;
    int bytes_sent_;
    int max_packet_len_;
    time_t ts_;
    float packet_loss_;
    enum AllocPolicy policy_;
    struct Packet* free_;
    struct Packet* sent_;
};

struct Packet* InitPacket(int len);
void ResetPacket(struct Packet* packet, int len);
struct Packet* ClonePacket(struct Packet* packet);
struct Packet* AddPacket(struct Channel* channel, const char* buffer, int len);
void FreePacket(struct Channel* channel, struct Packet* packet);
struct Packet* ConsumePacket(struct Channel* channel);
struct Channel* InitChannel(int max_packets, int packet_len, float packet_loss);
void CloseChannel(struct Channel* channel);
BOOL IsChannelReady(struct Channel* channel);
int GetInstantRate();
BOOL AllPacketsReceived(struct Channel* channel);

#endif