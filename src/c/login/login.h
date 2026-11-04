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

/** Load fixed-width credential records, creating the first account when the file is absent.
 * @param credentials Caller-owned table with room for MAX_ACCOUNTS records.
 * @param num_credentials Existing record count to increment as rows are loaded.
 * @return 0 when records load or first-account creation succeeds; otherwise 1.
 * @author marceline
 */

int loadCredentials(char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials);

/** Prompt for the first account and write the current credential table to disk.
 * @param credentials Fixed-size username/password rows; the new account is appended here.
 * @param num_credentials Current row count, incremented after input is accepted.
 * @return 0 when account creation and file writing succeed; otherwise 1.
 * @author marceline
 */
int createAccount(char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials);

/** Prompt until credentials match a loaded row or the user requests exit.
 * @param credentials Fixed-width account records to compare with user input.
 * @param num_credentials Number of initialized records in the table.
 * @return 0 for a match; 1 for input failure or the -1 exit command.
 * @author marceline
 */
int authenticateUser(char credentials[][2][CREDENTIAL_LENGTH], int num_credentials);

/** Load or create local credentials, then run the interactive authentication prompt.
 * @return 1 if loading or initial account creation fails; otherwise 0 after the prompt exits.
 * @author marceline
 */
int loginFunction(void);

#endif
