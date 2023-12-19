#include "producer.h"
#include "util.h"

#include <assert.h>


static const int MAX_WAIT_TIME = 1;
static const int FRAGMENT_ACK_BASE = 10;

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
    producer->bytes_left_ = producer->total_bytes_;
    producer->total_fragment_number_ = producer->bytes_left_ / producer->fragment_len_;
    if (producer->bytes_left_ % producer->fragment_len_ != 0) {
        producer->total_fragment_number_++;
    }
    producer->fragments_delivered_ = 0;

    producer->fragment_map_buf_len_ = producer->total_fragment_number_ / 8;
    if (producer->total_fragment_number_ % 8 > 0) {
        int padding = 8 - producer->total_fragment_number_ % 8;
        producer->fragment_map_buf_len_  += padding;
    }

    producer->fragments_map_ = malloc(producer->fragment_map_buf_len_ );
    assert(producer->fragments_map_ != NULL);
    memset(producer->fragments_map_, 0, producer->fragment_map_buf_len_ );

    printf("%s bytes_left %d to read file %s\n", __FUNCTION__, producer->bytes_left_, file_name);

    rewind(producer->file_);

    producer->fragment_seq_number_ = 0;

    channel->sender_ = producer;
    channel->ack_handler_ = HandleFragmentAck;
    channel->seq_num_mismatch_handler_ = HandleFragmentSeqNumberMismatch;

    producer->resend_seq_start_ = -1;
    producer->resend_seq_end_ = -1;
    producer->last_retry_ts_ = time(NULL);
    producer->last_fragment_ack_seq_num_ = 0;

    producer->fragmen_ack_counter_map_size_ = producer->total_fragment_number_ / FRAGMENT_ACK_BASE + 1;
    producer->fragmen_ack_counter_map_ = malloc(sizeof(short)*producer->fragmen_ack_counter_map_size_);
    memset(producer->fragmen_ack_counter_map_, 0, sizeof(short)*producer->fragmen_ack_counter_map_size_);

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

    free(producer->fragments_map_);
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

    if (SetFlag(channel->sender_->fragments_map_, channel->sender_->total_fragment_number_, fragment_seq_number)) {
        channel->sender_->fragments_delivered_++;
        channel->sender_->fragmen_ack_counter_map_[fragment_seq_number/FRAGMENT_ACK_BASE]++;
        // printf("%s: ack from %d fragment\n", __FUNCTION__, fragment_seq_number);
    } else {
        printf("%s: duplication ack from %d fragment\n", __FUNCTION__, fragment_seq_number);
        // assert(0);
    }

    if (channel->sender_->resend_seq_start_ == fragment_seq_number) {
        if (channel->sender_->resend_seq_end_ > channel->sender_->resend_seq_start_) {
            channel->sender_->resend_seq_start_++;
        } else {
            channel->sender_->resend_seq_start_ = -1;
            channel->sender_->resend_seq_end_ = -1;
        }
    }

    assert(channel->sender_->resend_seq_start_ < channel->sender_->total_fragment_number_);
    
    channel->sender_->last_fragment_ack_seq_num_ = fragment_seq_number;
    channel->sender_->last_retry_ts_ = time(NULL);
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
    channel->sender_->resend_seq_start_ = expected;
    channel->sender_->resend_seq_end_ = expected;

    assert(channel->sender_->resend_seq_start_ < channel->sender_->total_fragment_number_);

    if (SetFlag(channel->sender_->fragments_map_, channel->sender_->total_fragment_number_, received)) {
        channel->sender_->fragments_delivered_++;
        channel->sender_->fragmen_ack_counter_map_[received/FRAGMENT_ACK_BASE]++;
        channel->sender_->last_retry_ts_ = time(NULL);
    }
}

BOOL HasFragmentsToResend(struct Producer* producer)
{
    if (producer == NULL) {
        printf("%s: producer is not valid\n", __FUNCTION__);
        return FALSE;
    }

    return producer->resend_seq_start_ >= 0;
}

void ResendFragment(struct Producer* producer)
{
    BOOL rc = FALSE;

    int offset = GetFragmentOffset(producer, producer->resend_seq_start_);
    if (offset < 0) {
        printf("%s: invalid fragment offset\n", __FUNCTION__);
        return;
    }

    if (producer->fragmen_ack_counter_map_[producer->resend_seq_start_/FRAGMENT_ACK_BASE] >= FRAGMENT_ACK_BASE) {
        int range_start = FRAGMENT_ACK_BASE * (producer->resend_seq_start_/FRAGMENT_ACK_BASE);
        int range_end = range_start + FRAGMENT_ACK_BASE;
        printf("%s: all fragments in range %d-%d are already delivered, ignoring range\n", __FUNCTION__, range_start, range_end);
        producer->resend_seq_start_ = range_end;
    } else 
    if (GetFlag(producer->fragments_map_, producer->total_fragment_number_, producer->resend_seq_start_)) {
        printf("%s, %d fragment is already delivered, ignoring\n", __FUNCTION__, producer->resend_seq_start_);
        producer->resend_seq_start_++;
    } else {
        int current_pos = ftell(producer->file_);

        fseek(producer->file_, offset, SEEK_SET);
        int len = fread(producer->buf_, 1, producer->buf_len_, producer->file_);

        if (AddPacket(producer->channel_, producer->buf_, len, producer->resend_seq_start_)) {
            printf("%s: sending %d fragment once again\n", __FUNCTION__, producer->resend_seq_start_);
        }

        fseek(producer->file_, current_pos, SEEK_SET);
        producer->resend_seq_start_++;
    }

    if (producer->resend_seq_start_ >= producer->total_fragment_number_) {
        producer->resend_seq_start_ = -1;
        producer->resend_seq_end_ = -1;        
    }

    if ((producer->resend_seq_start_ >= producer->resend_seq_end_) || (producer->resend_seq_end_ < 0)) {
        producer->resend_seq_start_ = -1;
        producer->resend_seq_end_ = -1;
    }
}

int SendFileFragment(struct Producer* producer)
{
    if (producer == NULL) {
        printf("producer is not valid\n");
        return 0;
    }

    if (producer->file_ == NULL) {
        printf("file is not opened\n");
        return 0;
    }
    
    if (TryMakeChannelReady(producer->channel_)) {
        if (HasFragmentsToResend(producer)) {
            ResendFragment(producer);
        } else if (producer->bytes_left_ > 0)  {
            int len = fread(producer->buf_, 1, producer->buf_len_, producer->file_);

            if (AddPacket(producer->channel_, producer->buf_, len, producer->fragment_seq_number_)) {
                if (producer->bytes_left_ > len) {
                    producer->bytes_left_ -= len;
                    producer->fragment_seq_number_++;
                } else {
                    producer->bytes_left_ = 0;
                }
            } else {
                fseek(producer->file_, GetFragmentOffset(producer, producer->fragment_seq_number_), SEEK_SET);
            }
        } else {
            time_t ts = time(NULL);
            if (ts - producer->last_retry_ts_ >= MAX_WAIT_TIME) {
                producer->resend_seq_start_ = producer->last_fragment_ack_seq_num_ + 1;
                producer->resend_seq_end_ = producer->total_fragment_number_;

                producer->last_retry_ts_ = ts;

                assert(producer->resend_seq_start_ < producer->total_fragment_number_);

                printf("%s: need to re-send fragments in range %d-%d, ts: %ld\n", __FUNCTION__, producer->resend_seq_start_, producer->resend_seq_end_, ts);
            }
        }
    } else {
        // printf("%s: channel is not ready\n", __FUNCTION__);
    }

    return producer->bytes_left_;
}

BOOL AllPacketsReceived(struct Producer* producer)
{
    if (producer == NULL) {
        printf("%s: producer is not valid\n", __FUNCTION__);
        return 0;
    }

    return (producer->bytes_left_ == 0) && (producer->total_fragment_number_ == producer->fragments_delivered_);
}