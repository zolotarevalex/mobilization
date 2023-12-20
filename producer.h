#ifndef PRODUCER_H_
#define PRODUCER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "channel.h"

struct ProducerFragment 
{
    struct ProducerFragment* next_;
    struct ProducerFragment* prev_;
    int seq_number_;

    BOOL delivered_;
};

struct Producer
{
    struct Channel* channel_;

    //file
    FILE* file_;

    //total number 0f fragments requirted to send a file
    int total_fragment_number_;

    struct ProducerFragment* fragments_;
    struct ProducerFragment* fragments_head_;
    struct ProducerFragment* fragments_tail_;
    struct ProducerFragment* fragment_to_be_sent_;

    //total number of fragments with ACK received
    int fragments_delivered_;

    //max fragment len
    int fragment_len_;

    //file size
    int total_bytes_;

    //size of buffer to read frtagment from file
    int buf_len_;
    //buf to read single file fragment
    char buf_[];
};

int GetFragmentOffset(struct Producer* producer, int seq_number);

void HandleFragmentAck(struct Channel* channel, int fragment_seq_number);
void HandleFragmentSeqNumberMismatch(struct Channel* channel, int expected, int received);
void HandleFragmentDelivered(struct Producer* producer, struct ProducerFragment* fragment);

BOOL HasFragmentsToResend(struct Producer* producer);
void ResendFragment(struct Producer* producer);

struct Producer* InitProducer(struct Channel* channel, const char* file_name);
void CloseProducer(struct Producer* producer);
void SendFileFragment(struct Producer* producer);
BOOL AllPacketsReceived(struct Producer* producer);

#endif