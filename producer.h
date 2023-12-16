#ifndef PRODUCER_H_
#define PRODUCER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "channel.h"

struct Producer
{
    struct Channel* channel_;

    //file
    FILE* file_;
    
    //current fragment seq number
    int fragment_seq_number_;

    int last_fragment_ack_seq_num_;

    //size of fragments map
    int fragment_map_buf_len_;

    //map to handle fragment send/ack
    char* fragments_map_;

    //size of fragmen_ack_counter_map_
    int fragmen_ack_counter_map_size_;

    //map of fragments ack ranges, one entry per each 1000 fragments.
    //used to ommit range during in case if all fragments within this range already have acks
    short* fragmen_ack_counter_map_;

    //total number 0f fragments requirted to send a file
    int total_fragment_number_;

    //total number of fragments with ACK received
    int fragments_delivered_;

    //max fragment len
    int fragment_len_;

    //bytes still need to read
    int bytes_left_;

    //file size
    int total_bytes_;

    //represetns start of the fragments range to be resent
    int resend_seq_start_;

    //represetns end of the fragments range to be resent
    int resend_seq_end_;

    //timestamp of last retry iteration
    time_t last_retry_ts_;

    //size of buffer to read frtagment from file
    int buf_len_;
    //buf to read single file fragment
    char buf_[];
};

int GetFragmentOffset(struct Producer* producer, int seq_number);

void HandleFragmentAck(struct Channel* channel, int fragment_seq_number);
void HandleFragmentSeqNumberMismatch(struct Channel* channel, int expected, int received);

BOOL HasFragmentsToResend(struct Producer* producer);
void ResendFragment(struct Producer* producer);

struct Producer* InitProducer(struct Channel* channel, const char* file_name);
void CloseProducer(struct Producer* producer);
int SendFileFragment(struct Producer* producer);
BOOL AllPacketsReceived(struct Producer* producer);

#endif