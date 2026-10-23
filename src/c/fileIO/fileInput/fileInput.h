/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include "../file.c"

/*//////////////////////////////////////////////////
PUBLIC API
//////////////////////////////////////////////////*/

/* FILE SELECTION */
/**
 * Runs the current one-file selection flow in the configured input folder.
 *
 * @param username Owner copied into the temporary file record.
 * @return 1 after the selection flow is invoked.
 */
int selectFiles(char username[]);

/**
 * Prints a numbered listing of the entries in the supplied folder.
 *
 * @param folderAddress Path passed to the existing listing command.
 */
void displayFiles(char folderAddress[]);

/**
 * Prompts for a filename and creates a partially populated file record.
 *
 * @param folderAddress Folder associated with the selection; currently unused by the prompt.
 * @param username Owner copied into the record.
 * @return A record with its owner set; filename and type handling remain unfinished.
 */
file_t createFile(char folderAddress[], char username[]);

/* FOLDER NAVIGATION */
/**
 * Displays entries in the current folder and prompts for navigation or selection.
 *
 * @param folderAddress Current path, which navigation may update.
 */
void displayFolder(char folderAddress[]);

/**
 * Prompts for a folder path and updates the caller's path buffer.
 *
 * @param folderAddress Path buffer to update.
 */
void inputFolderAddress(char folderAddress[]);

/* FILE PROCESSING AND STORAGE */
/**
 * Reads a file path into a file record and infers its type from its extension.
 *
 * On success, the caller owns fileP->data and must release it with free().
 * The output record should be zero-initialized before this call.
 *
 * @param inputPath Source path to read in binary mode.
 * @param fileP Record populated with the bytes, size, name, and type.
 * @return 1 when the complete file was read, or 0 on invalid input or I/O failure.
 */
int readInputFile(const char *inputPath, file_t *fileP);

/**
 * Assigns IDs to the selected records and sends each through file handling.
 *
 * @param fileInList Records selected for processing.
 * @param listLength Number of records in fileInList.
 * @param username Owner assigned to processed records.
 * @return Number of records successfully processed.
 */
int inputFiles(file_t fileInList[], int listLength, char username[]);

/**
 * Generates a random string of 14 characters for use as a file ID.
 *
 * @return Pointer to the generated identifier.
 */
char *generateFileId(void);

/**
 * Runs one record through compression, encryption, and storage.
 *
 * @param fileP Record to process.
 * @return Status indicating whether processing succeeded.
 */
int handleFileInput(file_t *fileP);

/**
 * Writes a record's data and metadata into their separate storage locations.
 *
 * @param fileP Record to store.
 * @return Status indicating whether storage succeeded.
 */
int storeFile(file_t *fileP);

/**
 * Writes the record's binary data to a file named from its ID.
 *
 * @param fileP Record whose bytes should be written.
 * @return Status indicating whether the write succeeded.
 */
int writeFile(file_t *fileP);

/**
 * Writes the record metadata in binary form.
 *
 * @param fileP Record whose metadata should be written.
 * @return Status indicating whether the write succeeded.
 */
int writeMetaData(file_t *fileP);
