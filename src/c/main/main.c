#include <stdio.h>
#include <string.h>

#include "../login/login.h"
#include "../compression/compression.h"
#include "../encryption/encryption.h"
#include "../fileIO/fileInput/fileInput.h"
#include "../fileIO/fileOutput/fileOutput.h"
#include "../fileManagement/fileManagement.h"

int main() {
    printf("Welcome to the compression script. \n\n");
    char username[100] = "";
    if (loginFunction(username) != 0) {
        puts("Login failed.");
        return 0;
    }
    printf("%s\n", username);
    printf("\n-----------------------------------------------------\n\n");
    

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

            case 3: manageLibrary();
                break;
        }
    } while (selection != 4);

    printf("Thanks! \n");
    return 0;
}
