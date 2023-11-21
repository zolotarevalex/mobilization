#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "channel.h"
#include "consumer.h"
#include "producer.h"


char* MakeTestBuf(int len)
{
    char* buf = malloc(len);
    if (buf != NULL) {
        for (int j = 0; j < len; j++) {
            buf[j] = rand() % 255 + 1;
        }
    }
    
    return buf;
}


int main(int argc, char* argv[])
{
    srand(time(NULL));

    int packet_pool = 100000;
    int packet_len = 1024;
    float packet_loss = 0.1;

    if (argc > 1) {
        char* end = NULL;
        int packet_pool_arg = strtol(argv[1], &end, 10);
        if (packet_pool_arg == 0) {
            printf("packet_pool_arg is not valid, using default value %d\n", packet_pool);
        } else {
            packet_pool = packet_pool_arg;
        }
    }

    if (argc > 2) {
        char* end = NULL;
        int packet_len_arg = strtol(argv[2], &end, 10);
        if (packet_len_arg == 0) {
            printf("packet_len_arg is not valid, using default value %d\n", packet_len);
        } else {
            packet_len = packet_len_arg;
        }
    }

    if (argc > 3) {
        char* end = NULL;
        float packet_loss_arg = strtof(argv[3], &end);
        if (packet_loss_arg == 0) {
            printf("packet_loss_arg is not valid, using default value %f\n", packet_loss);
        } else {
            packet_loss = packet_loss_arg;
        }
    }

    printf("starting simulation with packet_pool: %d, packet_len: %d, packet_loss: %f\n", packet_pool, packet_len,packet_loss );

    struct Channel* channel = InitChannel(packet_pool, packet_len, packet_loss);
    struct Producer* producer = InitProducer(channel);
    struct Consumer* consumer = InitConsumer(channel);

    while (TRUE) {
        {
            char* buf = MakeTestBuf(packet_len);
            struct Packet* packet = SendPacket(producer, buf, packet_len);
            if (packet == NULL) {
                // printf("failed to send packet\n");
            }
            free(buf);
        }
        
        {
            struct Packet* packet = ReceivePacket(consumer);
            if (packet != NULL) {
                printf("received packet of %d bytes\n", packet->len_);
                free(packet);
            } else {
                // printf("nothing to receive from the channel\n");
            }
        }
    }

    CloseConsumer(consumer);
    CloseProducer(producer);
    CloseChannel(channel);
    
    return 0;
}