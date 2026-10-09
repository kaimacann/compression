#ifndef LOGIN_H
#define LOGIN_H

#define ACCOUNT_FILE ".login"

/* LOGIN — educational local-account interface. */

/**
 * Create an account for username using password and write its verifier to
 * ACCOUNT_FILE. Returns 0 on success and 1 on failure. Never store plaintext
 * passwords. With only the permitted standard C libraries, a secure password
 * hashing/KDF is unavailable; any in-project verifier is educational only.
 */
int createAccount(const char *username, const char *password);

/**
 * Check username and password against ACCOUNT_FILE. Returns 0 when accepted
 * and 1 when rejected or an error occurs. Do not claim production-grade
 * authentication when using a home-grown verifier.
 */
int authenticateUser(const char *username, const char *password);

#endif /* LOGIN_H */
