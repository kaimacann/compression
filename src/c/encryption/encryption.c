#include "cipher.h"

int initCipher(streamcipher_t *cipher, unsigned long long key) {
    if (cipher == NULL || key == 0) {
        return 1; //  Failure
    }
    
    cipher->state = key;
    cipher->random_value = 0;
    cipher->bytes_available = 0; 

    return 0; //Success
}

void nextRandom(streamcipher_t *cipher) {
    cipher->state ^= cipher->state >> 12;
    cipher->state ^= cipher->state << 25;
    cipher->state ^= cipher->state >> 27;
    cipher->random_value = cipher->state * PRNG_MULTIPLIER;
    cipher->bytes_available = 8;
}

int performCipher(streamcipher_t *cipher, char *buffer, size_t length) {
    size_t i; //declaring i as a size_t since it is being compared to length, also a size_t

    if (cipher == NULL || buffer == NULL) {
        return 1; //return failure if cipher is unassigned, or if the buffer is empty
    }
    
    for (i = 0; i < length; i++) {
        if (cipher->bytes_available == 0) {
            nextRandom(cipher); //continues to generate the stream
        }

        buffer[i] ^= (char)(cipher->random_value & 0xFFULL);
        //this line extracts the lowest byte of the cipher's random_value by performing a bitwise AND with 255
        //this means all bits over the first 8 are ANDed with 0, leaving 0
        //thus, only the lowest 8 bits of the random_value remain, which matches the size of buffer[i]
        //it then XORs it with the current read byte from the buffer

        cipher->random_value >>= 8; //shifts the random value right to use the next 8 bytes of it
        cipher->bytes_available--; //counts how many bytes remain in the random_value
    }

    return 0;
}

int encryptFile(const char *filename, unsigned long long key) {
    FILE *file;
    streamcipher_t cipher;
    char buffer[4096]; // check this
    size_t bytes_read;

    //failure checks for file

    if (filename == NULL) {
        return 1;
    }

    file = fopen(filename, "r+b"); //allows reading and writing to overwrite plaintext with ciphertext.
    if (file == NULL) {
        return 1;
    }

    //start encryption/decryption

    initCipher(&cipher, key);

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) { //reads file into buffer
        if (performCipher(&cipher, buffer, bytes_read) !=0) {
            fclose(file);
            return 1; //returns the failure status from performCipher
        }

        if (fseek(file, -(long)bytes_read, SEEK_CUR) != 0) { //returns pointer to position in file back to where fread started
            fclose(file);
            return 1; //returns the failure status from fseek
        }

        if (fwrite(buffer, 1, bytes_read, file) != bytes_read) { //writes the ciphertext over the plaintext
            fclose(file);
            return 1; //returns the failure status from fwrite
        }

        if (fseek(file, 0, SEEK_CUR) != 0) { //performs a file-positioning operation to allow changing from reading to writing in r+b mode
            fclose(file);
            return 1; //returns the failure status from fseek
        }
    }

    if (ferror(file)) {
        return 1; //returns failure if an error occurs during reading the file
    }

    if (fclose(file) != 0) {
        return 1; //returns failure if error closing the file
    }

    return 0;
}
