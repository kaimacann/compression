#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fileInput.h"

#define currentFile availableInputFiles[fileCount]

/* note: will restructure all functions to make more sense later. */

int inputFiles(char username[]) {
    /* read existing file library */
    file_t fileLibrary[200]; 
    int fileLibraryCount = initialiseFileLibrary(fileLibrary);

    /* get input folder from user */
    char folderAddress[150];
    strcpy(folderAddress, "./src/inputFolder"); /* to build file nav */

    /* read files in folder into an array */
    char availableInputFiles[100][200];
    int fileCount = readInputFolder(availableInputFiles, folderAddress);

    /* selecting files from array */
    int selection = selectFile(availableInputFiles, fileCount);

    /* creating the file and initialising its fields */
    char fileAddress[500];
    strcpy(fileAddress, folderAddress);
    strcat(fileAddress, "/");
    strcat(fileAddress, availableInputFiles[selection - 1]);

    file_t newFile;
    int successful = createFile(&newFile, fileAddress, username);
    if (successful == 0) {
        printf("File creation error\n");
        return 0;
    }
    storeFile(&newFile, fileLibrary, &fileLibraryCount);
    saveLibrary(fileLibrary, fileLibraryCount);
    return 1;
}

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

int readInputFolder(char availableInputFiles[100][200], char folderAddress[]) {
    char prompt[153] = "ls -1 ";
    strcat(prompt, folderAddress);
    strcat(prompt, " > ./src/c/fileIO/fileInput/inputFileList.txt");
    system(prompt);

    FILE *fptr = fopen("./src/c/fileIO/fileInput/inputFileList.txt", "r");
    int fileCount = 0;
    while (fileCount < 100 && fgets(currentFile, 200, fptr) != NULL) {
        currentFile[strcspn(currentFile, "\n")] = '\0';
        fileCount++;
    }
    fclose(fptr);

    return fileCount;
}

int selectFile(char availableInputFiles[100][200], int fileCount) {
    printf("Select one of the following files:\n");
    displayFiles(availableInputFiles, fileCount);
    printf("Enter the file number > ");

    int selection = 0;
    scanf("%d", &selection);

    while (selection == 0) {
        printf("\nInvalid selection.\nTry again >");
        scanf("%d", &selection);
    }

    return selection;
}

void displayFiles(char availableInputFiles[100][200], int fileCount) {
    int i;
    for (i = 0; i < fileCount; i++) {
        printf("%d. %s\n", (i + 1), availableInputFiles[i]);
    }
    printf("\n");
}



int createFile(file_t* fileP, char fileAddress[], char username[]) {
    /* name */
    char *fileName = (strrchr(fileAddress, '/') + 1);
    strcpy(fileP->name, fileName);

    /* username */
    strcpy(fileP->username, username);

    /* type */
    char *fileType = (strrchr(fileName, '.') + 1);
    strcpy(fileP->type, fileType);

    /* id */
    generateFileId(fileP);

    /* data */ 
    FILE *fptr = fopen(fileAddress, "rb"); 
    if (fptr == NULL) {
        return 0;
    }
    fseek(fptr, 0, SEEK_END);
    size_t fileSize = (size_t )ftell(fptr);
    rewind(fptr);
    if (fileSize < 0) {
        printf("File size error\n");
        return 0;
    }

    char* dataBuffer = malloc(fileSize);
    if (dataBuffer == NULL) {
        printf("Allocation failed.\n");
        return 0;
    }
    fread(dataBuffer, 1, fileSize, fptr);
    fileP->data = dataBuffer;
    fclose(fptr);

    /* data size */ 
    fileP->dataSize = fileSize; 
    return 1;
}


void generateFileId(file_t* fileP) {
    generateSeedNum(fileP);

    char characters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char newId[15];
    newId[14] = '\0';

    int i;
    for (i = 0; i < 14; i++) {
        newId[i] = characters[rand() % 62];
    }
    strcpy(fileP->id, newId);
}

/* add the amount of files already in the library as a paramater for more randomness */
void generateSeedNum(file_t* fileP) {
    char* fileName = fileP->name;
    char* username = fileP->username;

    int fileNameLen = strlen(fileName) - (strlen(fileP->type)+1);
    int usernameLen = strlen(username) - (strlen(fileP->type)+1);

    unsigned int ranSeedNum = fileName[fileNameLen] + username[0] + fileName[0] + username[usernameLen];
    srand(ranSeedNum);
}

void storeFile(file_t* fileP, file_t fileLibrary[200], int* fileLibraryCountP) { 
    /* Check if file is already stored */
    int alrExists = isInLibrary(fileP->id, fileLibrary, *fileLibraryCountP);
    if (alrExists) {
        printf("Error: fileId already Exists In Library,\n"
                "will replace file\n\n");
    }

    /* Add file to library */
    fileLibrary[(*fileLibraryCountP)] = *fileP;
    *fileLibraryCountP = (*fileLibraryCountP + 1);

    /* storeFile */ 
    storeFileBin(fileP);
}

int isInLibrary(char* fileId, file_t fileLibrary[200], int librarySize) {
    int i = 0;
    while (i < librarySize) {
        if (strcmp(fileId, fileLibrary[i].id) == 0) {
            return 1;
        }
        i++;
    }
    return 0;
}


void storeFileBin (file_t* fileP) {
    char fileStorageLocation[400] = "";
    strcat(fileStorageLocation, "./src/c/library/fileData/");
    strcat(fileStorageLocation, fileP->name);
    FILE* fptr = fopen(fileStorageLocation, "wb");
    if (fptr == NULL) {
        printf("File Binary Store Error");
        return;
    }
    fwrite(fileP->data, 1, fileP->dataSize, fptr);
    fclose(fptr);
}


