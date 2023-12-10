#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef int BOOL;

#define TRUE 1
#define FALSE 0

struct Consumer;
struct Producer;


// typedef void (*ack_handler)(int) FrameAckHandler;
// typedef void (*nack_handler)(int) FrameNAckHandler;

struct Packet
{
    struct Packet* next_;
    struct Packet* prev_;

    //packet sequence number
    int seq_number_;

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

    //packets sent
    int packet_sent_;

    //packets received
    int packet_received_;

    //bytes sent per second, used to measure instant rate
    int bits_sent_per_second_;

    //total amount of bytes sent
    int bytes_sent_;

    //bytes received by the consumer
    int bytes_received_;

    //buffer size to store single packet
    int max_packet_len_;

    //timestamp to measure rate (updeted each second)
    time_t ts_;

    //expected packet loss rate
    float packet_loss_;

    //bits per sec for simulation, calculated each second
    int traffic_rate_;

    //allocation policy. defines behavior behavoir of the channel
    //for the case when there is no beuffer left to store new packet 
    enum AllocPolicy policy_;

    //number of available buffers to store packets
    int max_packets_;

    //handler to notify producer about frame delivery
    void (*ack_handler_)(struct Channel*,int);

    //handler to notify producer about lost frame
    void (*nack_handler_)(struct Channel*,int);

    //
    struct Producer* sender_;

    //
    struct Consumer* receiver_; 

    //list of free buffers ready to store new packet
    struct Packet* free_;

    //list of buffers with packets being sent
    struct Packet* sent_;

    struct Packet* sent_tail_;
};

struct Packet* InitPacket(int len);
void ResetPacket(struct Packet* packet, int len);
struct Packet* ClonePacket(struct Packet* packet);
struct Packet* AddPacket(struct Channel* channel, const char* buffer, int len, int seq_number);
void FreePacket(struct Channel* channel, struct Packet* packet);
struct Packet* ConsumePacket(struct Channel* channel);
struct Channel* InitChannel(int max_packets, int packet_len, float packet_loss);
void CloseChannel(struct Channel* channel);
BOOL IsChannelReady(struct Channel* channel);
BOOL TryMakeChannelReady(struct Channel* channel);
int GetInstantRate();
BOOL AllPacketsReceived(struct Channel* channel);

#endif