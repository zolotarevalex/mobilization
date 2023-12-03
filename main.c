#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "channel.h"
#include "consumer.h"
#include "producer.h"

int main(int argc, char* argv[])
{
    srand(time(NULL));

    int packet_pool = 100000;
    int packet_len = 1024;
    float packet_loss = 0.1;

    if (argc < 6) {
        printf("too few arguments.\n");
        printf("usage: %s <packet_pool_size> <packet_len> <packet_loss> <input_file> <output_file>\n", argv[0]);
        return 0;
    }

    {
        char* end = NULL;
        int packet_pool_arg = strtol(argv[1], &end, 10);
        if (packet_pool_arg == 0) {
            printf("packet_pool_arg is not valid, using default value %d\n", packet_pool);
        } else {
            packet_pool = packet_pool_arg;
        }
    }

    {
        char* end = NULL;
        int packet_len_arg = strtol(argv[2], &end, 10);
        if (packet_len_arg == 0) {
            printf("packet_len_arg is not valid, using default value %d\n", packet_len);
        } else {
            packet_len = packet_len_arg;
        }
    }

    {
        char* end = NULL;
        float packet_loss_arg = strtof(argv[3], &end);
        if (packet_loss_arg == 0) {
            printf("packet_loss_arg is not valid, using default value %f\n", packet_loss);
        } else {
            packet_loss = packet_loss_arg;
        }
    }

    const char* in_file_name = argv[4];
    const char* out_file_name = argv[5];

    printf("starting simulation with packet_pool: %d, packet_len: %d, packet_loss: %f\n", packet_pool, packet_len,packet_loss );

    struct Channel* channel = InitChannel(packet_pool, packet_len, packet_loss);
    struct Producer* producer = InitProducer(channel, in_file_name);
    struct Consumer* consumer = InitConsumer(channel, out_file_name);

    do {
        int bytes_left = SendFile(producer);
        if (bytes_left > 0) {
            printf("bytes left %d\n", bytes_left);
        }
        
        int bytes_received = ReceiveFile(consumer);
        if (bytes_received > 0) {
            printf("bytes received %d\n", bytes_received);
        }

        // sleep(1);
    } while (!AllPacketsReceived(channel));

    CloseConsumer(consumer);
    CloseProducer(producer);
    CloseChannel(channel);
    
    return 0;
}