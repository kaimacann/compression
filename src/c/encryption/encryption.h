#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include<stdio.h>

/*FILE ENCRYPTION (Marceline) 
* --------------------------*/

/*Standard Multiplier for xorshift64*/
#define PRNG_MULTIPLIER 0x2545F4914F6CDD1DULL
#define KEY 39573927147483ULL

struct streamcipher {
    unsigned long long state;
    unsigned long long random_value;
    unsigned int bytes_available;
};
typedef struct streamcipher streamcipher_t;

/** Set a fresh nonzero seed and clear the buffered keystream state.
 * @param cipher State object to initialize; it must be non-null.
 * @param key Nonzero seed for the xorshift byte stream.
 * @return 0 when initialized, or 1 for a null state or zero key.
 * @author marceline
 */
int initCipher (streamcipher_t *cipher, unsigned long long key);

/** Advance the generator and make its next eight bytes available.
 * @param cipher Initialized stream state to advance.
 * @author marceline
 */
void nextRandom (streamcipher_t *cipher);

/** XOR a buffer with the stream, allowing the same state and key to restore it.
 * @param cipher Initialized stream state, advanced as bytes are consumed.
 * @param buffer Bytes transformed in place; must be non-null.
 * @param length Number of bytes to transform.
 * @return 0 on success, or 1 when the state or buffer is null.
 * @author marceline
 */
int performCipher(streamcipher_t *cipher, char *buffer, size_t length);

/** Transform an existing file in place with the selected XOR stream key.
 * @param filename Existing readable and writable file to transform.
 * @param key Nonzero seed shared with the matching reverse transform.
 * @return 0 when all bytes are transformed and the file closes cleanly; otherwise 1.
 * @author marceline
 */
int encryptFile (const char *filename, unsigned long long key);

#endif
