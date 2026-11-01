#include "../fileIO/file.c"

int initialiseFileLibrary (file_t fileLibrary[]);
void saveLibrary(file_t fileLibrary[], int fileLibraryCount);
void displayFiles(file_t fileLibrary[], int fileLibraryCount);

void manageLibrary();
int deleteFile(file_t fileLibrary[], int fileLibraryCount);
int deleteAllFiles();

int getSelection(int min, int max);