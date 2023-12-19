#ifndef UTIL_H_
#define UTIL_H_

typedef int BOOL;

#define TRUE 1
#define FALSE 0

int min(int val1, int val2);

BOOL GetFlag(const char* map, int map_len, int seq_number);
BOOL SetFlag(char* map, int map_len, int seq_number);
void GetIndex(int seq_number, int* octet, int* bit);

#endif //UTIL_H_