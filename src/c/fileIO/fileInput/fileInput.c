#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "fileInput.h"

#define currentFile availableInputFiles[fileCount]


int inputFiles(char userName[]) {
    /* read existing file library */
    file_t fileLibrary[200]; 
    int fileLibraryCount = initialiseFileLibrary(fileLibrary);

    /* get input folder from user */
    char folderAddress[150];
    strcpy(folderAddress, "./src/inputFolder");

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
    int successful = createFile(&newFile, fileAddress, userName, fileLibraryCount);
    if (successful == 0) {
        printf("File creation error\n");
        return 0;
    }
    storeFile(&newFile, fileLibrary, &fileLibraryCount);
    saveLibrary(fileLibrary, fileLibraryCount);
    return 1;
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
    displayFolder(availableInputFiles, fileCount);
    printf("Enter the file number > ");

    int selection = getSelection(1, fileCount);

    return selection;
}

void displayFolder(char availableInputFiles[100][200], int fileCount) {
    int i;
    for (i = 0; i < fileCount; i++) {
        printf("%d. %s\n", (i + 1), availableInputFiles[i]);
    }
    printf("\n");
}



int createFile(file_t* fileP, char fileAddress[], char userName[], int fileLibraryCount) {
    /* name */
    char fileName[100] = "";
    strcpy(fileName, (strrchr(fileAddress, '/') + 1));
    strcpy(fileP->name, fileName);

    /* userName */
    strcpy(fileP->userName, userName);

    /* type */
    char fileType = *(strrchr(fileName, '.') + 1);
    if (fileType != 'p' && fileType != 'd' && fileType != 'i') {
        fileP->type = 'o';
    }
    else {
        fileP->type = fileType;
    }

    /* id */
    generateFileId(fileP, fileLibraryCount);

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


void generateFileId(file_t* fileP, int fileLibraryCount) {
    generateSeedNum(fileP, fileLibraryCount);

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
void generateSeedNum(file_t* fileP, int fileLibraryCount) {
    char fileName[400];
    strcpy(fileName, fileP->name);
    *(strrchr(fileName, '.')) = '\0';

    char *userName = fileP->userName;

    unsigned int ranSeedNum = fileLibraryCount + fileName[strlen(fileName)-1] + userName[0] + fileName[0] + userName[strlen(userName)-1];
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
    strcat(fileStorageLocation, fileP->id);
    FILE* fptr = fopen(fileStorageLocation, "wb");
    if (fptr == NULL) {
        printf("File Binary Store Error");
        return;
    }
    fwrite(fileP->data, 1, fileP->dataSize, fptr);
    fclose(fptr);
}


