#pragma once
#include <stdlib.h>

struct file {
    char id[15];
    char name[200];
    char username[100];
    char type[10];
    char *data;
    size_t dataSize;
}; typedef struct file file_t;