#include <assert.h>

#include "consumer.h"
#include "producer.h"

#include "channel.h"
#include "util.h"

static const int MAX_DELAY = 5;

struct Packet* ClonePacket(struct Packet* packet)
{
    assert(packet != NULL);
    assert(packet->buffer_ != NULL);
    struct Packet* clone = malloc(sizeof(struct Packet));
    memset(clone, 0, sizeof(struct Packet*));
    assert(clone != NULL);
    clone->buffer_ = malloc(packet->data_len_);
    assert(clone->buffer_ != NULL);
    memcpy(clone->buffer_, packet->buffer_, packet->data_len_);
    clone->data_len_ = packet->data_len_;
    clone->ts_ = packet->ts_;
    clone->delay_ = packet->delay_;
    clone->seq_number_ = packet->seq_number_;
    clone->state_ = packet->state_;
    clone->resend_count_ = packet->resend_count_;
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

    if (channel->bits_sent_per_second_ > channel->traffic_rate_) {
        printf("decreasing bitrate, bits sent: %d, current rate: %d\n", channel->bits_sent_per_second_, channel->traffic_rate_);
        channel->ready_ = FALSE;
        return NULL;
    }

    if (channel->packet_reasm_buf_ == NULL) {
        size_t size = sizeof(struct Packet)*channel->fragments_total_count_;
        channel->packet_reasm_buf_ = malloc(size);
        assert(channel->packet_reasm_buf_ != NULL);
        memset(channel->packet_reasm_buf_, 0, size);
    }

    assert(channel->fragments_total_count_ > seq_number);

    struct Packet* packet = &channel->packet_reasm_buf_[seq_number];

    if (packet->state_ == OTHER) {
        packet->data_len_ = len;
        packet->seq_number_ = seq_number;
        if (channel->enable_packet_delay_) {
            packet->delay_ = rand() % MAX_DELAY;
        } else {
            packet->delay_ = 0;
        }
        packet->ts_ = time(NULL);
        packet->buffer_ = malloc(packet->data_len_);
        assert(packet->buffer_ != NULL);
        memcpy(packet->buffer_, buffer, packet->data_len_);
        packet->next_ = NULL;
        packet->prev_ = NULL;
        packet->state_ = SENT;
        packet->resend_count_ = 0;

        if (channel->sent_head_ == NULL) {
            channel->sent_head_ = packet;
            channel->sent_tail_ = packet;
        } else {
            channel->sent_tail_->next_ = packet;
            packet->prev_ = channel->sent_tail_;
            channel->sent_tail_ = packet;
        }

        printf("%s: changing list structure for %d fragment\n", __FUNCTION__, seq_number);
    } else {
        printf("%s, attempt ot send a copy of %d fragment\n", __FUNCTION__, seq_number);
        packet->resend_count_++;
    }

    channel->bits_sent_per_second_ += len*8;

    return packet;
}

void FreePacket(struct Channel* channel, struct Packet* packet, enum DeliveryState state)
{
    if (channel == NULL) {
        printf("channel is NULL\n");
        return;
    }

    if (packet == NULL) {
        printf("packet is not valid\n");
        return;
    }

    assert(packet->seq_number_ < channel->fragments_total_count_);
    assert(&channel->packet_reasm_buf_[packet->seq_number_] == packet);

    assert(packet == channel->sent_head_);

    if (packet == channel->sent_head_) {
        channel->sent_head_ = packet->next_;
    }

    if (channel->sent_head_ == NULL) {
        channel->sent_tail_ = NULL;
    }

    free(packet->buffer_);
    packet->buffer_ = NULL;
    packet->state_ = state;
    packet->next_ = NULL;
    packet->prev_ = NULL;
    packet->resend_count_ = 0;
}

struct Packet* ConsumePacket(struct Channel* channel)
{
    if (channel == NULL) {
        printf("channel is NULL\n");
        return NULL;
    }

    if (channel->sent_head_ == NULL) {
        // printf("no packets to consume\n");
        return NULL;
    }

    struct Packet* packet = channel->sent_head_;

    if (channel->enable_packet_loss_) {
        BOOL need_to_drop = (channel->drop_count_++ < channel->drop_threshold_);
        if (channel->drop_count_ == 100) {
            channel->drop_count_ = 0;
        }

        if (need_to_drop) {
            printf("channel is not able to deliver the packet, dropping %d packet...\n", packet->seq_number_);
            FreePacket(channel, packet, OTHER);
            return NULL;
        }
    }

    time_t ts = time(NULL);
    time_t time_diff = ts - packet->ts_;
    if (time_diff < packet->delay_) {
        printf("%s: need to delay packet: time passed %ld, %d delay, packet: %d, ts: %ld\n", __FUNCTION__, time_diff, packet->delay_, packet->seq_number_, ts);
        return NULL;
    }

    struct Packet* clone = ClonePacket(packet);
    FreePacket(channel, packet, DELIVERED);

    return clone;
}

struct Channel* InitChannel(int packet_len, float packet_loss)
{
    int channel_size = sizeof(struct Channel);
    struct Channel* channel = malloc(channel_size);
    if (channel != NULL) {
        channel->ready_ = TRUE;
        channel->enable_packet_delay_ = TRUE;
        channel->enable_packet_loss_ = TRUE;
        channel->enable_random_rate_ = TRUE;
        channel->bits_sent_per_second_ = 0;
        channel->max_packet_len_ = packet_len;
        channel->ts_ = time(NULL);
        channel->traffic_rate_ = GetInstantRate(channel);
        channel->ack_handler_ = NULL;
        channel->seq_num_mismatch_handler_ = NULL;
        channel->sender_ = NULL;
        channel->receiver_ = NULL;
        channel->sent_head_ = NULL;
        channel->sent_tail_ = NULL;
        channel->drop_threshold_ = packet_loss * 100;
        channel->drop_count_ = 0;
        channel->fragments_total_count_ = 0;
        channel->packet_reasm_buf_ = NULL;
    }
    return channel;
}

void CloseChannel(struct Channel* channel)
{
    if (channel == NULL) {
        printf("channel is already closed");
        return;
    }

    if (channel->packet_reasm_buf_ != NULL) {
        for (int i = 0; i < channel->fragments_total_count_; i++) {
            if (channel->packet_reasm_buf_[i].buffer_ != NULL) {
                free(channel->packet_reasm_buf_[i].buffer_);
            }
        }
        free(channel->packet_reasm_buf_);
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