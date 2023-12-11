#ifndef PRODUCER_H_
#define PRODUCER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "channel.h"

struct Fragment
{
    int seq_number_;
    int len_;
    long int file_offset_;
    time_t ts_;
    struct Fragment* next_;
};

struct LostFragments
{
    int seq_number_start_;
    int seq_number_end_;
    long int file_offset_;
    struct LostFragments* next_;
};

struct Producer
{
    struct Channel* channel_;
    FILE* file_;
    int buf_len_;
    int bytes_left_;
    struct Fragment* next_fragment_;
    struct Fragment* lost_fragments_;
    int fragment_seq_number_;
    char buf_[];
};

struct Fragment* GetNewFragment(int len, int seq_number, int file_offset);
struct Fragment* GetFragment(struct Fragment* fragment, int seq_number);
struct Fragment* CloneFragment(struct Fragment* fragment);
struct Fragment* FreeFragment(struct Producer* producer, int seq_number);
BOOL ResendFragment(struct Producer* producer, struct Fragment* fragment);
void HandleFragmentAck(struct Channel* channel, int fragment_seq_number);
void HandleFragmentNAck(struct Channel* channel, int seq_number_start, int seq_number_end);

struct Producer* InitProducer(struct Channel* channel, const char* file_name);
void CloseProducer(struct Producer* producer);
int SendFileFragment(struct Producer* producer);
BOOL AllPacketsReceived(struct Producer* producer);

#endif