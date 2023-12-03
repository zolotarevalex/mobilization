#ifndef PRODUCER_H_
#define PRODUCER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "channel.h"

struct Producer
{
    struct Channel* channel_;
    FILE* file_;
    char* buf_;
    int bytes_used_;
    int bytes_left_;
};

struct Producer* InitProducer(struct Channel* channel, const char* file_name);
void CloseProducer(struct Producer* producer);
struct Packet* SendPacket(struct Producer* producer, const char* buffer, int len);
int SendFile(struct Producer* producer);

#endif