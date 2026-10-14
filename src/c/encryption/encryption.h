#ifndef CIPHER_H
#define CIPHER_H

#include <stdlib.h>

/* FILE ENCRYPTION — educational stream-cipher interface. */

#define PRNG_MULTIPLIER 2685821657736338717ULL

/* Mutable state for the xorshift64* byte stream. */
typedef struct streamcipher {
    unsigned long long state;
    unsigned long long random_value;
    unsigned int bytes_available;
} streamcipher_t;

/**
 * Initialise cipher state from key. A zero state must be rejected or replaced,
 * since xorshift would otherwise produce only zeroes.
 */
void initCipher(streamcipher_t *cipher, unsigned long long key);

/** Generate the next 64-bit xorshift64* value and update cipher state. */
unsigned long long nextRandom(streamcipher_t *cipher);

/**
 * XOR length bytes in buffer with the cipher stream. The operation is
 * symmetric: applying it again with the same initial state restores the bytes.
 * Returns 0 on success and 1 on invalid input or failure.
 */
int performCipher(
    streamcipher_t *cipher,
    unsigned char *buffer,
    size_t length);

/**
 * Read inputFilename in binary chunks, apply performCipher, and write to
 * outputFilename. Returns 0 on success and 1 on failure. The implementation
 * must reject identical paths and check all file I/O results.
 *
 * This educational xorshift/XOR construction is not secure encryption and
 * must not be used to protect sensitive files.
 */
int encryptFile(
    const char *inputFilename,
    const char *outputFilename,
    unsigned long long key);

/** Decrypt a file using the same XOR stream transformation as encryption. */
int decryptFile(
    const char *inputFilename,
    const char *outputFilename,
    unsigned long long key);

#endif /* CIPHER_H */
