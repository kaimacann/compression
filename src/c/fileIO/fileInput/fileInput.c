#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fileInput.h"

void clearInput() {
	int ch;
	while ((ch = getchar()) != '\n' && ch != EOF);
}

int selectFiles(char username[]) {
    char folderAddress[150];
    strcpy(folderAddress, "./src/inputFolder"); /* to build file nav */

    displayFiles(folderAddress);

    file_t newFile = createFile(folderAddress, username);

    return 1;
}

void displayFiles(char folderAddress[]) {
    printf("Select one of the following files:\n");
    char prompt[153] = "ls -1 ";
    strcat(prompt, folderAddress);
    strcat(prompt, "| cat -n");
    system(prompt);
}

file_t createFile(char folderAddress[], char username[]) {
    clearInput();
    printf("Enter file name > ");
    char fileName[100];
    fgets(fileName, 100, stdin);

    file_t newFile;
    strcpy(newFile.userName, username);

    char *type = strchr(fileName, '.');
    return newFile;
}
