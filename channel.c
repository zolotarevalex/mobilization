#include <assert.h>

#include "consumer.h"
#include "producer.h"

#include "channel.h"
#include "util.h"

static const int MAX_DELAY = 5;

struct Packet* InitPacket(int len, BOOL enable_delay)
{
    struct Packet* packet = malloc(sizeof(struct Packet) + len);
    ResetPacket(packet, len, enable_delay);
    return packet;
}

void ResetPacket(struct Packet* packet, int len, BOOL enable_delay)
{
    if (packet != NULL) {
        packet->next_ = NULL;
        packet->prev_ = NULL;
        packet->seq_number_ = 0;
        packet->ts_ = time(NULL);

        if (enable_delay){
            BOOL need_delay = rand() % 11 == 0;
            packet->delay_ = need_delay ? rand() % MAX_DELAY : 0;
        } else {
            packet->delay_ = 0;
        }
        
        packet->data_len_ = 0;
        packet->len_ = len;
        memset(packet->buffer_, 0, len);
    }
}

struct Packet* ClonePacket(struct Packet* packet)
{
    struct Packet* clone = InitPacket(packet->len_, packet->delay_);
    if (clone != NULL) {
        memcpy(clone->buffer_, packet->buffer_, packet->len_);
        clone->data_len_ = packet->data_len_;
        clone->ts_ = packet->ts_;
        clone->delay_ = packet->delay_;
        clone->seq_number_ = packet->seq_number_;
    }
    return clone;
}

struct Packet* AddPacket(struct Channel* channel, const char* buffer, int len, int seq_number)
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
        assert(0);
        return NULL;
    }

    TryMakeChannelReady(channel);

    if (channel->free_ == NULL) {
        if (channel->policy_ == NO_REALLOC) {
            printf("no space left for the packet\n");
            channel->ready_ = FALSE;
            return NULL;
        }

        printf("allocating new buffer\n");
        channel->free_ = InitPacket(len, channel->enable_packet_delay_);
    }

    if (channel->bits_sent_per_second_ > channel->traffic_rate_) {
        printf("decreasing bitrate, bits sent: %d, current rate: %d\n", channel->bits_sent_per_second_, channel->traffic_rate_);
        channel->ready_ = FALSE;
        return NULL;
    }

    channel->bits_sent_per_second_ += len*8;

    struct Packet* packet = channel->free_;
    struct Packet* free_head = packet->next_;
    channel->free_ = free_head;

    ResetPacket(packet, packet->len_, channel->enable_packet_delay_);

    if (channel->sent_ != NULL) {
        packet->next_ = channel->sent_;
        channel->sent_->prev_ = packet;      
    } else {
        channel->sent_tail_ = packet;
    }

    channel->sent_ = packet;

    memcpy(packet->buffer_, buffer, min(len, packet->len_));
    
    packet->data_len_ = len;
    packet->seq_number_ = seq_number;

    //we don't want to take ito account packets being re-sent
    if (channel->packet_sent_ >= seq_number) {
        channel->packet_sent_++;
    } else {
        printf("%s: unexpected seq num, expected %d, received %d\n", __FUNCTION__, channel->packet_sent_, seq_number);
    }

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

    if (packet == channel->sent_tail_) {
        channel->sent_tail_ = packet->prev_;
    }

    if (packet == channel->sent_) {
        channel->sent_ = NULL;
    }

    if (channel->sent_tail_ != NULL) {
        channel->sent_tail_->next_ = NULL;
    }

    ResetPacket(packet, packet->len_, channel->enable_packet_delay_);

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

    if (channel->sent_tail_ == NULL) {
        // printf("no packets to consume\n");
        return NULL;
    }

    struct Packet* packet = channel->sent_tail_;
    time_t time_diff = time(NULL) - packet->ts_;
    if (time_diff < packet->delay_) {
        printf("%s: need to delay packet: time passed %ld, %d delay, packet: %d\n", __FUNCTION__, time_diff, packet->delay_, packet->seq_number_);
        return NULL;
    }

    if (channel->enable_packet_loss_) {
        int packets_dropped = channel->packet_sent_ - channel->packet_received_ ;
        float packet_loss_rate = (float)packets_dropped / channel->packet_sent_;

        if (packet_loss_rate <= channel->packet_loss_) {
            //we don't want to drop the packet one more time
            //that will lead to an infinite loop
            static int last_dropped = -1;
            if (packet->seq_number_ > last_dropped) {
                printf("channel is not able to deliver the packet, dropping %d packet...\n", packet->seq_number_);
                printf("packet loss %f with packets sent %d, dropped %d\n", packet_loss_rate, channel->packet_sent_, packets_dropped);
                last_dropped = packet->seq_number_;
                FreePacket(channel, packet);
                return NULL;
            }
        }
    }

    channel->packet_received_++;
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
        channel->enable_packet_delay_ = TRUE;
        channel->enable_packet_loss_ = TRUE;
        channel->enable_random_rate_ = TRUE;
        channel->max_packets_ = max_packets;
        channel->data_len_ = 0;
        channel->packet_sent_ = 0;
        channel->packet_received_ = 0;
        channel->bits_sent_per_second_ = 0;
        channel->bytes_received_ = 0;
        channel->max_packet_len_ = packet_len;
        channel->ts_ = time(NULL);
        channel->packet_loss_ = packet_loss;
        channel->traffic_rate_ = GetInstantRate(channel);
        channel->ack_handler_ = NULL;
        channel->nack_handler_ = NULL;
        channel->sender_ = NULL;
        channel->receiver_ = NULL;
        // channel->policy_ = NO_REALLOC;
        channel->policy_ = REALLOC;
        channel->sent_ = NULL;
        channel->sent_tail_ = NULL;
        for (int i = 0; i < max_packets; i++) {
            FreePacket(channel, InitPacket(channel->max_packet_len_, channel->enable_packet_delay_));
        }
    }
    return channel;
}

void ResetChannel(struct Channel* channel)
{
    struct Packet* packet = channel->sent_;
    while (packet) {
        struct Packet* next = packet->next_;
        FreePacket(channel, packet);
        packet = next;
    }
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

BOOL TryMakeChannelReady(struct Channel* channel)
{
    time_t current_ts = time(NULL); 
    if (current_ts - channel->ts_ >= 1) {
        channel->ts_ = current_ts;
        channel->bits_sent_per_second_ = 0;
        channel->traffic_rate_ = GetInstantRate(channel);
        channel->ready_ = TRUE;
    } 
    return IsChannelReady(channel);
}

BOOL IsChannelReady(struct Channel* channel)
{
    if (channel == NULL) {
        return FALSE;
    }

    return channel->ready_;
}

int GetInstantRate(struct Channel* channel)
{
    static const int rate_base = 100000000;
    if (channel->enable_random_rate_) {
        return rand() % rate_base;
    }

    return rate_base;
}