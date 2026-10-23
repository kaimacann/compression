#include "../file.c"
/**
 * Header file with function prototypes relating to file selection and output
 * All functions have been created, and will be implemented by Haadi Anwary - 25578814
 */

/**
 * Creates a list of selected files and files to display.
 * Helps user find and select the files they want.
 * 
 * Inputs:
 * - void
 * 
 * Outputs:
 * - Number of outputted files
 */
int browseFiles(void);

/**
 * Displays the given file list with a set amount of files per page.
 * Can navigate through pages
 * 
 * Inputs:
 * - file list to display
 * - number of files in that list
 * - the amount of files to display per page
*/
void displaySearch(file_t fileList[], int totalFiles, int filesPerPage);

/**
 * Writes the metadata of all saved files to the file list.
 * Overwrites data if there is anything on the list.
 * 
 * Inputs:
 * - file list to populate
 * 
 * Outputs:
 * - number of files from the metadata
*/
int readMetadata(file_t fileList[]);

/**
 * Allows user to type in type commands to sort / filter their search
 * 
 * inputs:
 * - file list to modify
 * - pointer to the number of files on that list
 * 
 * outputs:
 * - Modifies the file list depending on query
 */
void modifySearch(file_t fileListp[], int* totalFiles);

/**
 * Displays a help menu to help users with command prompts
 * 
 * Inputs:
 * - void
 * 
 * Outputs:
 * - void
 */
void displayHelpMenu(void);

/**
 * Takes the sort section of the prompt and rearranges list to follow
 * 
 * Inputs:
 * - string of the sort portion of the command prompt
 * - the file list to be sorted
 * - the number of files on that list
 */
void sort(char sort, file_t fileList[], int totalFiles);

/**
 * sorts a given list alphabetically, or reverse alphabetically
 * 
 * Inputs:
 * - the file list
 * - the length of that list
 * - Orientation of conversion 
 */
void sortAlphabetical(file_t fileList[], int totalFiles, int isReversed);

/**
 * sorts a given list by size
 * 
 * Inputs:
 * - the file list
 * - the length of that list
 * - Orientation of conversion 
 */
void sortSize(file_t fileList[], int totalFiles, int isReversed);

/**
 * Filters out list of files based apon command prompt
 * 
 * Inputs:
 * - filter portion of command prompt string
 * - list to be filtered
 * - pointer to the size of that list
 * 
 * Outputs:
 * - modifies the list it was given
 */
void filter(char filter[], file_t fileList[], int* totalFiles);

/**
 * Filters out files that don't have a portion of a string included in their names
 * 
 * Inputs:
 * - search string to be included
 * - file list to be modified
 * - pointer to the number of files on that list
 */
void fileIncludes(char search[], file_t fileList[], int totalFiles);

/**
 * Filters out files that aren't owned by the given user
 * 
 * Inputs:
 * - string of the owners username
 * - file list to be modified
 * - pointer to the number of files on that list
 */
void filterOwner(char ownerName[], file_t fileList[], int totalFiles);

/**
 * Filters out files of a specific type
 * 
 * Inputs:
 * - char of the file type. (d= document, i= image)
 * - file list to be modified
 * - pointer to the number of files on that list
 */
void filterFileType(char fileType, file_t fileList[], int totalFiles);

/**
 * Takes the selected files and gives each file to be handled
 * 
 * Inputs:
 * - list of selected files
 * - total amount of files selected
 * 
 * Outputs
 * - if the file output was successful (int)
 */
int outputFiles(file_t selectedFileList[], int totalSelected);

/**
 * prepares to output each file
 * de-encrypts and decompresses the files before outputting
 * Writes the file to the output folder
 * 
 * Inputs:
 * - pointer to the file
 * - address of the output folder
 * 
 * Outputs:
 * - if it was successful
 */
int handleFileOutput(file_t* file, char outputFolderAdr[]);

/**
 * Reads the binary data file of the associated file metadata (matched with fileID)
 * Stores a pointer to that data in the file metadata struct
 * 
 * Inputs:
 * - pounter to the file
 * 
 * Outputs:
 * - if it was successful (int)
 */
int readFileBinary(file_t* pointer);

/* OUTPUT FILE STUB */
/**
 * Placeholder for writing compression output to a destination path.
 *
 * @param outputPath Destination path for the generated data.
 */
void writeOutputFile(const char *outputPath);

