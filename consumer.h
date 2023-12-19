#ifndef CONSUMER_H_
#define CONSUMER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "channel.h"

struct Fragment 
{
    int seq_number_;
    int len_;
    char* buf_;
};

struct Consumer
{
    struct Channel* channel_;
    FILE* file_; 
    int next_seq_number_;
    int reasm_buf_len_;
    struct Fragment* reasm_buf_;
};
struct Consumer* InitConsumer(struct Channel* channel, const char* file_name);
void CloseConsumer(struct Consumer* consumer);
struct Packet* ReceivePacket(struct Consumer* receiver);
int ReceiveFileFragment(struct Consumer* receiver);
void FlushReasmBuf(struct Consumer* receiver);


#endif