#include "producer.h"
#include "util.h"

#include <assert.h>


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

    memset(producer->buf_, 0, producer->buf_len_);

    producer->channel_ = channel;
    producer->file_ = fopen(file_name, "rb");
    if (producer->file_ == NULL) {
        printf("failed to opent %s file\n", file_name);
        free(producer);
        return NULL;
    }

    fseek(producer->file_, 0, SEEK_END);
    producer->bytes_left_ = ftell(producer->file_);

    printf("%s bytes_left %d to read file %s\n", __FUNCTION__, producer->bytes_left_, file_name);

    rewind(producer->file_);

    producer->channel_->data_len_ = producer->bytes_left_; 
    producer->next_fragment_ = NULL;
    producer->missed_fragment_ = NULL;
    producer->fragment_seq_number_ = 0;

    channel->sender_ = producer;
    channel->ack_handler_ = HandleFragmentAck;
    channel->nack_handler_ = HandleFragmentNAck;

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

    struct Fragment* fragment = producer->next_fragment_;
    while(fragment) {
        struct Fragment* next = fragment->next_;
        free(fragment);
        fragment = next;
    }

    fragment = producer->missed_fragment_;
    while(fragment) {
        struct Fragment* next = fragment->next_;
        free(fragment);
        fragment = next;
    }
}

struct Fragment* GetNewFragment(int len, int seq_number, int file_offset)
{
    struct Fragment* fragment = malloc(sizeof(struct Fragment));

    if (fragment) {
        fragment->len_ = len;
        fragment->seq_number_ = seq_number;
        fragment->next_ = NULL;
        fragment->file_offset_ = file_offset;
    }

    return fragment;
}

struct Fragment* GetFragment(struct Producer* producer, int fragment_seq_number)
{
    if (producer == NULL) {
        printf("%s, producer is not valid\n", __FUNCTION__);
        return NULL;
    }

    struct Fragment* fragment = producer->next_fragment_;
    while(fragment) {
        if (fragment->seq_number_ == fragment_seq_number) {
            return fragment;
        }
        fragment = fragment->next_;
    }

    printf("%s: failed to find %d fragment\n", __FUNCTION__, fragment_seq_number);
    return NULL;
}

struct Fragment* FreeFragment(struct Producer* producer, int fragment_seq_number)
{
    if (producer == NULL) {
        printf("%s, producer is not valid\n", __FUNCTION__);
        return NULL;
    }

    struct Fragment* fragment = producer->next_fragment_;
    struct Fragment* prev_fragment = fragment;
    while(fragment) {
        if (fragment->seq_number_ == fragment_seq_number) {
            if (fragment == producer->next_fragment_) {
                producer->next_fragment_ = fragment->next_;
            } else {
                prev_fragment->next_ = fragment->next_;
            }

            fragment->next_ = NULL;
            return fragment;
        }
        prev_fragment = fragment;
        fragment = fragment->next_;
    }

    printf("%s: failed to find %d fragment\n", __FUNCTION__, fragment_seq_number);
    return NULL;
}

struct Fragment* CloneFragment(struct Fragment* fragment)
{
    if (fragment == NULL) {
        printf("%s: fragment is not valid\n", __FUNCTION__);
        return NULL;
    }

    return GetNewFragment(fragment->len_, fragment->seq_number_, fragment->file_offset_);
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

    free(FreeFragment(channel->sender_, fragment_seq_number));
}

void HandleFragmentNAck(struct Channel* channel, int fragment_seq_number)
{
    if (channel == NULL) {
        printf("%s: channel is not valid\n", __FUNCTION__);
        return;
    }

    if (channel->sender_ == NULL) {
        printf("%s: producer is not valid\n", __FUNCTION__);
        return;
    }

    // struct Fragment* fragment = FreeFragment(channel->sender_, fragment_seq_number);
    // if (fragment != NULL) {
    //     struct Fragment* to_resend = GetNewFragment(fragment->len_, fragment->seq_number_, fragment->file_offset_);
    //     if (to_resend == NULL) {
    //         printf("%s: failed to clone %d fragment\n", __FUNCTION__, fragment->seq_number_);
    //     } else {
    //         to_resend->next_ = channel->sender_->missed_fragment_;
    //         channel->sender_->missed_fragment_ = to_resend;
    //         printf("%s: adding %d fragment to resend queue\n", __FUNCTION__, fragment->seq_number_);
    //     }
    //     fragment->next_ = channel->sender_->next_fragment_;
    //     channel->sender_->next_fragment_ = fragment; 
    // } else {
    //     printf("%s: failed to handle packet drop, %d frame not found\n", __FUNCTION__, fragment_seq_number);
    // }
}

BOOL ResendFragment(struct Producer* producer, struct Fragment* fragment)
{
    BOOL rc = TRUE;
    int current_pos = ftell(producer->file_);

    fseek(producer->file_, fragment->file_offset_, SEEK_SET);
    int len = fread(producer->buf_, 1, producer->buf_len_, producer->file_);
    if (len != fragment->len_) {
        printf("%s: unexpected fragment len, expected %d, received %d, fragment %d\n", __FUNCTION__, fragment->len_, len, fragment->seq_number_);
        assert(len == fragment->len_);
    }

    if (AddPacket(producer->channel_, producer->buf_, fragment->len_, fragment->seq_number_) == NULL) {
        printf("%s: unable to re-deliver %d fragment\n", __FUNCTION__, fragment->seq_number_);
        rc = FALSE;
    }

    fseek(producer->file_, current_pos, SEEK_SET);
    return rc;
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
        if (producer->next_fragment_ != NULL) {
            ResendFragment(producer, producer->next_fragment_);
        } else {
            struct Fragment* fragment = GetNewFragment(producer->channel_->max_packet_len_, producer->fragment_seq_number_, ftell(producer->file_));

            if (fragment == NULL) {
                printf("%s: not able to allocate fragment\n", __FUNCTION__);
                printf("%s, channel->data_len_: %d, channel->bytes_sent_: %d, channel->bytes_received_: %d\n", 
                        __FUNCTION__, producer->channel_->data_len_, producer->channel_->bytes_sent_, producer->channel_->bytes_received_);
                return 0;
            }

            fragment->len_ = fread(producer->buf_, 1, producer->buf_len_, producer->file_);

            if (AddPacket(producer->channel_, producer->buf_, fragment->len_, fragment->seq_number_)) {
                if (producer->bytes_left_ > fragment->len_) {
                    producer->bytes_left_ -= fragment->len_;
                    producer->fragment_seq_number_++;
                } else {
                    producer->bytes_left_ = 0;
                }

                fragment->next_ = producer->next_fragment_;
                producer->next_fragment_ = fragment;
            } else {
                fseek(producer->file_, fragment->file_offset_, SEEK_SET);
                free(fragment);
            }    
        }
    } else {
        printf("%s: channel is not ready\n", __FUNCTION__);
    }

    return producer->bytes_left_;
}