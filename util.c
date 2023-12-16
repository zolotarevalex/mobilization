#include "util.h"

#include <stdio.h>
#include <assert.h>

int min(int val1, int val2)
{
    return val1 > val2 ? val2 : val1;
}

BOOL GetFlag(const char* map, int map_len, int seq_number)
{
    if (map == NULL || map_len == 0) {
        printf("%s: bitmap is not valid\n", __FUNCTION__);
        assert(0);
        return FALSE;
    }

    int octet = -1;
    int bit = -1;

    GetIndex(seq_number, &octet, &bit);

    if (octet < 0 || bit < 0) {
        printf("%s: failed to get octet/bit index\n", __FUNCTION__);
        assert(0);
        return FALSE;
    }

    if (octet >= map_len) {
        printf("%s, %d octet is ot of range %d\n", __FUNCTION__, octet, map_len);
        assert(0);
        return FALSE;
    }

    if (bit > 8) {
        printf("%s, bit offset %d is out of range\n", __FUNCTION__, bit);
        assert(0);
        return FALSE;
    }

    char mask = 1 << bit;

    return (map[octet] & mask);
}

BOOL SetFlag(char* map, int map_len, int seq_number)
{
    if (map == NULL || map_len == 0) {
        printf("%s: bitmap is not valid\n", __FUNCTION__);
        assert(0);
        return FALSE;
    }

    int octet = -1;
    int bit = -1;

    GetIndex(seq_number, &octet, &bit);

    if (octet < 0 || bit < 0) {
        printf("%s: failed to get octet/bit index\n", __FUNCTION__);
        assert(0);
        return FALSE;
    }

    if (octet >= map_len) {
        printf("%s, %d octet is ot of range %d\n", __FUNCTION__, octet, map_len);
        assert(0);
        return FALSE;
    }

    if (bit > 8) {
        printf("%s, bit offset %d is out of range\n", __FUNCTION__, bit);
        assert(0);
        return FALSE;
    }

    BOOL changed = FALSE;
    char mask = 1 << bit;
    char oldVal = map[octet];
    if ((mask & oldVal) == 0) {
        map[octet] |= mask;
        changed = TRUE;
    }

    return changed;
}

void GetIndex(int seq_number, int* octet, int* bit)
{
    if (seq_number < 0) {
        *octet = -1;
        *bit = -1;  
    } else {
        *octet = seq_number / 8;
        *bit = seq_number % 8;
    }
}
