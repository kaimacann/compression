#pragma once

#include <stddef.h>

/*//////////////////////////////////////////////////
FILE RECORD
//////////////////////////////////////////////////*/

/* data holds the loaded byte buffer, and dataSize records its byte length. */
struct file {
    char id[15];
    char name[100];
    char userName[100];
    char type; /* 'p'=PDF, 'd'=document, 'i'=image, 'o'=other */
    unsigned char *data;
    size_t dataSize;
};

typedef struct file file_t;
