#include "../../src/c/compression/codec.h"
#include "../../src/c/encryption/encryption.h"

#include <stdio.h>

/**
 * Verify packed PNG sample extraction across each supported bit depth.
 * @return Nonzero when every expected packed sample is extracted.
 */
static int samplesMatch(void) {
    const unsigned char oneBit[] = {0xb2};
    const unsigned char twoBit[] = {0x1b, 0xe4};
    const unsigned char fourBit[] = {0xab, 0xcd};
    const unsigned char eightBit[] = {0x00, 0xff};
    const unsigned char sixteenBit[] = {0x12, 0x34, 0xab, 0xcd};
    static const unsigned expectedOne[] = {1, 0, 1, 1, 0, 0, 1, 0};
    static const unsigned expectedTwo[] = {0, 1, 2, 3, 3, 2, 1, 0};
    static const unsigned expectedFour[] = {10, 11, 12, 13};
    size_t index;

    for (index = 0; index < sizeof(expectedOne) / sizeof(expectedOne[0]); index++)
        if (rcSample(oneBit, index, 1) != expectedOne[index])
            return 0;
    for (index = 0; index < sizeof(expectedTwo) / sizeof(expectedTwo[0]); index++)
        if (rcSample(twoBit, index, 2) != expectedTwo[index])
            return 0;
    for (index = 0; index < sizeof(expectedFour) / sizeof(expectedFour[0]); index++)
        if (rcSample(fourBit, index, 4) != expectedFour[index])
            return 0;
    return rcSample(eightBit, 1, 8) == 255 && rcSample(sixteenBit, 0, 16) == 0x1234 &&
           rcSample(sixteenBit, 1, 16) == 0xabcd;
}

/**
 * Verify all five PNG row filters reconstruct their original bytes.
 * @return Nonzero when filters zero through four reconstruct their source rows.
 */
static int filtersMatch(void) {
    unsigned char none[] = {1, 2, 3};
    unsigned char sub[] = {10, 10, 10};
    unsigned char up[] = {1, 2, 3};
    unsigned char average[] = {10, 10, 10};
    unsigned char paeth[] = {3, 4, 5};
    const unsigned char priorUp[] = {5, 6, 7};
    const unsigned char priorAverage[] = {2, 4, 6};
    const unsigned char priorPaeth[] = {10, 20, 30};
    unsigned char unchanged[] = {7, 8};

    if (!rcUnfilter(none, NULL, (RcUnfilterOptions){sizeof(none), 1, 0}) || none[0] != 1 || none[1] != 2 || none[2] != 3)
        return 0;
    if (!rcUnfilter(sub, NULL, (RcUnfilterOptions){sizeof(sub), 1, 1}) || sub[0] != 10 || sub[1] != 20 || sub[2] != 30)
        return 0;
    if (!rcUnfilter(up, priorUp, (RcUnfilterOptions){sizeof(up), 1, 2}) || up[0] != 6 || up[1] != 8 || up[2] != 10)
        return 0;
    if (!rcUnfilter(average, priorAverage, (RcUnfilterOptions){sizeof(average), 1, 3}) || average[0] != 11 ||
        average[1] != 17 || average[2] != 21)
        return 0;
    if (!rcUnfilter(paeth, priorPaeth, (RcUnfilterOptions){sizeof(paeth), 1, 4}) || paeth[0] != 13 || paeth[1] != 24 ||
        paeth[2] != 35)
        return 0;
    return !rcUnfilter(unchanged, NULL, (RcUnfilterOptions){sizeof(unchanged), 1, 5}) && unchanged[0] == 7 &&
           unchanged[1] == 8;
}

/**
 * Verify buffer growth, zero-length append, and the configured size limit.
 * @return Nonzero when appends preserve bytes and reject an append beyond the limit.
 */
static int appendRespectsBounds(void) {
    RcBuffer buffer = {0};
    RcBuffer atLimit = {NULL, RC_LIMIT, RC_LIMIT};
    const unsigned char bytes[] = {1, 2, 3};
    int success = rcAppend(&buffer, NULL, 0) && rcAppend(&buffer, bytes, sizeof(bytes)) &&
                  buffer.size == sizeof(bytes) && !memcmp(buffer.data, bytes, sizeof(bytes)) &&
                  !rcAppend(&atLimit, bytes, 1);

    free(buffer.data);
    return success;
}

/**
 * Verify a zlib round trip for the supplied byte sequence.
 * @param source Uncompressed source bytes; may be NULL when length is zero.
 * @param length Number of source bytes to compress and restore.
 * @return Nonzero when the restored bytes exactly match the source.
 */
static int deflateRoundTrips(const unsigned char *source, size_t length) {
    RcBuffer encoded = {0}, decoded = {0};
    int success = rcDeflate(source, length, &encoded) &&
                  rcInflate(encoded.data, encoded.size, &decoded) && decoded.size == length &&
                  (!length || !memcmp(decoded.data, source, length));

    free(encoded.data);
    free(decoded.data);
    return success;
}

/**
 * Reject broken zlib headers, truncated trailers, and incorrect Adler checksums.
 * @return Nonzero when each malformed stream is rejected.
 */
static int rejectsInvalidStreams(void) {
    const unsigned char source[] = "codec primitive fixture";
    const unsigned char invalidHeader[] = {0, 0, 0, 0, 0, 0};
    RcBuffer encoded = {0}, decoded = {0};
    size_t last;
    int success = !rcInflate(invalidHeader, sizeof(invalidHeader), &decoded) &&
                  rcDeflate(source, sizeof(source) - 1, &encoded);

    if (!success)
        goto cleanup;
    if (rcInflate(encoded.data, encoded.size - 1, &decoded)) {
        success = 0;
        goto cleanup;
    }
    last = encoded.size - 1;
    encoded.data[last] ^= 1;
    if (rcInflate(encoded.data, encoded.size, &decoded))
        success = 0;

cleanup:
    free(encoded.data);
    free(decoded.data);
    return success;
}

/**
 * Verify cipher initialization and reversible stream state across split buffers.
 * @return Nonzero when chunked encryption matches one-shot encryption and decrypts exactly.
 */
static int cipherRoundTrips(void) {
    const char source[] = "stream cipher fixture";
    char chunked[sizeof(source)];
    char wholeBuffer[sizeof(source)];
    streamcipher_t encryptor, decryptor, whole;

    memcpy(chunked, source, sizeof(source));
    memcpy(wholeBuffer, source, sizeof(source));
    if (initCipher(NULL, 1) != 1 || initCipher(&encryptor, 0) != 1 ||
        initCipher(&encryptor, 0x123456789abcdefULL) != 0 ||
        initCipher(&decryptor, 0x123456789abcdefULL) != 0 ||
        initCipher(&whole, 0x123456789abcdefULL) != 0)
        return 0;
    if (performCipher(&encryptor, chunked, 5) != 0 ||
        performCipher(&encryptor, chunked + 5, sizeof(chunked) - 5) != 0 ||
        performCipher(&whole, wholeBuffer, sizeof(wholeBuffer)) != 0 ||
        memcmp(chunked, wholeBuffer, sizeof(chunked)) ||
        memcmp(chunked, source, sizeof(chunked)) == 0 ||
        performCipher(&decryptor, chunked, sizeof(chunked)) != 0 ||
        memcmp(chunked, source, sizeof(chunked)) || performCipher(NULL, wholeBuffer, 1) != 1 ||
        performCipher(&whole, NULL, 1) != 1)
        return 0;
    return performCipher(&whole, wholeBuffer, 0) == 0;
}

/**
 * Run direct contract checks for the reusable codec and buffer primitives.
 * @return Zero when all primitive contracts pass, or one after the first failure.
 */
int main(void) {
    static const unsigned char repeated[] = "aaaaaaaaabbbbbbbbbccccccccc";
    static const unsigned char literals[] = {0, 1, 2, 3, 0xff, 0x10, 0x20};

    if (!samplesMatch()) {
        fputs("Packed PNG samples did not match.\n", stderr);
        return 1;
    }
    if (!filtersMatch()) {
        fputs("PNG row filters did not reconstruct the source.\n", stderr);
        return 1;
    }
    if (!appendRespectsBounds()) {
        fputs("Buffer append ignored its size contract.\n", stderr);
        return 1;
    }
    if (!deflateRoundTrips(NULL, 0) || !deflateRoundTrips(literals, sizeof(literals)) ||
        !deflateRoundTrips(repeated, sizeof(repeated) - 1)) {
        fputs("DEFLATE round-trip did not preserve bytes.\n", stderr);
        return 1;
    }
    if (!rejectsInvalidStreams()) {
        fputs("Inflate accepted an invalid zlib stream.\n", stderr);
        return 1;
    }
    if (!cipherRoundTrips()) {
        fputs("Cipher stream state did not preserve a reversible transform.\n", stderr);
        return 1;
    }
    return 0;
}
