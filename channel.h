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

    //timespamt of packet creation
    time_t ts_;

    //to simulate traffic delay
    int delay_;

    //actual data size
    int data_len_;

    //buffer size
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

    //total size of data to be transfered
    int data_len_;

    //total packets sent count (including drops)
    int packet_count_;

    //packets sent
    int packet_sent_;

    //bytes sent
    int bytes_sent_;

    //bytes received by the consumer
    int bytes_received_;

    //buffer size to store single packet
    int max_packet_len_;

    //timestamp to measure rate (updeted each second)
    time_t ts_;

    //expected packet loss rate
    float packet_loss_;

    //allocation policy. defines behavior behavoir of the channel
    //for the case when there is no beuffer left to store new packet 
    enum AllocPolicy policy_;

    //number of available buffers to store packets
    int max_packets_;

    //list of free buffers ready to store new packet
    struct Packet* free_;

    //list of buffers with packets being sent
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