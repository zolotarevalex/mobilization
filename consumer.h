#ifndef CONSUMER_H_
#define CONSUMER_H_

#include "channel.h"

struct Consumer
{
    struct Channel* channel_;
};

struct Consumer* InitConsumer(struct Channel* channel);
void CloseConsumer(struct Consumer* consumer);
struct Packet* ReceivePacket(struct Consumer* receiver);

#endif