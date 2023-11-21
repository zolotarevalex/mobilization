#ifndef PRODUCER_H_
#define PRODUCER_H_

#include "channel.h"

struct Producer
{
    struct Channel* channel_;
};

struct Producer* InitProducer(struct Channel* channel);
void CloseProducer(struct Producer* producer);
struct Packet* SendPacket(struct Producer* producer, const char* buffer, int len);

#endif