#include <assert.h>

#include "channel.h"
#include "util.h"

static const int MAX_DELAY = 5;

struct Packet* InitPacket(int len)
{
    struct Packet* packet = malloc(sizeof(struct Packet) + len);
    ResetPacket(packet, len);
    return packet;
}

void ResetPacket(struct Packet* packet, int len)
{
    if (packet != NULL) {
        packet->next_ = NULL;
        packet->ts_ = time(NULL);

        BOOL need_delay = rand() % 3 == 0;
        packet->delay_ = need_delay ? rand() % MAX_DELAY : 0;
        
        packet->data_len_ = 0;
        packet->len_ = len;
        memset(packet->buffer_, 0, len);
    }
}

struct Packet* ClonePacket(struct Packet* packet)
{
    struct Packet* clone = InitPacket(packet->len_);
    if (clone != NULL) {
        memcpy(clone->buffer_, packet->buffer_, packet->len_);
        clone->data_len_ = packet->data_len_;
        clone->ts_ = packet->ts_;
        clone->delay_ = packet->delay_;
    }
    return clone;
}

struct Packet* AddPacket(struct Channel* channel, const char* buffer, int len)
{
    if (channel == NULL) {
        printf("channel is NULL\n");
        return NULL;
    }

    if (buffer == NULL) {
        printf("packet is not valid\n");
        return NULL;
    }

    if (len == 0) {
        printf("packet len is not valid\n");
        return NULL;
    }

    channel->packet_count_++;

    time_t current_ts = time(NULL); 
    if (current_ts - channel->ts_ >= 1) {
        channel->ts_ = current_ts;
        channel->bytes_sent_ = 0;
    }

    if (channel->free_ == NULL) {
        if (channel->policy_ == NO_REALLOC) {
            printf("no space left for the packet\n");
            channel->ready_ = FALSE;
            return NULL;
        }

        printf("allocating new buffer\n");
        channel->free_ = InitPacket(len);
    }

    BOOL can_be_dropped = ((rand() % 100 + 1) <= (100 * channel->packet_loss_));

    if (can_be_dropped) {
        int packets_dropped = channel->packet_count_ - channel->packet_sent_;
        float packet_loss_rate = (float)packets_dropped / channel->packet_count_;

        if (packet_loss_rate <= channel->packet_loss_) {
            printf("channel is not able to deliver the packet, dropping...\n");
            printf("packet loss %f\n", packet_loss_rate);
            return NULL;
        }
    }

    int rate = GetInstantRate();
    if (channel->bytes_sent_ > rate) {
        printf("decreasing bitrate, bytes sent: %d, current rate: %d\n", channel->bytes_sent_, rate);
        return NULL;
    }

    struct Packet* packet = channel->free_;
    struct Packet* free_head = packet->next_;
    channel->free_ = free_head;

    ResetPacket(packet, packet->len_);

    if (channel->sent_ != NULL) {
        packet->next_ = channel->sent_;
    }

    channel->sent_ = packet;

    memcpy(packet->buffer_, buffer, min(len, packet->len_));
    
    packet->data_len_ = len;

    channel->packet_sent_++;
    channel->bytes_sent_ += len;

    return channel->sent_;
}

void FreePacket(struct Channel* channel, struct Packet* packet)
{
    if (channel == NULL) {
        printf("channel is NULL\n");
        return;
    }

    if (packet == NULL) {
        printf("packet is not valid\n");
        return;
    }

    if (packet == channel->sent_) {
        channel->sent_ = packet->next_;
    } else if (channel->sent_ != NULL ) {
        printf("packet != channel->sent_\n");
        assert(0);
        // struct Packet* current_alloc_node = channel->sent_;
        // struct Packet* prev_alloc_node = NULL;
        // while(current_alloc_node != NULL) {
        //     if (current_alloc_node == packet) {
        //         prev_alloc_node->next_ = current_alloc_node->next_;
        //         break;
        //     }

        //     prev_alloc_node = current_alloc_node; 
        //     current_alloc_node = current_alloc_node->next_;
        // }
    }

    ResetPacket(packet, packet->len_);

    packet->next_ = channel->free_;
    channel->free_ = packet;  
    channel->ready_ = TRUE;
}

struct Packet* ConsumePacket(struct Channel* channel)
{
    if (channel == NULL) {
        printf("channel is NULL\n");
        return NULL;
    }

    if (channel->sent_ == NULL) {
        printf("no packets to consume\n");
        return NULL;
    }

    struct Packet* packet = channel->sent_;
    if (time(NULL) - packet->ts_ < packet->delay_) {
        // printf("need to delay packet\n");
        return NULL;
    }

    channel->bytes_received_ += packet->data_len_;

    struct Packet* clone = ClonePacket(packet);
    FreePacket(channel, packet);

    return clone;
}

struct Channel* InitChannel(int max_packets, int packet_len, float packet_loss)
{
    int channel_size = sizeof(struct Channel);
    struct Channel* channel = malloc(channel_size);
    if (channel != NULL) {
        channel->ready_ = TRUE;
        channel->max_packets_ = max_packets;
        channel->data_len_ = 0;
        channel->packet_count_ = 0;
        channel->packet_sent_ = 0;
        channel->bytes_sent_ = 0;
        channel->bytes_received_ = 0;
        channel->max_packet_len_ = packet_len;
        channel->ts_ = time(NULL);
        channel->packet_loss_ = packet_loss;
        channel->policy_ = NO_REALLOC;
        //channel->policy_ = REALLOC;
        channel->sent_ = NULL;
        for (int i = 0; i < max_packets; i++) {
            FreePacket(channel, InitPacket(channel->max_packet_len_));
        }
        printf("channel size: %d\n", channel_size);
    }
    return channel;
}

void CloseChannel(struct Channel* channel)
{
    if (channel == NULL) {
        printf("channel is already closed");
        return;
    }

    while(channel->free_) {
        struct Packet* packet = channel->free_;
        channel->free_ = packet->next_;
        free(packet);
    }

    while(channel->sent_) {
        struct Packet* packet = channel->sent_;
        channel->sent_ = packet->next_;
        free(packet);
    }

    free(channel);
}

BOOL IsChannelReady(struct Channel* channel)
{
    if (channel == NULL) {
        return FALSE;
    }

    return channel->ready_;
}

int GetInstantRate()
{
    return rand() % 101 * 1000000;
}

BOOL AllPacketsReceived(struct Channel* channel)
{
    if (channel == NULL) {
        return TRUE;
    }

    return ((channel->data_len_ == channel->bytes_sent_) && 
            (channel->bytes_sent_ == channel->bytes_received_));
}