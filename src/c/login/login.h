#ifndef LOGIN_H
#define LOGIN_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

/*LOGIN SECURITY (Marceline) 
* --------------------------*/

#define ACCOUNT_FILE ".login"
#define MAX_ACCOUNTS 100
#define CREDENTIAL_LENGTH 51

/*This function checks if the hidden password file exists and is not empty.
* If it does not exist or is empty, the program is being run for the first time and the user must create an account.
* Output:
* - 1 for failure
* - 0 for success
*/

int loadCredentials(char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials);

/*This function creates a user after checking if the program is run for the first time via the loadCredentials function.
* This function gets user input for a username and password.
* It then encrypts this input using a hash function and writes it to the hidden password file.
* Output:
* - 1 for failure
* - 0 for success
*/
int createAccount(char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials);

/*This is the password function which checks if the username password is correct.
* Output:
* - 1 for failure
* - 0 for success
*/
int authenticateUser(char credentials[][2][CREDENTIAL_LENGTH], int num_credentials);

#endif