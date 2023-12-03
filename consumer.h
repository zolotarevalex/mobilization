#ifndef CONSUMER_H_
#define CONSUMER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "channel.h"

struct Consumer
{
    struct Channel* channel_;
    FILE* file_; 
};

struct Consumer* InitConsumer(struct Channel* channel, const char* file_name);
void CloseConsumer(struct Consumer* consumer);
struct Packet* ReceivePacket(struct Consumer* receiver);
int ReceiveFile(struct Consumer* receiver);


#endif