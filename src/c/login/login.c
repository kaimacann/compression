#include "login.h"

/** Prompt for the first account and write the current credential table to disk.
 * @param credentials Fixed-size username/password rows; the new account is appended here.
 * @param num_credentials Current row count, incremented after input is accepted.
 * @return 0 when account creation and file writing succeed; otherwise 1.
 * @author marceline
 */
int createAccount(char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials) { //This is being passed a pointer to the start of the 2d array.
    if (*num_credentials < MAX_ACCOUNTS) {
        FILE *file = fopen(ACCOUNT_FILE, "wbx");
        if (file == NULL) {
            printf("Error in Create Account");
            return 1; // Failure: unable to create account file.
        }

        printf("Create a new account:\n");
        printf("Enter username: ");
        if (fgets(credentials[*num_credentials][0], CREDENTIAL_LENGTH, stdin) == NULL ||
            credentials[*num_credentials][0][0] == '\n' ||
            (strchr(credentials[*num_credentials][0], '\n') == NULL && getchar() != '\n' &&
             !feof(stdin))) {
            fclose(file);
            remove(ACCOUNT_FILE);
            return 1;
        }
        credentials[*num_credentials][0][strcspn(credentials[*num_credentials][0], "\n")] = '\0';

        printf("Enter password: ");
        if (fgets(credentials[*num_credentials][1], CREDENTIAL_LENGTH, stdin) == NULL ||
            credentials[*num_credentials][1][0] == '\n' ||
            (strchr(credentials[*num_credentials][1], '\n') == NULL && getchar() != '\n' &&
             !feof(stdin))) {
            fclose(file);
            remove(ACCOUNT_FILE);
            return 1;
        }
        credentials[*num_credentials][1][strcspn(credentials[*num_credentials][1], "\n")] = '\0';

        (*num_credentials)++;

        int i = 0;
        for (; i < *num_credentials; i++) {
            int j = 0;
            for (; j < 2; j++) {
                if (fwrite(credentials[i][j], 1, CREDENTIAL_LENGTH, file) != CREDENTIAL_LENGTH) {
                    fclose(file);
                    remove(ACCOUNT_FILE);
                    return 1;
                }
            }
        }

        if (fclose(file) != 0) {
            remove(ACCOUNT_FILE);
            return 1;
        }
        printf("New account created successfully!\n\n");
        return 0; // Success: account created.
    } else {
        printf("Unable to create new account: Max accounts reached.");
        return 1;
    }
}

/** Load fixed-width credential records, prompting to create the first account if absent.
 * @param credentials Caller-owned table with room for MAX_ACCOUNTS records.
 * @param num_credentials Existing record count to increment as rows are loaded.
 * @return 0 when records load or first-account creation succeeds; otherwise 1.
 * @author marceline
 */
int loadCredentials (char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials) {
    FILE *file = fopen(ACCOUNT_FILE, "rb");
    if (file == NULL) {
        return createAccount(credentials, num_credentials); // File does not exist, first time run.
    }

    // Read existing credentials from the file
    *num_credentials = 0;
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        size_t username_read = fread(credentials[i][0], sizeof(char), CREDENTIAL_LENGTH, file); //Finds length of current account
        if (username_read != CREDENTIAL_LENGTH ||
            memchr(credentials[i][0], '\0', CREDENTIAL_LENGTH) == NULL)
            break;
        size_t password_read = fread(credentials[i][1], sizeof(char), CREDENTIAL_LENGTH, file);
        if (password_read != CREDENTIAL_LENGTH ||
            memchr(credentials[i][1], '\0', CREDENTIAL_LENGTH) == NULL)
            break;

        (*num_credentials) += 1;
    }

    if (fclose(file) != 0 || *num_credentials == 0)
        return 1;
    return 0; // Success: credentials loaded.
}

/** Prompt until credentials match a loaded row or the user requests exit.
 * @param credentials Fixed-width account records to compare with user input.
 * @param num_credentials Number of initialized records in the table.
 * @return 0 for a match; 1 for input failure or the -1 exit command.
 * @author marceline
 */
int authenticateUser(char credentials[][2][CREDENTIAL_LENGTH], int num_credentials) {
    char username[CREDENTIAL_LENGTH];
    char password[CREDENTIAL_LENGTH];
    printf("Type -1 to exit.\n");
    while (1) {
        printf("Enter username: ");
        if (fgets(username, CREDENTIAL_LENGTH, stdin) == NULL) {
            return 1;
        }
        if (strchr(username, '\n') == NULL && getchar() != '\n' && !feof(stdin))
            return 1;
        username[strcspn(username, "\n")] = '\0';

        if (strcmp(username, "-1") == 0) {
            return 1;
        }

        printf("Enter password: ");
        if (fgets(password, CREDENTIAL_LENGTH, stdin) == NULL) {
            return 1;
        }
        if (strchr(password, '\n') == NULL && getchar() != '\n' && !feof(stdin))
            return 1;
        password[strcspn(password, "\n")] = '\0';

        if (strcmp(password, "-1") == 0) {
            return 1;
        }

        int i = 0;
        for (; i < num_credentials; i++) {
            if (strcmp(username, credentials[i][0]) == 0 &&
                strcmp(password, credentials[i][1]) == 0) {
                    printf("Login successful.\n");
                    return 0;
                }
        }
        printf("Incorrect username or password. Please try again.\n");
    }
}

/** Load or create local credentials, then run the interactive authentication prompt.
 * @return 1 if loading or initial account creation fails; otherwise 0 after the prompt exits.
 * @author marceline
 */
int loginFunction(void) {
    char credentials[MAX_ACCOUNTS][2][CREDENTIAL_LENGTH]; // Creates 3D array to hold usernames and passwords.
    int num_credentials = 0;
    if (loadCredentials(credentials, &num_credentials) != 0) {
        printf("Error loading credentials.\n");
        return 1; // Failure
    }

    if (authenticateUser(credentials, num_credentials) != 0) {
        printf("Exiting program.\n");
        return 1;
    }

    printf("Logged in successfully.\n");
    return 0;
}
