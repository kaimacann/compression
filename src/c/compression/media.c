/**
 * @file media.c Format-aware processing and staged output transactions.
 * @author kai
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* SHARED CONTRACT*/
#include "media.h"
#include "codec.h"
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

/* PROCESSING STATE*/
enum {
    IO_BLOCK_SIZE = 8192,
    TEMP_PATH_OVERHEAD = 32,
    TEMP_FILE_ATTEMPTS = 1000,
    SIDECAR_PATH_OVERHEAD = 48,
    PDF_SIGNATURE_SIZE = 5,
    PNG_SIGNATURE_SIZE = 8,
    JPEG_SIGNATURE_SIZE = 2,
    JPEG_MARKER_PREFIX = 255,
    JPEG_MARKER_IMAGE = 216,
    MAX_PALETTE_COLORS = 256,
    DECIMAL_RADIX = 10,
    SINGLE_DIGIT_WIDTH = 1,
    SIDECAR_INDEX_WIDTH = 3,
};

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
} StringBuilder;
typedef struct {
    unsigned long value;
    size_t minimumDigits;
} UnsignedText;

typedef struct {
    char *path, *stage, *backup;
    const unsigned char *data;
    size_t length;
    int existing, committed, skip, saved;
} Output;
typedef enum {
    MEDIA_FORMAT_UNSUPPORTED,
    MEDIA_FORMAT_PDF,
    MEDIA_FORMAT_PNG,
    MEDIA_FORMAT_JPEG,
} MediaFormat;
typedef struct {
    RcBuffer source;
    RcBuffer candidate;
    RcBuffer *images;
    RcImage image;
    RcVector vector;
    const RcBuffer *result;
    MediaFormat format;
    const char *formatName;
    const char *statusName;
    size_t imageCount;
    size_t candidateSize;
    unsigned unsupported;
} MediaWork;

/**
 * Append a bounded text slice and maintain a NUL terminator.
 * @param builder Destination string builder.
 * @param text Source text to append.
 * @param textLength Number of source bytes to append.
 * @return Nonzero when the slice fits and is appended.
 * @author kai
 */
static int appendText(StringBuilder *builder, const char *text, size_t textLength) {
    size_t offset;
    if (builder->length >= builder->capacity || textLength >= builder->capacity - builder->length) {
        return 0;
    }
    for (offset = 0; offset < textLength; offset++) {
        builder->data[builder->length + offset] = text[offset];
    }
    builder->length += textLength;
    builder->data[builder->length] = '\0';
    return 1;
}

/**
 * Append an unsigned decimal value with a minimum number of digits.
 * @param builder Destination string builder.
 * @param number Unsigned value and minimum width, padded with leading zeroes.
 * @return Nonzero when the complete decimal representation is appended.
 * @author kai
 */
static int appendUnsigned(StringBuilder *builder, UnsignedText number) {
    char digits[sizeof(number.value) * CHAR_BIT];
    size_t digitCount = 0;
    do {
        digits[digitCount++] = (char)('0' + (number.value % DECIMAL_RADIX));
        number.value /= DECIMAL_RADIX;
    } while (number.value);
    while (digitCount < number.minimumDigits) {
        if (!appendText(builder, "0", 1)) {
            return 0;
        }
        number.minimumDigits--;
    }
    while (digitCount) {
        char digit[2] = {digits[--digitCount], '\0'};
        if (!appendText(builder, digit, 1)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Write a best-effort diagnostic without interpreting dynamic text as a format string.
 * @param prefix Fixed diagnostic text before the detail.
 * @param detail Dynamic diagnostic text.
 * @param suffix Fixed diagnostic text after the detail.
 * @author kai
 */
static void writeDiagnostic(const char *prefix, const char *detail, const char *suffix) {
    (void)fputs(prefix, stderr);
    (void)fputs(detail, stderr);
    (void)fputs(suffix, stderr);
}

/**
 * Write an unsigned value as best-effort decimal diagnostic text.
 * @param value Unsigned value to write.
 * @author kai
 */
static void writeUnsignedDiagnostic(unsigned long value) {
    char digits[sizeof(value) * CHAR_BIT];
    size_t digitCount = 0;
    do {
        digits[digitCount++] = (char)('0' + (value % DECIMAL_RADIX));
        value /= DECIMAL_RADIX;
    } while (value);
    while (digitCount) {
        (void)fputc(digits[--digitCount], stderr);
    }
}

/*//////////////////////////////////////////////////
FILES
//////////////////////////////////////////////////*/

/* INPUT BUFFERS*/
/**
 * Allocate and copy a NUL-terminated path or label.
 * @param text NUL-terminated text to duplicate.
 * @return Owned copy of the text, or NULL when allocation fails.
 * @author kai
 */
static char *copyText(const char *text) {
    size_t textLength = strlen(text) + 1;
    char *copy = malloc(textLength);
    if (copy) {
        size_t offset;
        for (offset = 0; offset < textLength; offset++) {
            copy[offset] = text[offset];
        }
    }
    return copy;
}
/**
 * Read a file into a bounded buffer and report read or close failures.
 * @param path Input file path to open in binary mode.
 * @param buffer Receives the bounded file contents.
 * @return Nonzero when the complete file is read and closed successfully.
 * @author kai
 */
static int readWhole(const char *path, RcBuffer *buffer) {
    FILE *file = fopen(path, "rb");
    unsigned char block[IO_BLOCK_SIZE];
    size_t bytesRead;
    int readOk = 1;
    if (!file) {
        perror(path);
        return 0;
    }
    do {
        bytesRead = fread(block, 1, sizeof(block), file);
        if (bytesRead && !rcAppend(buffer, block, bytesRead)) {
            readOk = 0;
            break;
        }
        if (ferror(file)) {
            readOk = 0;
            break;
        }
    } while (bytesRead == sizeof(block));
    if (readOk && !feof(file)) {
        readOk = 0;
    }
    if (fclose(file)) {
        readOk = 0;
    }
    if (!readOk) {
        (void)fputs("Could not read input within the ", stderr);
        writeUnsignedDiagnostic((unsigned long)RC_LIMIT);
        writeDiagnostic("-byte limit: ", path, "\n");
    }
    return readOk;
}
/**
 * Compare an open file with a byte sequence without loading the file into memory.
 * @param file Open file positioned at its first byte.
 * @param expected Expected bytes for comparison.
 * @param expectedLength Expected byte count.
 * @return Nonzero only when the file contains exactly the expected bytes.
 * @author kai
 */
static int fileEquals(FILE *file, const unsigned char *expected, size_t expectedLength) {
    unsigned char block[IO_BLOCK_SIZE];
    size_t offset = 0;
    size_t bytesRead;
    for (;;) {
        bytesRead = fread(block, 1, sizeof(block), file);
        if (offset > expectedLength || bytesRead > expectedLength - offset ||
            (bytesRead && memcmp(block, expected + offset, bytesRead) != 0)) {
            return 0;
        }
        offset += bytesRead;
        if (ferror(file)) {
            return 0;
        }
        if (feof(file)) {
            break;
        }
        if (!bytesRead) {
            return 0;
        }
    }
    return !ferror(file) && offset == expectedLength;
}

/* OUTPUT LIFETIME*/
/**
 * Create a collision-free staging or backup file beside the requested destination.
 * @param path Destination path used to form a sibling temporary name.
 * @param suffix Temporary-file purpose included in the name.
 * @param handle Receives the exclusively created file stream.
 * @return Owned temporary path, or NULL if allocation or exclusive creation fails.
 * @author kai
 */
static char *uniqueFile(const char *path, const char *suffix, FILE **handle) {
    size_t capacity = strlen(path) + strlen(suffix) + TEMP_PATH_OVERHEAD;
    char *name = malloc(capacity);
    unsigned attempt;
    if (!name) {
        return NULL;
    }
    for (attempt = 0; attempt < TEMP_FILE_ATTEMPTS; attempt++) {
        StringBuilder builder = {name, capacity, 0};
        if (!appendText(&builder, path, strlen(path)) || !appendText(&builder, ".", 1) ||
            !appendText(&builder, suffix, strlen(suffix)) || !appendText(&builder, "-", 1) ||
            !appendUnsigned(&builder, (UnsignedText){attempt, SINGLE_DIGIT_WIDTH})) {
            free(name);
            return NULL;
        }
        *handle = fopen(name, "wbx");
        if (*handle) {
            return name;
        }
    }
    free(name);
    return NULL;
}
/**
 * Remove temporary transaction files and release output records.
 * @param out Output records whose staged files and owned paths are released.
 * @param count Number of records in the array.
 * @return None.
 * @author kai
 */
static void cleanOutputs(Output *out, size_t count) {
    size_t index;
    if (!out) {
        return;
    }
    for (index = 0; index < count; index++) {
        if (out[index].stage) {
            (void)remove(out[index].stage);
        }
        if (out[index].backup) {
            (void)remove(out[index].backup);
        }
        free(out[index].path);
        free(out[index].stage);
        free(out[index].backup);
    }
    free(out);
}

/*//////////////////////////////////////////////////
TRANSACTIONS
//////////////////////////////////////////////////*/

/* STAGING AND ROLLBACK*/
typedef struct {
    Output *outputs;
    size_t count;
    const unsigned char *source;
    size_t sourceLength;
    int force;
} OutputBatch;

/**
 * Validate one destination for duplicate paths and source aliases.
 * @param batch Destinations and source data for the output transaction.
 * @param outputIndex Destination currently being validated.
 * @return Nonzero when the destination is safe to stage.
 * @author kai
 */
static int validateOutputTarget(const OutputBatch *batch, size_t outputIndex) {
    size_t previousIndex;
    Output *output = &batch->outputs[outputIndex];
    struct stat metadata;
    for (previousIndex = 0; previousIndex < outputIndex; previousIndex++) {
        if (!strcmp(output->path, batch->outputs[previousIndex].path)) {
            writeDiagnostic("Duplicate output path: ", output->path, "\n");
            return 0;
        }
    }
    if (lstat(output->path, &metadata)) {
        if (errno != ENOENT) {
            perror(output->path);
            return 0;
        }
        return 1;
    }
    if (!S_ISREG(metadata.st_mode)) {
        writeDiagnostic("Refusing to replace a non-regular output target: ", output->path, "\n");
        return 0;
    }
    FILE *existing = fopen(output->path, "rb");
    if (!existing) {
        perror(output->path);
        return 0;
    }
    output->existing = 1;
    if (!batch->force) {
        (void)fclose(existing);
        writeDiagnostic("Output already exists: ", output->path,
                        "\nUse another output path or add --force to replace derived outputs.\n");
        return 0;
    }

    /* An identical unchanged copy needs no replacement; differing derived data must use another
     * path.*/
    int matchesSource = fileEquals(existing, batch->source, batch->sourceLength);
    if (fclose(existing)) {
        return 0;
    }
    if (matchesSource) {
        if (output->length == batch->sourceLength &&
            !memcmp(output->data, batch->source, batch->sourceLength)) {
            output->skip = 1;
        } else {
            (void)fputs("Refusing to replace an output that matches the source; choose a fresh "
                        "destination.\n",
                        stderr);
            return 0;
        }
    }
    return 1;
}

/**
 * Validate every destination before creating any staging file.
 * @param batch Destinations and source data for the output transaction.
 * @return Nonzero when all destinations are safe to stage.
 * @author kai
 */
static int validateOutputTargets(const OutputBatch *batch) {
    size_t outputIndex;
    for (outputIndex = 0; outputIndex < batch->count; outputIndex++) {
        if (!validateOutputTarget(batch, outputIndex)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Write and close one staged output before committing any destination.
 * @param output Output record to stage.
 * @return Nonzero when the complete staged payload is written and closed.
 * @author kai
 */
static int stageOutput(Output *output) {
    FILE *stage;
    if (output->skip) {
        return 1;
    }
    output->stage = uniqueFile(output->path, "processing", &stage);
    if (!output->stage) {
        writeDiagnostic("Cannot stage output beside ", output->path,
                        "; check its parent directory and permissions.\n");
        return 0;
    }
    int written = fwrite(output->data, 1, output->length, stage) == output->length;
    if (fclose(stage)) {
        written = 0;
    }
    if (!written) {
        writeDiagnostic("Failed to write staged output: ", output->path, "\n");
    }
    return written;
}

/**
 * Stage every payload before moving any prior destination.
 * @param batch Destinations and source data for the output transaction.
 * @return Nonzero when every staged payload is ready to commit.
 * @author kai
 */
static int stageOutputs(const OutputBatch *batch) {
    size_t outputIndex;
    for (outputIndex = 0; outputIndex < batch->count; outputIndex++) {
        if (!stageOutput(&batch->outputs[outputIndex])) {
            return 0;
        }
    }
    return 1;
}

/**
 * Move an existing destination to a recoverable backup path.
 * @param output Output record with an existing destination.
 * @return Nonzero when the original destination has been saved.
 * @author kai
 */
static int saveExistingOutput(Output *output) {
    FILE *placeholder;
    output->backup = uniqueFile(output->path, "previous", &placeholder);
    if (!output->backup) {
        return 0;
    }
    if (fclose(placeholder) || remove(output->backup) || rename(output->path, output->backup)) {
        return 0;
    }
    output->saved = 1;
    return 1;
}

/**
 * Reserve a new destination so rollback can remove it after a later commit failure.
 * @param output Output record with a new destination.
 * @return Nonzero when the destination is exclusively created and closed.
 * @author kai
 */
static int reserveOutput(Output *output) {
    FILE *reserve = fopen(output->path, "wbx");
    if (!reserve) {
        return 0;
    }
    output->committed = 1;
    return fclose(reserve) == 0;
}

/**
 * Restore committed outputs in reverse order and retain backups that cannot be restored.
 * @param outputs Output records whose committed files must be rolled back.
 * @param lastOutputIndex Last output reached by the commit loop.
 * @author kai
 */
static void rollbackOutputs(Output *outputs, size_t lastOutputIndex) {
    size_t outputIndex = lastOutputIndex + 1;
    while (outputIndex > 0) {
        Output *output = &outputs[--outputIndex];
        if (output->committed) {
            (void)remove(output->path);
        }
        if (output->saved && rename(output->backup, output->path)) {
            writeDiagnostic("Previous output is retained at ", output->backup,
                            "; automatic restore failed.\n");
            free(output->backup);
            output->backup = NULL;
        }
    }
}

/**
 * Commit staged outputs and restore backups if a commit step fails.
 * @param batch Destinations and source data for the output transaction.
 * @return Nonzero when every destination is committed.
 * @author kai
 */
static int commitOutputs(const OutputBatch *batch) {
    size_t outputIndex;
    for (outputIndex = 0; outputIndex < batch->count; outputIndex++) {
        Output *output = &batch->outputs[outputIndex];
        if (output->skip) {
            continue;
        }
        if (output->existing ? !saveExistingOutput(output) : !reserveOutput(output)) {
            goto rollback;
        }
        if (rename(output->stage, output->path)) {
            perror(output->path);
            goto rollback;
        }
        output->committed = 1;
    }
    return 1;
rollback:
    rollbackOutputs(batch->outputs, outputIndex);
    return 0;
}

/**
 * Validate, stage, and commit the complete output set.
 * @param batch Destinations and source data for the output transaction.
 * @return Nonzero when all outputs are committed or safely skipped.
 * @author kai
 */
static int writeOutputs(const OutputBatch *batch) {
    return validateOutputTargets(batch) && stageOutputs(batch) && commitOutputs(batch);
}

/*//////////////////////////////////////////////////
PROCESSING
//////////////////////////////////////////////////*/

/* SIDECAR NAMES*/
/**
 * Build the numbered SVG sidecar path beside the PDF or in the requested directory.
 * @param output Processed PDF path used as the default sidecar base.
 * @param number One-based image index used in the sidecar filename.
 * @param directory Optional directory for all SVG sidecars.
 * @return Owned sidecar path, or NULL when allocation fails.
 * @author kai
 */
static char *imagePath(const char *output, size_t number, const char *directory) {
    const char *base = output;
    const char *cursor;
    const char *dot;
    size_t dirLength = 0;
    size_t directoryLength = 0;
    size_t baseLength;
    size_t capacity;
    char *result;
    for (cursor = output; *cursor; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            base = cursor + 1;
            dirLength = (size_t)(cursor - output) + 1;
        }
    }
    dot = strrchr(base, '.');
    baseLength = dot ? (size_t)(dot - base) : strlen(base);
    if (directory) {
        directoryLength = strlen(directory);
    }
    capacity = (directory ? directoryLength : dirLength) + baseLength + SIDECAR_PATH_OVERHEAD;
    result = malloc(capacity);
    if (!result) {
        return NULL;
    }
    StringBuilder builder = {result, capacity, 0};
    int pathOk;
    if (directory) {
        pathOk = appendText(&builder, directory, directoryLength) && appendText(&builder, "/", 1) &&
                 appendText(&builder, base, baseLength);
    } else {
        pathOk = appendText(&builder, output, dirLength) && appendText(&builder, base, baseLength);
    }
    if (!pathOk || !appendText(&builder, "-image-", strlen("-image-")) ||
        !appendUnsigned(&builder, (UnsignedText){(unsigned long)number, SIDECAR_INDEX_WIDTH}) ||
        !appendText(&builder, ".svg", strlen(".svg"))) {
        free(result);
        return NULL;
    }
    return result;
}

/* FORMAT ROUTING*/
/**
 * Identify media from its signature rather than its filename extension.
 * @param data Source bytes to inspect.
 * @param length Source byte count.
 * @return The recognized PDF, PNG, or JPEG format, or the unsupported format marker.
 * @author kai
 */
static MediaFormat detectMediaFormat(const unsigned char *data, size_t length) {
    if (length >= PDF_SIGNATURE_SIZE && !memcmp(data, "%PDF-", PDF_SIGNATURE_SIZE)) {
        return MEDIA_FORMAT_PDF;
    }
    if (length >= PNG_SIGNATURE_SIZE && !memcmp(data, "\211PNG\r\n\032\n", PNG_SIGNATURE_SIZE)) {
        return MEDIA_FORMAT_PNG;
    }
    if (length >= JPEG_SIGNATURE_SIZE && data[0] == JPEG_MARKER_PREFIX &&
        data[1] == JPEG_MARKER_IMAGE) {
        return MEDIA_FORMAT_JPEG;
    }
    return MEDIA_FORMAT_UNSUPPORTED;
}

/**
 * Validate user-selected media processing options.
 * @param input Source path.
 * @param output Destination path.
 * @param options Processing mode and output policy.
 * @return Nonzero when all required values are valid.
 * @author kai
 */
static int validMediaRequest(const char *input, const char *output, const MediaOptions *options) {
    if (!input || !output || !options || !input[0] || !output[0] || !strcmp(input, output) ||
        options->colors < 2 || options->colors > MAX_PALETTE_COLORS ||
        !isfinite(options->tolerance) || options->tolerance < 0 || options->tolerance > 2) {
        (void)fputs("Input and a distinct output, 2..256 colours, and a tolerance from 0 to 2 are "
                    "required.\n",
                    stderr);
        return 0;
    }
    return 1;
}

/**
 * Create and select the candidate representation for a PDF source.
 * @param work Owned state for the current media operation.
 * @param options Processing mode and output policy.
 * @return Nonzero when the PDF can be processed or preserved.
 * @author kai
 */
static int preparePdf(MediaWork *work, const MediaOptions *options) {
    if (!processPdf(work->source.data, work->source.size, options, &work->candidate, &work->images,
                    &work->imageCount, &work->unsupported)) {
        (void)fputs("Malformed PDF or image processing/resource failure.\n", stderr);
        return 0;
    }
    work->candidateSize = work->candidate.size ? work->candidate.size : work->source.size;
    if (work->candidate.size && work->candidate.size < work->source.size) {
        work->result = &work->candidate;
        work->statusName = work->unsupported ? "smaller-partial" : "smaller";
    } else if (work->unsupported) {
        work->statusName = "unchanged-unsupported";
    } else if (options->vectorMode) {
        work->statusName = "unchanged-no-size-reduction";
    } else {
        work->statusName = "unchanged-lossless";
    }
    return 1;
}

/**
 * Decode and select the lossless or vector candidate for a raster source.
 * @param work Owned state for the current media operation.
 * @param options Processing mode and output policy.
 * @return Nonzero when the raster image can be processed.
 * @author kai
 */
static int prepareRaster(MediaWork *work, const MediaOptions *options) {
    if (!rcDecode(work->source.data, work->source.size, &work->image)) {
        (void)fputs("Invalid or unsupported PNG/JPEG encoding, or image exceeds resource limits.\n",
                    stderr);
        return 0;
    }
    if (options->vectorMode) {
        if (!rcVectorize(&work->image, (RcVectorOptions){options->colors, options->tolerance},
                         &work->vector)) {
            (void)fputs("Image vectorization exceeded resources or encountered invalid geometry.\n",
                        stderr);
            return 0;
        }
        work->result = &work->vector.svg;
        work->statusName =
            work->result->size < work->source.size ? "derived-smaller" : "derived-larger";
    } else {
        work->statusName = "unchanged-lossless";
    }
    work->candidateSize = work->result->size;
    return 1;
}

/**
 * Populate the primary and sidecar outputs for one processing result.
 * @param output Primary output path.
 * @param options Processing mode and sidecar policy.
 * @param work Owned state for the current media operation.
 * @param outputs Receives the allocated output records.
 * @param outputCount Receives the number of output records.
 * @return Nonzero when all output paths are available.
 * @author kai
 */
static int createOutputs(const char *output, const MediaOptions *options, const MediaWork *work,
                         Output **outputs, size_t *outputCount) {
    size_t imageIndex;
    *outputCount = 1 + work->imageCount;
    *outputs = calloc(*outputCount, sizeof(**outputs));
    if (!*outputs) {
        return 0;
    }
    (*outputs)[0].path = copyText(output);
    (*outputs)[0].data = work->result->data;
    (*outputs)[0].length = work->result->size;
    if (!(*outputs)[0].path) {
        return 0;
    }
    for (imageIndex = 0; imageIndex < work->imageCount; imageIndex++) {
        (*outputs)[imageIndex + 1].path =
            imagePath(output, imageIndex + 1, options->imagesDirectory);
        (*outputs)[imageIndex + 1].data = work->images[imageIndex].data;
        (*outputs)[imageIndex + 1].length = work->images[imageIndex].size;
        if (!(*outputs)[imageIndex + 1].path) {
            return 0;
        }
    }
    return 1;
}

/**
 * Print the result summary and format-specific processing details.
 * @param work Completed processing state.
 * @param options Processing mode and output policy.
 * @param outputs Committed output records.
 * @param outputCount Number of committed output records.
 * @param format Source media format name.
 * @author kai
 */
static void reportMediaResult(const MediaWork *work, const MediaOptions *options,
                              const Output *outputs, size_t outputCount, const char *format) {
    (void)printf("format=%s mode=%s status=%s\n", format,
                 options->vectorMode ? "vector" : "lossless", work->statusName);
    (void)printf(
        "source=%lu bytes crc32=%08lX; output=%lu bytes crc32=%08lX; candidate=%lu bytes\n",
        (unsigned long)work->source.size, rcCrc(work->source.data, work->source.size),
        (unsigned long)work->result->size, rcCrc(work->result->data, work->result->size),
        (unsigned long)work->candidateSize);
    if (work->image.rgba) {
        (void)printf("dimensions=%u x %u; palette=%u; tolerance=%.3g pixels\n", work->image.width,
                     work->image.height, options->vectorMode ? work->vector.colors : 0,
                     options->tolerance);
    }
    if (work->format == MEDIA_FORMAT_PDF) {
        (void)printf("processed-images=%lu unsupported-features-or-images=%u; PDF text and page "
                     "objects are retained\n",
                     (unsigned long)work->imageCount, work->unsupported);
    }
    for (size_t outputIndex = 0; outputIndex < outputCount; outputIndex++) {
        (void)printf("Wrote %s (%lu bytes)\n", outputs[outputIndex].path,
                     (unsigned long)outputs[outputIndex].length);
    }
}

/**
 * Release all image, vector, and output data owned by one media operation.
 * @param work Processing state to clear.
 * @param outputs Output records to release.
 * @param outputCount Number of allocated output records.
 * @author kai
 */
static void cleanMediaWork(MediaWork *work, Output *outputs, size_t outputCount) {
    cleanOutputs(outputs, outputCount);
    free(work->source.data);
    free(work->candidate.data);
    free(work->image.rgba);
    rcFreeVector(&work->vector);
    for (size_t imageIndex = 0; imageIndex < work->imageCount; imageIndex++) {
        free(work->images[imageIndex].data);
    }
    free(work->images);
}

/**
 * Validate the input format, apply the selected raster or vector pipeline, and commit outputs.
 * @param input PNG, JPEG, or PDF source path.
 * @param output Destination path for the selected output.
 * @param options Processing mode, palette, contour, and replacement policy.
 * @return Nonzero when processing and output commit succeed; zero on validation or write failure.
 * @author kai
 */
int processMedia(const char *input, const char *output, const MediaOptions *options) {
    MediaWork work = {0};
    Output *outputs = NULL;
    size_t outputCount = 0;
    int success = 0;

    if (!validMediaRequest(input, output, options)) {
        return 0;
    }
    work.result = &work.source;
    if (!readWhole(input, &work.source) || !rcAppend(&work.source, "", 1)) {
        goto done;
    }
    work.source.size--;
    MediaFormat format = detectMediaFormat(work.source.data, work.source.size);
    work.format = format;
    if (format == MEDIA_FORMAT_UNSUPPORTED) {
        (void)fputs("Unsupported input: expected PNG, JPEG, or PDF file contents.\n", stderr);
        goto done;
    }
    if (format == MEDIA_FORMAT_PDF) {
        work.formatName = "PDF";
        if (!preparePdf(&work, options)) {
            goto done;
        }
    } else {
        work.formatName = format == MEDIA_FORMAT_PNG ? "PNG" : "JPEG";
        if (!prepareRaster(&work, options)) {
            goto done;
        }
    }

    if (!createOutputs(output, options, &work, &outputs, &outputCount)) {
        goto done;
    }

    OutputBatch batch = {outputs, outputCount, work.source.data, work.source.size, options->force};
    if (!writeOutputs(&batch)) {
        goto done;
    }
    reportMediaResult(&work, options, outputs, outputCount, work.formatName);
    success = 1;
done:
    cleanMediaWork(&work, outputs, outputCount);
    return success;
}
