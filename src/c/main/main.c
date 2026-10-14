#include <stdio.h>
#include "../login/login.h"
#include "../compression/compression.h"
#include "../encryption/encryption.h"
#include "../fileIO/fileInput/fileInput.h"
#include "../fileIO/fileOutput/fileOutput.h"

int main() {
    printf("Welcome to the compression script. \n");

    int selection = 0;
    do {
        printf("Would you like to:\n"
               "1. Input files\n"
               "2. Output files\n"
               "3. Exit program\n");

        scanf("%d", &selection);
    } while (selection != 3);

    switch (selection) {
        case:
    }
    return 0;
}
