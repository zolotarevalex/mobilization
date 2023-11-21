#include "consumer.h"

struct Consumer* InitConsumer(struct Channel* channel)
{
    struct Consumer* consumer = malloc(sizeof(struct Consumer));

    if (consumer != NULL) {
        consumer->channel_ = channel;
    }

    return consumer;
}

void CloseConsumer(struct Consumer* consumer)
{
    if (consumer == NULL) {
        printf("consumer is already closed\n");
        return;
    }
    free(consumer);
}

struct Packet* ReceivePacket(struct Consumer* receiver)
{
    if (receiver == NULL) {
        printf("consumer in not valid\n");
        return NULL;
    }

    return ConsumePacket(receiver->channel_);
}