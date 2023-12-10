#include <assert.h>

#include "consumer.h"
#include "producer.h"

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
        packet->prev_ = NULL;
        packet->seq_number_ = 0;
        packet->ts_ = time(NULL);

        // BOOL need_delay = rand() % 3 == 0;
        // packet->delay_ = need_delay ? rand() % MAX_DELAY : 0;
        packet->delay_ = 0;
        
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
        channel->free_ = InitPacket(len);
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

    ResetPacket(packet, packet->len_);

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
    if (channel->packet_sent_ == seq_number) {
        channel->packet_sent_++;
        channel->bytes_sent_ += len;
    } else {
        printf("unexpected seq num: %d, expected: %d\n", seq_number, channel->packet_sent_);
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
    } else if (channel->sent_tail_ != NULL ) {
        printf("packet != channel->sent_\n");
        assert(0);
    }

    if (packet == channel->sent_) {
        channel->sent_ = NULL;
    }

    if (channel->sent_tail_ != NULL) {
        channel->sent_tail_->next_ = NULL;
    }

    ResetPacket(packet, packet->len_);

    packet->next_ = channel->free_;
    channel->free_ = packet;
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
    if (time(NULL) - packet->ts_ < packet->delay_) {
        // printf("need to delay packet\n");
        return NULL;
    }
#if 1
    int packets_dropped = channel->packet_sent_ - channel->packet_received_ ;
    float packet_loss_rate = (float)packets_dropped / channel->packet_sent_;

    if (packet_loss_rate <= channel->packet_loss_) {
        //we don't want to drop the packet one more time
        //that will lead to an infinite loop
        if (packet->seq_number_ > channel->packet_received_) {
            printf("channel is not able to deliver the packet, dropping %d packet...\n", packet->seq_number_);
            printf("packet loss %f with packets sent %d, dropped %d\n", packet_loss_rate, channel->packet_sent_, packets_dropped);
            FreePacket(channel, packet);
            return NULL;
        }
    }
#endif

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
        channel->max_packets_ = max_packets;
        channel->data_len_ = 0;
        channel->packet_sent_ = 0;
        channel->packet_received_ = 0;
        channel->bits_sent_per_second_ = 0;
        channel->bytes_sent_ = 0;
        channel->bytes_received_ = 0;
        channel->max_packet_len_ = packet_len;
        channel->ts_ = time(NULL);
        channel->packet_loss_ = packet_loss;
        channel->traffic_rate_ = GetInstantRate();
        channel->ack_handler_ = NULL;
        channel->nack_handler_ = NULL;
        channel->sender_ = NULL;
        channel->receiver_ = NULL;
        // channel->policy_ = NO_REALLOC;
        channel->policy_ = REALLOC;
        channel->sent_ = NULL;
        channel->sent_tail_ = NULL;
        for (int i = 0; i < max_packets; i++) {
            FreePacket(channel, InitPacket(channel->max_packet_len_));
        }
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

BOOL TryMakeChannelReady(struct Channel* channel)
{
    time_t current_ts = time(NULL); 
    if (current_ts - channel->ts_ >= 1) {
        channel->ts_ = current_ts;
        channel->bits_sent_per_second_ = 0;
        channel->traffic_rate_ = GetInstantRate();
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

int GetInstantRate()
{
    // return rand() % 100000000;
    return 100000000;
}

BOOL AllPacketsReceived(struct Channel* channel)
{
    if (channel == NULL) {
        return TRUE;
    }

    return ((channel->data_len_ == channel->bytes_sent_) && 
            (channel->bytes_sent_ == channel->bytes_received_));
}