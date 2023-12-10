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

    if (AllPacketsReceived(receiver->channel_)) {
        printf("all fragments received\n");
        return 0;
    }

    int bytes = 0;
    struct Packet* packet = ReceivePacket(receiver);
    if (packet != NULL) {
        if (receiver->next_seq_number_ < packet->seq_number_) {
            printf("%s: expected %d, received %d fragment\n", __FUNCTION__, receiver->next_seq_number_, packet->seq_number_);
            receiver->channel_->nack_handler_(receiver->channel_, receiver->next_seq_number_);
        } else {
            if (receiver->next_seq_number_ - packet->seq_number_ > 1) {
                printf("%s: ignoring packet duplication of %d frame\n", __FUNCTION__, packet->seq_number_);
            } else {
                printf("%s: consumed %d packet\n", __FUNCTION__, receiver->next_seq_number_);
                bytes = fwrite(packet->buffer_, 1, packet->data_len_, receiver->file_);
                receiver->next_seq_number_ = packet->seq_number_ + 1;
            }

            receiver->channel_->ack_handler_(receiver->channel_, packet->seq_number_);
        }

        free(packet);
    }

    return bytes;
}