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

    BOOL enable_packet_delay = TRUE;
    BOOL enable_packet_loss = TRUE;
    BOOL enable_random_rate = TRUE;

    if (argc < 8) {
        printf("too few arguments.\n");
        printf("usage: %s <packet_pool_size> <packet_len> <packet_loss> <enable rand rate> <enable delay> <input_file> <output_file>\n", argv[0]);
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
            enable_packet_loss = FALSE;
        } else {
            packet_loss = packet_loss_arg;
        }
    }

    {
        char* end = NULL;
        float traffic_rate_arg = strtol(argv[4], &end, 10);
        if (traffic_rate_arg == 0) {
            enable_random_rate = FALSE;
        }
    }

    {
        char* end = NULL;
        float packet_delay_arg = strtol(argv[4], &end, 10);
        if (packet_delay_arg == 0) {
            enable_packet_delay = FALSE;
        }
    }

    const char* in_file_name = argv[6];
    const char* out_file_name = argv[7];

    printf("starting simulation with packet_pool: %d, packet_len: %d, packet_loss: %f\n", packet_pool, packet_len,packet_loss );


    struct Channel* channel = InitChannel(packet_pool, packet_len, packet_loss);
    struct Producer* producer = InitProducer(channel, in_file_name);
    struct Consumer* consumer = InitConsumer(channel, out_file_name);

    channel->enable_packet_delay_ = enable_packet_delay;
    channel->enable_packet_loss_ = enable_packet_loss;
    channel->enable_random_rate_ = enable_random_rate;

    time_t start_ts = time(NULL);

    do {
        int bytes_left = SendFileFragment(producer);
        if (bytes_left > 0) {
            printf("bytes left %d\n", bytes_left);
        }
        
        int bytes_received = ReceiveFileFragment(consumer);
        if (bytes_received > 0) {
            printf("bytes received %d\n", bytes_received);
        }

        // sleep(1);
    } while (!AllPacketsReceived(producer));

    time_t end_ts = time(NULL);

    int fragments_left = 0;
    struct Fragment* fragment = producer->next_fragment_;
    while (fragment)
    {
        fragment = fragment->next_;
        fragments_left++;
    }
    
    printf("fragments left: %d\n", fragments_left);

    CloseConsumer(consumer);
    CloseProducer(producer);
    CloseChannel(channel);
    
    printf("simulation took %ld seconds\n", end_ts - start_ts);

    return 0;
}