#include <stdio.h>
#include <string.h>

#include "../login/login.h"
#include "../compression/compression.h"
#include "../encryption/encryption.h"
#include "../fileIO/fileInput/fileInput.h"
#include "../fileIO/fileOutput/fileOutput.h"

int main() {
    printf("Welcome to the compression script. \n");
    char username[100];
    strcpy(username, "testperson67");
    loginScreen(username);
    

    int selection = 0;
    do {
        printf("Would you like to:\n"
               "1. Input files\n"
               "2. Output files\n"
               "3. Exit program\n");

        scanf("%d", &selection);

        switch (selection) {
            case 1: selectFiles(username);
                break;
                
            case 2: browseFiles();
                break;
        }
    } while (selection != 3);

    printf("Thanks! \n");
    return 0;
}
