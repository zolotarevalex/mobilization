#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "files.h"
#include "util.h"

const char* make_file_name(char* buf, int size, const char* base, int base_size, const char* ext, int ext_size)
{
    if (buf == NULL) {
        printf("dst buffer is not valid\n");
        return NULL;
    }

    if (size <= 0) {
        printf("dst buffer size must be > 0\n");
        return NULL;
    }

    if (base == NULL) {
        printf("base file name is not valid\n");
        return NULL;
    }

    if (ext == NULL) {
        printf("file ext is not valid\n");
        return NULL;
    }

    if (ext_size <= 0) {
        printf("file ext size must be > 0\n");
        return NULL;
    }

    if (size < (base_size + ext_size + 2)) {
        printf("not enough space to store the result\n");
        return NULL;
    }

    snprintf(buf, size - 1, "%s.%s", base, ext);
    return buf;
}

const char* make_in_name(char* buf, int size, const char* base, int base_size)
{
    return make_file_name(buf, size, base, base_size, "in", 2);
}

const char* make_out_name(char* buf, int size, const char* base, int base_size)
{
    return make_file_name(buf, size, base, base_size, "out", 3);
}

void gen_file(int file_size, const char* name)
{
    if (file_size <= 0) {
        printf("file size must be > 0\n");
        return;
    }

    if (name == NULL) {
        printf("file name is not valid\n");
        return;
    }

    char* buf = malloc(file_size);
    if (buf == NULL) {
        printf("failed to allocate buffer\n");
        return;
    }

    FILE* pFile = fopen(name, "wb");
    if (pFile == NULL) {
        printf("failed to create %s file\n", name);
        return;
    }

    for (int i = 0; i < file_size; i++) {
        buf[i] = rand() % 255;
    }

    fwrite(buf, file_size, file_size, pFile);

    fclose(pFile);
    free(buf);
}

void gen_test_files(int files_num, int file_size, const char* base_name)
{
    if (base_name == NULL) {
        printf("base name is not balid\n");
        return;
    }

    if (file_size <= 0) {
        printf("file size must be > 0\n");
        return;
    }

    for (int i = 0; i < files_num; i++) {
        char name_buf[255] = {0,};
        snprintf(name_buf,  sizeof(name_buf) - 1, "%s%d.in", base_name, i);
        gen_file(file_size, name_buf);   
    }
}

const char* read_test_file(const char* name)
{
    if (name == NULL) {
        printf("filename is not valid\n");
        return NULL;
    }

    char* buf = NULL;

    FILE* pFile = fopen(name, "rb");
    if (pFile == NULL) {
        printf("failed to open %s file\n", name);
        return NULL;
    } 

    int size = fseek(pFile, 0, SEEK_END);
    printf("reading file %s with size %d\n,", name, size);
    if (size == 0) {
        printf("%s file is empty\n", name);
        fclose(pFile);
        return NULL;
    }

    rewind(pFile);

    buf = malloc(size);
    if (buf == NULL) {
        printf("failed to allocate buffer\n");
        fclose(pFile);
        return NULL;
    }

    fread(buf, size, size, pFile);

    fclose(pFile);

    return buf;
}

void save_test_file(const char* buf, int size, const char* name)
{
    if (buf == NULL) {
        printf("input buffer is not valid\n");
        return;
    }

    if (size <= 0) {
        printf("buffer size must be > 0\n");
        return;
    }

    if (name == NULL) {
        printf("filename is not valid\n");
        return;
    }

    FILE* pFile = fopen(name, "wb");
    if (pFile == NULL) {
        printf("failed to create %s file\n", name);
        return;
    }

    fwrite(buf, size, size, pFile);
    fclose(pFile);
}