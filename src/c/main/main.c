#include <stdio.h>
#include <string.h>

#include "../login/login.h"
#include "../compression/compression.h"
#include "../encryption/encryption.h"
#include "../fileIO/fileInput/fileInput.h"
#include "../fileIO/fileOutput/fileOutput.h"

int main() {
    printf("Welcome to the compression script. \n\n");
    char username[100];
    strcpy(username, "testperson67");
    loginScreen(username);
    

    int selection = 0;
    do {
        printf("Would you like to:\n"
               "1. Input files\n"
               "2. Output files\n"
               "3. Manage library\n"
               "4. Exit program\n\n");
        
        printf("Enter your selection > ");
        scanf("%d", &selection);
        printf("\n-----------------------------------------------------\n\n");
        switch (selection) {
            case 1: inputFiles(username);
                break;
                
            case 2: browseFiles();
                break;
        }
    } while (selection != 4);

    printf("Thanks! \n");
    return 0;
}
