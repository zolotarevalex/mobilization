#include "producer.h"
#include "util.h"

#include <assert.h>


static const int MAX_WAIT_TIME = 0;
static const int FRAGMENT_ACK_BASE = 100;

struct Producer* InitProducer(struct Channel* channel, const char* file_name)
{
    if (channel == NULL) {
        printf("channel is not valid\n");
        return NULL;
    }

    if (file_name == NULL) {
        printf("file name is not valid\n");
        return NULL;
    }

    struct Producer* producer = malloc(sizeof(struct Producer) + channel->max_packet_len_);
    if (producer == NULL) {
        printf("failed to allocate producer\n");
        return NULL;
    }

    producer->buf_len_ = channel->max_packet_len_;
    producer->fragment_len_ = channel->max_packet_len_;

    memset(producer->buf_, 0, producer->buf_len_);

    producer->channel_ = channel;
    producer->file_ = fopen(file_name, "rb");
    if (producer->file_ == NULL) {
        printf("failed to opent %s file\n", file_name);
        free(producer);
        return NULL;
    }

    fseek(producer->file_, 0, SEEK_END);
    producer->total_bytes_ = ftell(producer->file_); 
    producer->total_fragment_number_ = producer->total_bytes_ / producer->fragment_len_;
    if (producer->total_bytes_ % producer->fragment_len_ != 0) {
        producer->total_fragment_number_++;
    }

    producer->fragments_ = malloc(sizeof(struct ProducerFragment)*producer->total_fragment_number_);
    assert(producer->fragments_ != NULL);
    memset(producer->fragments_, 0, sizeof(struct ProducerFragment)*producer->total_fragment_number_);
    producer->fragments_head_ = &producer->fragments_[0];
    producer->fragments_tail_ = producer->fragments_head_;
    for (int i = 1; i < producer->total_fragment_number_; i++) {
        struct ProducerFragment* fragment = &producer->fragments_[i];
        fragment->seq_number_ = i;
        fragment->prev_ = producer->fragments_tail_;
        producer->fragments_tail_->next_ = fragment;
        producer->fragments_tail_ = fragment;
    }

    assert(producer->fragments_head_->next_ == &producer->fragments_[1]);

    producer->fragment_to_be_sent_ = NULL;
    producer->fragments_delivered_ = 0;

    printf("%s bytes_left %d to read file %s\n", __FUNCTION__, producer->total_bytes_, file_name);

    rewind(producer->file_);

    channel->sender_ = producer;
    channel->ack_handler_ = HandleFragmentAck;
    channel->seq_num_mismatch_handler_ = HandleFragmentSeqNumberMismatch;
    channel->fragments_total_count_ = producer->total_fragment_number_;

    return producer;
}

void CloseProducer(struct Producer* producer)
{
    if (producer == NULL) {
        printf("producer is already closed\n");
        return;
    }

    if (producer->file_) {
        fclose(producer->file_);
    }

    free(producer->fragments_);
}

int GetFragmentOffset(struct Producer* producer, int seq_number)
{
    if (producer == NULL) {
        printf("%s, producer is not valid\n", __FUNCTION__);
        return -1;
    }
    return producer->fragment_len_ * seq_number;
}

void HandleFragmentAck(struct Channel* channel, int fragment_seq_number)
{
    if (channel == NULL) {
        printf("%s: channel is not valid\n", __FUNCTION__);
        return;
    }

    if (channel->sender_ == NULL) {
        printf("%s: producer is not valid\n", __FUNCTION__);
        return;
    }

    assert(fragment_seq_number < channel->sender_->total_fragment_number_);

    HandleFragmentDelivered(channel->sender_, &channel->sender_->fragments_[fragment_seq_number]);
}

void HandleFragmentSeqNumberMismatch(struct Channel* channel, int expected, int received)
{
    if (channel == NULL) {
        printf("%s: channel is not valid\n", __FUNCTION__);
        return;
    }

    if (channel->sender_ == NULL) {
        printf("%s: producer is not valid\n", __FUNCTION__);
        return;
    }
    
    assert(expected <= received);
    assert(received < channel->sender_->total_fragment_number_);
    
    HandleFragmentDelivered(channel->sender_, &channel->sender_->fragments_[received]);
}

void HandleFragmentDelivered(struct Producer* producer, struct ProducerFragment* fragment)
{
    assert(producer != NULL);
    assert(fragment != NULL);

    if (fragment->delivered_ == FALSE) {
        if (fragment == producer->fragments_head_) {
            producer->fragments_head_ = fragment->next_;
        }

        if (fragment == producer->fragments_tail_) {
            producer->fragments_tail_ = fragment->prev_;
        }

        if (fragment->prev_ != NULL) {
            fragment->prev_->next_ = fragment->next_;
        }

        if (fragment->next_ != NULL) {
            fragment->next_->prev_ = fragment->prev_;
        }

        fragment->next_ = NULL;
        fragment->prev_ = NULL;

        producer->fragments_delivered_++; 
        printf("%s: fragments delivered %d\n", __FUNCTION__, producer->fragments_delivered_);       
    }
}

BOOL HasFragmentsToResend(struct Producer* producer)
{
    if (producer == NULL) {
        printf("%s: producer is not valid\n", __FUNCTION__);
        return FALSE;
    }

    return producer->fragments_head_ != NULL;
}

void ResendFragment(struct Producer* producer)
{
    assert(producer->fragments_head_ != NULL);
    assert(producer->fragments_tail_ != NULL);

    if (producer->fragment_to_be_sent_ == NULL) {
        producer->fragment_to_be_sent_ = producer->fragments_head_;
    }

    assert(producer->fragment_to_be_sent_ != NULL);

    int offset = GetFragmentOffset(producer, producer->fragment_to_be_sent_->seq_number_);
    if (offset < 0) {
        printf("%s: invalid fragment offset\n", __FUNCTION__);
        return;
    }

    int current_pos = ftell(producer->file_);

    fseek(producer->file_, offset, SEEK_SET);
    int len = fread(producer->buf_, 1, producer->buf_len_, producer->file_);

    if (AddPacket(producer->channel_, producer->buf_, len, producer->fragment_to_be_sent_->seq_number_)) {
        printf("%s: sending %d fragment once again\n", __FUNCTION__, producer->fragment_to_be_sent_->seq_number_);
    }

    fseek(producer->file_, current_pos, SEEK_SET);
    
    producer->fragment_to_be_sent_ = producer->fragment_to_be_sent_->next_;
}

void SendFileFragment(struct Producer* producer)
{
    if (producer == NULL) {
        printf("producer is not valid\n");
        return;
    }

    if (producer->file_ == NULL) {
        printf("file is not opened\n");
        return;
    }
    
    if (TryMakeChannelReady(producer->channel_)) {
        if (HasFragmentsToResend(producer)) {
            ResendFragment(producer);
        } 
    } else {
        // printf("%s: channel is not ready\n", __FUNCTION__);
    }
}

BOOL AllPacketsReceived(struct Producer* producer)
{
    if (producer == NULL) {
        printf("%s: producer is not valid\n", __FUNCTION__);
        return FALSE;
    }

    return (producer->total_fragment_number_ == producer->fragments_delivered_);
}