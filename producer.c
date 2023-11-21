#include "producer.h"

struct Producer* InitProducer(struct Channel* channel)
{
    struct Producer* producer = malloc(sizeof(struct Producer));

    if (producer != NULL) {
        producer->channel_ = channel;
    }

    return producer;
}

void CloseProducer(struct Producer* producer)
{
    if (producer == NULL) {
        printf("producer is already closed\n");
        return;
    }
    free(producer);
}

struct Packet* SendPacket(struct Producer* producer, const char* buffer, int len)
{
    if (producer == NULL) {
        printf("producer in not valid\n");
        return NULL;
    }

    if (IsChannelReady(producer->channel_)) {
        return AddPacket(producer->channel_, buffer, len);
    }
    return NULL;
}