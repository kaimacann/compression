#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fileManagement.h"

int initialiseFileLibrary (file_t fileLibrary[]) {
    int fileLibraryCount = 0;

	FILE* fptr = fopen("./src/c/library/fileLibraryMetadata.db", "rb");
    if (fptr == NULL) {
        printf("Creating new file Library\n\n");
        return fileLibraryCount;
    }
    fileLibraryCount = fread(fileLibrary, sizeof(file_t), 200, fptr);
    fclose(fptr);
    return fileLibraryCount;
}

void saveLibrary(file_t fileLibrary[], int fileLibraryCount) {
	FILE* fptr = fopen("./src/c/library/fileLibraryMetadata.db", "wb");
    if (fptr == NULL) {
        printf("Could not open input file list.\n");
        return;
    }
	fwrite(fileLibrary, sizeof(file_t), fileLibraryCount, fptr);
	fclose(fptr);
}

void displayFiles(file_t fileLibrary[], int fileLibraryCount) {
    int i;
    for (i = 0; i < fileLibraryCount; i++) {
        printf("%d. %s\n", (i+1), fileLibrary[i].name);
    }
    printf("\n");
}

void manageLibrary() {
    int selection = 0;
    file_t fileLibrary[200];
    int fileLibraryCount;
    do { 
        fileLibraryCount = initialiseFileLibrary(fileLibrary);

        printf("How would you like to change the file library?:\n"
               "1. Delete file\n"
               "2. Delete all files\n"
               "3. Exit program\n\n");
        
        printf("Enter your selection > ");
        scanf("%d", &selection);
        printf("\n-----------------------------------------------------\n\n");
        switch (selection) {
            case 1: deleteFile(fileLibrary, fileLibraryCount);
                break;
                
            case 2: deleteAllFiles();
                break;
        }
    } while (selection != 3);
    printf("Returning to Home Menu...\n");
}

int deleteFile(file_t fileLibrary[], int fileLibraryCount) {
    printf("Pick a file to delete:\n");
    displayFiles(fileLibrary, fileLibraryCount);
    printf("Enter your selection >");
    int selection = getSelection(1, fileLibraryCount);

    return 1;
}

int deleteAllFiles() {

    return 1;
}

int getSelection(int min, int max){
    int selection = 0;
    scanf("%d", &selection);

    while (selection < min || selection > max) {
        printf("\nInvalid selection.\nTry again >");
        scanf("%d", &selection);
    }
    return selection;
}