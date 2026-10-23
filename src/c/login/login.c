#include "login.h"

int createAccount(char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials) { //This is being passed a pointer to the start of the 2d array.
    if (*num_credentials < MAX_ACCOUNTS) {
        FILE *file = fopen(ACCOUNT_FILE, "wb");
        if (file == NULL) {
            printf("Error in Create Account");
            return 1; // Failure: unable to create account file.
        }

        printf("Create a new account:\n");
        printf("Enter username: ");
        if (fgets(credentials[*num_credentials][0], CREDENTIAL_LENGTH, stdin) != NULL) {
            credentials[*num_credentials][0][strcspn(credentials[*num_credentials][0], "\n")] = '\0'; // Remove newline character
        }

        printf("Enter password: ");
        if (fgets(credentials[*num_credentials][1], CREDENTIAL_LENGTH, stdin) != NULL) {
            credentials[*num_credentials][1][strcspn(credentials[*num_credentials][1], "\n")] = '\0'; // Remove newline character
        }

        (*num_credentials)++;

        //hash function here

        int i = 0;
        for (i; i < *num_credentials; i++) {
            int j = 0;
            for (j; j < 2; j++){
                fwrite(credentials[i][j], sizeof(char), CREDENTIAL_LENGTH, file); 
            }
        }

        fclose(file);
        printf("New account created successfully!\n\n");
        return 0; // Success: account created.
    } else {
        printf("Unable to create new account: Max accounts reached.");
    }
}

int loadCredentials (char credentials[][2][CREDENTIAL_LENGTH], int *num_credentials) {
    FILE *file = fopen(ACCOUNT_FILE, "rb");
    if (file == NULL) {
        return createAccount(credentials, num_credentials); // File does not exist, first time run.
    }

    // Read existing credentials from the file
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        size_t username_read = fread(credentials[i][0], sizeof(char), CREDENTIAL_LENGTH, file); //Finds length of current account
        if (username_read != CREDENTIAL_LENGTH) {
            break; //Breaks if no username is found, i.e. end of file is reached.
        };
        size_t password_read = fread(credentials[i][1], sizeof(char), CREDENTIAL_LENGTH, file);
        if (password_read != CREDENTIAL_LENGTH) {
            break;
        }

        (*num_credentials) += 1;
    }

    fclose(file);
    return 0; // Success: credentials loaded.
}

int authenticateUser(char credentials[][2][CREDENTIAL_LENGTH], int num_credentials) {
    char username[CREDENTIAL_LENGTH];
    char password[CREDENTIAL_LENGTH];
    printf("Type -1 to exit.\n");
    while (1) {
        printf("Enter username: ");
        if (fgets(username, CREDENTIAL_LENGTH, stdin) == NULL) {
            return 1;
        }
        username[strcspn(username, "\n")] = '\0';

        if (strcmp(username, "-1") == 0) {
            return 1;
        }

        printf("Enter password: ");
        if (fgets(password, CREDENTIAL_LENGTH, stdin) == NULL) {
            return 1;
        }
        password[strcspn(password, "\n")] = '\0';

        if (strcmp(password, "-1") == 0) {
            return 1;
        }

        int i = 0;
        for (i; i < num_credentials; i++) {
            if (strcmp(username, credentials[i][0]) == 0 &&
                strcmp(password, credentials[i][1]) == 0) {
                    printf("Login successful.\n");
                    return 0;
                }
        }
        printf("Incorrect username or password. Please try again.\n");
    }
}

int main() {
    char credentials[MAX_ACCOUNTS][2][CREDENTIAL_LENGTH]; // Creates 3D array to hold usernames and passwords.
    int num_credentials = 0;
    if (loadCredentials(credentials, &num_credentials) != 0) {
        printf("Error loading credentials.\n");
        return 1; // Failure
    }

    if (authenticateUser(credentials, num_credentials) == 0) {
        // Continue to menu()
    }
    
    printf("Exiting program.\n");

    return 0; // Success
}