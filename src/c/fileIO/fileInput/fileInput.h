#include "../file.c"
/**
 * Header file with function prototypes relating to file input
 * All functions have been created, and will be implemented by Haadi Anwary - 25578814
 */

/**
 * - Function that creates and populates a list of user selected files from multiple locations.
 * - It creates a file struct for each file and populates the username, file type, data and data size.
 * 
 * Inputs:
 * - username
 * 
 * Outputs:
 * - number of successfully inputted files
 */
int selectFiles(char username[]);

/**
 * Displays the files in a given folder location
 */
void displayFiles(char folderAddress[]);

/**
 * Creates a file from a given name
 */
file_t createFile(char folderAddress[], char username[]);

/**
 * - Displays the items in the current folder address
 * - Prompts user if they want to navigate elsewhere or if they want to select files.
 * - Default address is the directory in which the program is located
 * 
 * Inputs:
 * - current folder address
 * 
 * Outputs:
 * - may change the value of folder address
 */
void displayFolder(char folderAddress[]);

/**
 * Allows user to change the folder address they are nagivating to.
 * 
 * Inputs:
 * - folder address
 * 
 * Outputs:
 * - May change the value of folder address
 */
void inputFolderAddress(char folderAddress[]);


/**
 * Takes the compiled list of files, assigns them a unique ID
 * and calls "handleFileInput" for each file
 * 
 * Inputs:
 * - The list of files to be stored
 * - The length of that list
 * 
 * Outputs:
 * - The number of files successfully inputted.
*/
int inputFiles(file_t fileInList[], int listLength, char username[]);

/**
 * Generates a random string of 14 characters (to be used for fileIDs).
 * 
 * Inputs:
 * - void
 * 
 * Outputs:
 * - Pointer to generated string
 */
char* generateFileId();

/**
 * Takes each file from "inputFiles" and runs them through the storage process
 * - calls compression, encryption, and store functions.
 * 
 * Inputs:
 * - pointer to the file
 * 
 * Outputs:
 * - int for if its successful or not
*/
int handleFileInput(file_t* fileP);

/**
 * Takes a pointer to a file struct and splits up its data and metadata to write
 * in seperate folders.
 * 
 * Inputs:
 * - pointer to the file
 * 
 * Outputs:
 * - int for if its successful or not
 */
int storeFile(file_t* fileP);

/**
 * writes binary data to file using the name of "<fileId>.bin"
 * 
 * Inputs:
 * - pointer to the file
 * 
 * Outputs:
 * - int for if its successful or not
 */
int writeFile(file_t* fileP);

/**
 * Writes all the structs as binary
 * 
 * Inputs:
 * - pointer to the file
 * 
 * Outputs:
 * - int for if its successful or not
 */
int writeMetaData(file_t* fileP);

