#pragma once
#include <stdlib.h>

struct file {
    char id[15];
    char name[100];
    char userName[100];
    char type;
    unsigned char data;
    size_t dataSize;
}; typedef struct file file_t;