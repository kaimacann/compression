/** @file media.c Format-aware orchestration and staged output transactions. @author kai */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// SHARED CONTRACT
#include "media.h"

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

// PROCESSING STATE
typedef struct {
    char *path, *stage, *backup;
    const unsigned char *data;
    size_t length;
    int existing, committed, skip, saved;
} Output;

/*//////////////////////////////////////////////////
FILES
//////////////////////////////////////////////////*/

// INPUT BUFFERS
/** Allocate an owned copy of an output path. @author kai */
static char *copyText(const char *p) {
    size_t n = strlen(p) + 1;
    char *s = malloc(n);
    if (s)
        memcpy(s, p, n);
    return s;
}
/** Read a file within the processing byte limit. @author kai */
static int readWhole(const char *path, RcBuffer *b) {
    FILE *f = fopen(path, "rb");
    unsigned char block[8192];
    size_t n;
    int ok = 1;
    if (!f) {
        perror(path);
        return 0;
    }
    while ((n = fread(block, 1, sizeof(block), f)) != 0)
        if (!rcAppend(b, block, n)) {
            ok = 0;
            break;
        }
    if (ferror(f))
        ok = 0;
    if (fclose(f))
        ok = 0;
    if (!ok)
        fprintf(stderr, "Could not read input within the %lu-byte limit: %s\n",
                (unsigned long)RC_LIMIT, path);
    return ok;
}
/** Compare an open destination with source bytes to protect source aliases. @author kai */
static int fileEquals(FILE *f, const unsigned char *p, size_t n) {
    unsigned char block[8192];
    size_t at = 0, k;
    while ((k = fread(block, 1, sizeof(block), f)) != 0) {
        if (k > n - at || memcmp(block, p + at, k))
            return 0;
        at += k;
    }
    return !ferror(f) && at == n;
}

// OUTPUT LIFETIME
/** Reserve a sibling staging filename with exclusive creation. @author kai */
static char *uniqueFile(const char *path, const char *suffix, FILE **handle) {
    size_t n = strlen(path) + strlen(suffix) + 32;
    char *name = malloc(n);
    unsigned i;
    if (!name)
        return NULL;
    for (i = 0; i < 1000; i++) {
        snprintf(name, n, "%s.%s-%u", path, suffix, i);
        *handle = fopen(name, "wbx");
        if (*handle)
            return name;
    }
    free(name);
    return NULL;
}
/** Remove owned temporary files and release transaction paths. @author kai */
static void cleanOutputs(Output *out, size_t count) {
    size_t i;
    if (!out)
        return;
    for (i = 0; i < count; i++) {
        if (out[i].stage)
            remove(out[i].stage);
        if (out[i].backup)
            remove(out[i].backup);
        free(out[i].path);
        free(out[i].stage);
        free(out[i].backup);
    }
    free(out);
}

/*//////////////////////////////////////////////////
TRANSACTIONS
//////////////////////////////////////////////////*/

// STAGING AND ROLLBACK
/** Commit a group of outputs only after every staged write succeeds.
 * Prior destinations remain recoverable until all replacements commit.
 * @return 0 after rollback; failed restoration leaves a reported backup path.
 * @author kai
 */
static int writeOutputs(Output *out, size_t count, const unsigned char *source, size_t sourceLength,
                        int force) {
    size_t i, j;

    // Validate the complete destination set before creating staging files.

    // PREFLIGHT THE DESTINATION SET
    for (i = 0; i < count; i++) {
        FILE *existing;
        for (j = 0; j < i; j++)
            if (!strcmp(out[i].path, out[j].path)) {
                fprintf(stderr, "Duplicate output path: %s\n", out[i].path);
                return 0;
            }
        existing = fopen(out[i].path, "rb");
        if (existing) {
            out[i].existing = 1;
            if (!force) {
                fclose(existing);
                fprintf(stderr,
                        "Output already exists: %s\nUse another output path or add --force to "
                        "replace derived outputs.\n",
                        out[i].path);
                return 0;
            }

            // PROTECT SOURCE ALIASES
            // An identical unchanged copy needs no replacement; differing derived data must use
            // another path.
            if (fileEquals(existing, source, sourceLength)) {
                if (out[i].length == sourceLength && !memcmp(out[i].data, source, sourceLength))
                    out[i].skip = 1;
                else {
                    fclose(existing);
                    fputs("Refusing to replace an output that matches the source; choose a fresh "
                          "destination.\n",
                          stderr);
                    return 0;
                }
            }
            fclose(existing);
        }
    }

    // Stage every payload before moving any prior destination.

    // WRITE ALL STAGED PAYLOADS
    for (i = 0; i < count; i++) {
        FILE *stage;
        int written;
        if (out[i].skip)
            continue;
        out[i].stage = uniqueFile(out[i].path, "processing", &stage);
        if (!out[i].stage) {
            fprintf(stderr,
                    "Cannot stage output beside %s; check its parent directory and permissions.\n",
                    out[i].path);
            return 0;
        }
        written = fwrite(out[i].data, 1, out[i].length, stage) == out[i].length;
        if (fclose(stage))
            written = 0;
        if (!written) {
            fprintf(stderr, "Failed to write staged output: %s\n", out[i].path);
            return 0;
        }
    }

    // Retain backups until the whole output set has committed.

    // COMMIT WITH RECOVERABLE BACKUPS
    for (i = 0; i < count; i++) {
        if (out[i].skip)
            continue;
        if (out[i].existing) {
            FILE *placeholder;
            out[i].backup = uniqueFile(out[i].path, "previous", &placeholder);
            if (!out[i].backup)
                goto rollback;
            if (fclose(placeholder) || remove(out[i].backup) || rename(out[i].path, out[i].backup))
                goto rollback;
            out[i].saved = 1;
        } else {
            FILE *reserve = fopen(out[i].path, "wbx");
            if (!reserve)
                goto rollback;
            out[i].committed = 1;
            if (fclose(reserve))
                goto rollback;
        }
        if (rename(out[i].stage, out[i].path)) {
            perror(out[i].path);
            goto rollback;
        }
        out[i].committed = 1;
    }
    return 1;
rollback:
    // ROLL BACK IN REVERSE COMMIT ORDER
    // Keep a backup available and report its location if automatic restoration fails.
    for (j = i + 1; j > 0; j--) {
        Output *o = &out[j - 1];
        if (o->committed)
            remove(o->path);
        if (o->saved && rename(o->backup, o->path)) {
            fprintf(stderr, "Previous output is retained at %s; automatic restore failed.\n",
                    o->backup);
            free(o->backup);
            o->backup = NULL;
        }
    }
    return 0;
}

/*//////////////////////////////////////////////////
PROCESSING
//////////////////////////////////////////////////*/

// SIDECAR NAMES
/** Derive one SVG sidecar path beside the PDF or in the selected directory. @author kai */
static char *imagePath(const char *output, const char *directory, size_t number) {
    const char *base = output, *p, *dot;
    size_t dirLength = 0, baseLength, capacity;
    char *result;
    for (p = output; *p; p++)
        if (*p == '/' || *p == '\\') {
            base = p + 1;
            dirLength = (size_t)(p - output) + 1;
        }
    dot = strrchr(base, '.');
    baseLength = dot ? (size_t)(dot - base) : strlen(base);
    if (directory)
        dirLength = strlen(directory);
    capacity = dirLength + baseLength + 48;
    result = malloc(capacity);
    if (!result)
        return NULL;
    if (directory)
        snprintf(result, capacity, "%s/%.*s-image-%03lu.svg", directory, (int)baseLength, base,
                 (unsigned long)number);
    else
        snprintf(result, capacity, "%.*s%.*s-image-%03lu.svg", (int)dirLength, output,
                 (int)baseLength, base, (unsigned long)number);
    return result;
}

// FORMAT ROUTING
/** Preserve the source and commit a selected media representation.
 * Vector mode explicitly permits palette and contour approximation.
 * @return 1 after committing all outputs; 0 after validation or transaction failure.
 * @author kai
 */
int processMedia(const char *input, const char *output, const MediaOptions *options) {
    RcBuffer source = {0}, candidate = {0}, *images = NULL;
    const RcBuffer *result = &source;
    RcImage image = {0};
    RcVector vector = {0};
    Output *outputs = NULL;
    size_t imageCount = 0, count = 0, i, candidateSize = 0;
    unsigned unsupported = 0;
    const char *format, *status;
    int pdf, isPng, isJpeg, ok = 0;

    // VALIDATE THE REQUEST
    if (!input || !output || !options || !input[0] || !output[0] || !strcmp(input, output) ||
        options->colors < 2 || options->colors > 256 || !isfinite(options->tolerance) ||
        options->tolerance < 0 || options->tolerance > 2) {
        fputs("Input and a distinct output, 2..256 colours, and a tolerance from 0 to 2 are "
              "required.\n",
              stderr);
        return 0;
    }

    // READ AND IDENTIFY THE SOURCE
    // Detection uses file signatures rather than the supplied filename extension.
    // A sentinel bounds strtod in PDF dictionaries without becoming logical file data.
    if (!readWhole(input, &source) || !rcAppend(&source, "", 1))
        goto done;
    source.size--;
    pdf = source.size >= 5 && !memcmp(source.data, "%PDF-", 5);
    isPng = source.size >= 8 && !memcmp(source.data, "\211PNG\r\n\032\n", 8);
    isJpeg = source.size >= 2 && source.data[0] == 255 && source.data[1] == 216;
    if (!pdf && !isPng && !isJpeg) {
        fputs("Unsupported input: expected PNG, JPEG, or PDF file contents.\n", stderr);
        goto done;
    }
    format = pdf ? "PDF" : isPng ? "PNG" : "JPEG";

    // Select a candidate before deciding whether the primary PDF shrinks.

    // PROCESS THE SELECTED FORMAT
    if (pdf) {
        if (!processPdf(source.data, source.size, options, &candidate, &images, &imageCount,
                        &unsupported)) {
            fputs("Malformed PDF or image processing/resource failure.\n", stderr);
            goto done;
        }
        candidateSize = candidate.size ? candidate.size : source.size;

        // SELECT THE PRIMARY PDF REPRESENTATION
        // Borrow source bytes for preservation; only a smaller rebuilt candidate is selected.
        if (candidate.size && candidate.size < source.size) {
            result = &candidate;
            status = unsupported ? "smaller-partial" : "smaller";
        } else
            status = unsupported           ? "unchanged-unsupported"
                     : options->vectorMode ? "unchanged-no-size-reduction"
                                           : "unchanged-lossless";
    } else {
        if (!rcDecode(source.data, source.size, &image)) {
            fputs("Invalid or unsupported PNG/JPEG encoding, or image exceeds resource limits.\n",
                  stderr);
            goto done;
        }
        if (options->vectorMode) {
            if (!rcVectorize(&image, options->colors, options->tolerance, &vector)) {
                fputs("Image vectorization exceeded resources or encountered invalid geometry.\n",
                      stderr);
                goto done;
            }
            result = &vector.svg;
            status = result->size < source.size ? "derived-smaller" : "derived-larger";
        } else
            status = "unchanged-lossless";
        candidateSize = result->size;
    }

    // Collect primary and sidecar outputs into one transaction.

    // BUILD THE OUTPUT TRANSACTION
    count = 1 + imageCount;
    outputs = calloc(count, sizeof(*outputs));
    if (!outputs)
        goto done;
    outputs[0].path = copyText(output);
    outputs[0].data = result->data;
    outputs[0].length = result->size;
    if (!outputs[0].path)
        goto done;
    for (i = 0; i < imageCount; i++) {
        outputs[i + 1].path = imagePath(output, options->imagesDirectory, i + 1);
        outputs[i + 1].data = images[i].data;
        outputs[i + 1].length = images[i].size;
        if (!outputs[i + 1].path)
            goto done;
    }

    // COMMIT AND REPORT THE RESULT
    if (!writeOutputs(outputs, count, source.data, source.size, options->force))
        goto done;
    printf("format=%s mode=%s status=%s\n", format, options->vectorMode ? "vector" : "lossless",
           status);
    printf("source=%lu bytes crc32=%08lX; output=%lu bytes crc32=%08lX; candidate=%lu bytes\n",
           (unsigned long)source.size, rcCrc(source.data, source.size), (unsigned long)result->size,
           rcCrc(result->data, result->size), (unsigned long)candidateSize);

    // REPORT FORMAT-SPECIFIC METADATA
    // Printed sizes and checksums describe the selected representation, including unchanged
    // fallbacks.
    if (!pdf)
        printf("dimensions=%u x %u; palette=%u; tolerance=%.3g pixels\n", image.width, image.height,
               options->vectorMode ? vector.colors : 0, options->tolerance);
    if (pdf)
        printf("processed-images=%lu unsupported-features-or-images=%u; PDF text and page objects "
               "are retained\n",
               (unsigned long)imageCount, unsupported);
    for (i = 0; i < count; i++)
        printf("Wrote %s (%lu bytes)\n", outputs[i].path, (unsigned long)outputs[i].length);
    ok = 1;
done:
    // RELEASE OWNED BUFFERS
    cleanOutputs(outputs, count);
    free(source.data);
    free(candidate.data);
    free(image.rgba);
    rcFreeVector(&vector);
    for (i = 0; i < imageCount; i++)
        free(images[i].data);
    free(images);
    return ok;
}
