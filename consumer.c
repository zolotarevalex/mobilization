#include "consumer.h"

#include <assert.h>

struct Consumer* InitConsumer(struct Channel* channel, const char* file_name)
{
    if (channel == NULL) {
        printf("channel is not valid\n");
        return NULL;
    }

    if (file_name == NULL) {
        printf("file name is not valid\n");
        return NULL;
    }

    struct Consumer* consumer = malloc(sizeof(struct Consumer));
    if (consumer == NULL) {
        printf("failed to allocate consumer\n");
        return NULL;
    }

    consumer->next_seq_number_ = 0;
    consumer->reasm_buf_len_ = 0;
    consumer->reasm_buf_ = NULL;
    consumer->channel_ = channel;
    consumer->file_ = fopen(file_name, "wb");
    if (consumer->file_ == NULL) {
        printf("failed to open %s file\n", file_name);
        free(consumer);
        return NULL;
    }

    channel->receiver_ = consumer;

    return consumer;
}

void CloseConsumer(struct Consumer* consumer)
{
    if (consumer == NULL) {
        printf("consumer is already closed\n");
        return;
    }

    if (consumer->file_) {
        fclose(consumer->file_);
        consumer->file_ = NULL;
    }

    if (consumer->reasm_buf_ != NULL) {
        for (int i = 0; i < consumer->reasm_buf_len_; i++) {
            free(consumer->reasm_buf_[i].buf_);
        }
        free(consumer->reasm_buf_);
        consumer->reasm_buf_ = NULL;
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

int ReceiveFileFragment(struct Consumer* receiver)
{
    if (receiver == NULL) {
        printf("receiver is not valid\n");
        return 0;
    }

    if (receiver->file_ == NULL) {
        printf("file is not opened\n");
        return 0;
    }

    int bytes = 0;
    struct Packet* packet = ReceivePacket(receiver);
    if (packet != NULL) {
        if (receiver->next_seq_number_ < packet->seq_number_) {

            if (receiver->reasm_buf_ == NULL) {
                assert(receiver->channel_->fragments_total_count_ > 0);
                receiver->reasm_buf_len_ = receiver->channel_->fragments_total_count_;
                int size = sizeof(struct ConsumerFragment)*receiver->reasm_buf_len_;
                receiver->reasm_buf_ = malloc(size);
                assert(receiver->reasm_buf_ != NULL);
                memset(receiver->reasm_buf_, 0, size);
            }

            printf("%s: expected %d, received %d fragment\n", __FUNCTION__, receiver->next_seq_number_, packet->seq_number_);

            if (receiver->reasm_buf_[packet->seq_number_].buf_ != NULL) {
                printf("%s: %d fragment duplication, ignoring\n", __FUNCTION__, packet->seq_number_);
            } else {
                receiver->reasm_buf_[packet->seq_number_].seq_number_ = packet->seq_number_;
                receiver->reasm_buf_[packet->seq_number_].len_ = packet->data_len_;
                receiver->reasm_buf_[packet->seq_number_].buf_ = malloc(packet->data_len_);

                assert(receiver->reasm_buf_[packet->seq_number_].len_ > 0);
                assert(receiver->reasm_buf_[packet->seq_number_].buf_ != NULL);

                memcpy(receiver->reasm_buf_[packet->seq_number_].buf_, packet->buffer_, packet->data_len_);                
            }

            receiver->channel_->seq_num_mismatch_handler_(receiver->channel_, receiver->next_seq_number_, packet->seq_number_);
        } else {
            if (receiver->next_seq_number_ == packet->seq_number_) {
                bytes = fwrite(packet->buffer_, 1, packet->data_len_, receiver->file_);
                receiver->next_seq_number_ = packet->seq_number_ + 1;

                receiver->channel_->ack_handler_(receiver->channel_, packet->seq_number_);

                printf("%s: consumed %d fragment, next seq number %d\n", __FUNCTION__, packet->seq_number_, receiver->next_seq_number_);

                FlushReasmBuf(receiver);
            } else {
                printf("%s: ignoring fragment duplication of %d fragment\n", __FUNCTION__, packet->seq_number_);
                // assert(0);
            }
        }

        free(packet->buffer_);
        free(packet);
    }

    return bytes;
}

void FlushReasmBuf(struct Consumer* receiver)
{
    if (receiver == NULL) {
        printf("%s: receiver is not valid\n", __FUNCTION__);
        return;
    }

    if (receiver->reasm_buf_ == NULL || receiver->reasm_buf_len_ == 0) {
        printf("%s: reasm buf is empty\n", __FUNCTION__);
        return;
    }

    printf("%s: next seq number %d\n", __FUNCTION__, receiver->next_seq_number_);
    
    for (; receiver->next_seq_number_ < receiver->reasm_buf_len_; receiver->next_seq_number_++) {
        if (receiver->reasm_buf_[receiver->next_seq_number_].buf_ == NULL) {
            break;
        }

        printf("%s: writing %d fragment\n", __FUNCTION__, receiver->reasm_buf_[receiver->next_seq_number_].seq_number_);

        fwrite(receiver->reasm_buf_[receiver->next_seq_number_].buf_, 1, receiver->reasm_buf_[receiver->next_seq_number_].len_, receiver->file_);

        free(receiver->reasm_buf_[receiver->next_seq_number_].buf_);
        receiver->reasm_buf_[receiver->next_seq_number_].buf_ = NULL;
        receiver->reasm_buf_[receiver->next_seq_number_].len_ = 0;
    }
}