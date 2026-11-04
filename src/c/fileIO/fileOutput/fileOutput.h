#include "../file.c"

/**
 * Report the current saved-file browsing placeholder result.
 * @return 0 because saved-file browsing is not implemented.
 * @author haadi
 */
int browseFiles(void);

/**
 * Display a page of search results.
 * @param fileList Files to display.
 * @param totalFiles Number of entries in fileList.
 * @param filesPerPage Maximum entries per page.
 * @author haadi
 */
void displaySearch(file_t fileList[], int totalFiles, int filesPerPage);

/**
 * Load saved metadata into a file list.
 * @param fileList Destination for loaded records.
 * @return Number of records loaded.
 * @author haadi
 */
int readMetadata(file_t fileList[]);

/**
 * Read a query and update the search results.
 * @param fileListp Records to sort or filter.
 * @param totalFiles Number of records; updated when the list changes.
 * @author haadi
 */
void modifySearch(file_t fileListp[], int *totalFiles);

/**
 * Display command help.
 * @author haadi
 */
void displayHelpMenu(void);

/**
 * Apply the requested sort operation.
 * @param sort Sort selector from the query.
 * @param fileList Records to reorder.
 * @param totalFiles Number of records.
 * @author haadi
 */
void sort(char sort, file_t fileList[], int totalFiles);

/**
 * Sort records by filename.
 * @param fileList Records to reorder.
 * @param totalFiles Number of records.
 * @param isReversed Nonzero for reverse order.
 * @author haadi
 */
void sortAlphabetical(file_t fileList[], int totalFiles, int isReversed);

/**
 * Sort records by file size.
 * @param fileList Records to reorder.
 * @param totalFiles Number of records.
 * @param isReversed Nonzero for reverse order.
 * @author haadi
 */
void sortSize(file_t fileList[], int totalFiles, int isReversed);

/**
 * Apply a filter expression to the record list.
 * @param filter Filter expression from the query.
 * @param fileList Records to filter.
 * @param totalFiles Number of records; updated when entries are removed.
 * @author haadi
 */
void filter(char filter[], file_t fileList[], int *totalFiles);

/**
 * Keep records whose filenames contain the search text.
 * @param search Text to find in each filename.
 * @param fileList Records to inspect.
 * @param totalFiles Number of records.
 * @author haadi
 */
void fileIncludes(char search[], file_t fileList[], int totalFiles);

/**
 * Keep records owned by the requested user.
 * @param ownerName Owner name to match.
 * @param fileList Records to inspect.
 * @param totalFiles Number of records.
 * @author haadi
 */
void filterOwner(char ownerName[], file_t fileList[], int totalFiles);

/**
 * Keep records matching the requested file type.
 * @param fileType Type selector: d for document or i for image.
 * @param fileList Records to inspect.
 * @param totalFiles Number of records.
 * @author haadi
 */
void filterFileType(char fileType, file_t fileList[], int totalFiles);

/**
 * Process the selected records for output.
 * @param selectedFileList Records selected for output.
 * @param totalSelected Number of selected records.
 * @return Status indicating whether output succeeded.
 * @author haadi
 */
int outputFiles(file_t selectedFileList[], int totalSelected);

/**
 * Restore and write one selected record.
 * @param file Record to restore.
 * @param outputFolderAdr Destination folder path.
 * @return Status indicating whether output succeeded.
 * @author haadi
 */
int handleFileOutput(file_t *file, char outputFolderAdr[]);

/**
 * Read a record's stored binary data.
 * @param pointer Record whose data is populated.
 * @return Status indicating whether the read succeeded.
 * @author haadi
 */
int readFileBinary(file_t *pointer);

/**
 * Write a complete byte buffer to a destination file.
 * @param outputPath Destination path for the generated data.
 * @param data Bytes to write; NULL is valid only when dataSize is zero.
 * @param dataSize Number of bytes in data.
 * @return 1 when the full buffer is written, or 0 on invalid input or I/O failure.
 * @author haadi
 */
int writeOutputFile(const char *outputPath, const unsigned char *data, size_t dataSize);
