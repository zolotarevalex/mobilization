#include "producer.h"
#include "util.h"

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

    struct Producer* producer = malloc(sizeof(struct Producer));
    if (producer == NULL) {
        printf("failed to allocate producer\n");
        return NULL;
    }

    producer->channel_ = channel;
    producer->file_ = fopen(file_name, "rb");
    if (producer->file_ == NULL) {
        printf("failed to opent %s file\n", file_name);
        free(producer);
        return NULL;
    }
    producer->bytes_left_ = fseek(producer->file_, 0, SEEK_END);
    rewind(producer->file_);
    producer->buf_ = NULL;
    producer->bytes_used_ = 0;
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

int SendFile(struct Producer* producer)
{
    if (producer == NULL) {
        printf("producer is not valid\n");
        return 0;
    }

    if (producer->file_ == NULL) {
        printf("file is not opened\n");
        return 0;
    }

    if (producer->bytes_left_ <= 0) {
        printf("no more bytes to send\n");
        return 0;
    }

    if (producer->buf_ == NULL) {
        int buf_len = producer->channel_->max_packet_len_;
        producer->buf_ = malloc(buf_len);
        if (producer->buf_ == NULL) {
            printf("failed to allocate buffer");
            return 0;            
        }

        producer->bytes_used_ = fread(producer->buf_, 1, min(buf_len, producer->bytes_left_), producer->file_);
    }

    if (SendPacket(producer, producer->buf_, producer->bytes_used_)) {
        free(producer->buf_);
        if (producer->bytes_left_ > producer->bytes_used_) {
            producer->bytes_left_ -= producer->bytes_used_;
        } else {
            producer->bytes_left_ = 0;
        }
        producer->bytes_used_ = 0;
    }


    return producer->bytes_left_;
}