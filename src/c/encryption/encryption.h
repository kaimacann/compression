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

/*This function initialises the cipher.
* The key is loaded into the state, and default values are set for random_value and bytes_available.
* Inputs:
* - The cipher.
* - The encryption key.
*/
int initCipher (streamcipher_t *cipher, unsigned long long key);

/* This function performs the bitwise XOR shifts on the cipher and then the multiplier.
* This may also later alter the key based on the metadata to ensure a different key for each file.
* Inputs:
* - The cipher.
* Output:
* - An unsigned 64-bit integer to overwrite the cipher's "random_value" attribute evry 8 bytes of encryption.
*/
void nextRandom (streamcipher_t *cipher);

/*This function applies the cipher to the input file which has been loaded into the buffer in the encrypt_file function.
* It performs XOR encryption on the input with the random_value in the cipher.
* Inputs:
* - The cipher.
* Output:
* - The new value for the cipher's random_value.
*/
int performCipher(streamcipher_t *cipher, char *buffer, size_t length);

/*This function encrypts or decrypts a file by reading in the file and passing
* input from the buffer along with the buffer's length and the cipher to the 
* perform_cipher function, until the EOF is reached.
* Inputs:
* - The name of the file to use.
* - The encryption key.
* Output:
* - 1 for failure
* - 0 for success
*/
int encryptFile (const char *filename, unsigned long long key);

#endif
