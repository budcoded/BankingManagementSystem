/**
 * @file server.c
 * Name:- Ajay Kumar
 * Roll Number:- MT2022007
 */

#ifndef CUSTOMER_FUNCTIONS
#define CUSTOMER_FUNCTIONS

#include <sys/ipc.h>
#include <sys/sem.h>

#include "./helper.h"

// Logged In customer's details
struct Customer loggedInCustomer;
int semIdentifier;

bool customerOperationHandler(int connFD);
bool deposit(int connFD);
bool withdraw(int connFD);
bool getBalance(int connFD);
bool changePassword(int connFD);
bool lockCriticalSection(struct sembuf *semOp);
bool unlockCriticalSection(struct sembuf *sem_op);
void writeTransactionToArray(int *transactionArray, int ID);
int writeTransactionToFile(int accountNumber, long int oldBalance, long int newBalance, bool operation);

bool customerOperationHandler(int connFD) {
    if (loginHandler(false, connFD, &loggedInCustomer)) {
        // Taking buffers for communication
        ssize_t writeBytes, readBytes;
        char readBuffer[1000], writeBuffer[1000];

        // Get a semaphore for the user
        // Generating a key based on the account number so different customers will have different semaphores
        key_t semKey = ftok(CUSTOMER_FILE, loggedInCustomer.accountNumber);
        union semun {
            int val; // Value of the semaphore
        } semSet;

        int semctlStatus;
        semIdentifier = semget(semKey, 1, 0); // Get the semaphore if it exists
        if (semIdentifier == -1) {
            semIdentifier = semget(semKey, 1, IPC_CREAT | 0700); // Create a new semaphore if not exists
            if (semIdentifier == -1) {
                perror("Error while creating semaphore!");
                _exit(1);
            }

            semSet.val = 1; // Set a binary semaphore
            semctlStatus = semctl(semIdentifier, 0, SETVAL, semSet);
            if (semctlStatus == -1) {
                perror("Error while initializing a binary sempahore!");
                _exit(1);
            }
        }

        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "---> Welcome Customer <---");
        while (1) {
            strcat(writeBuffer, "\n");
            strcat(writeBuffer, "1 --> Get Customer Details\n2 --> Deposit Money\n3 --> Withdraw Money\n4 --> Get Balance\n5 --> Get Transaction information\n6 --> Change Password\nPress any other key to logout.");
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            bzero(writeBuffer, sizeof(writeBuffer));

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            int choice = atoi(readBuffer);
            switch (choice) {
            case 1:
                getCustomerDetails(connFD, loggedInCustomer.id);
                break;
            case 2:
                deposit(connFD);
                break;
            case 3:
                withdraw(connFD);
                break;
            case 4:
                getBalance(connFD);
                break;
            case 5:
                getTransactionDetails(connFD, loggedInCustomer.accountNumber);
                break;
            case 6:
                changePassword(connFD);
                break;
            default:
                writeBytes = write(connFD, "Logging customer out!$", strlen("Logging customer out!$"));
                return false;
            }
        }
    } else {
        // Customer login failed
        return false;
    }
    return true;
}

bool deposit(int connFD) {
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;
    struct Account account;

    account.accountNumber = loggedInCustomer.accountNumber;
    long int depositAmount = 0;

    // Locking the critical section
    struct sembuf semOp;
    lockCriticalSection(&semOp);
    if (getAccountDetails(connFD, &account)) {
        if (account.isActive) {
            writeBytes = write(connFD, "How much money to add into bank account:- ", strlen("How much money to add into bank account:- "));
            if (writeBytes == -1) {
                unlockCriticalSection(&semOp);
                return false;
            }

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1) {
                perror("Error reading deposit money from client!");
                unlockCriticalSection(&semOp);
                return false;
            }

            depositAmount = atol(readBuffer);
            // Customer can't add 0 rupee in the account
            if (depositAmount != 0) {
                int newTransactionId = writeTransactionToFile(account.accountNumber, account.balance, account.balance + depositAmount, 1);
                writeTransactionToArray(account.transactions, newTransactionId);
                account.balance += depositAmount;

                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset = lseek(accountFileDescriptor, account.accountNumber * sizeof(struct Account), SEEK_SET);
                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
                if (lockingStatus == -1) {
                    perror("Error obtaining write lock on account file!");
                    unlockCriticalSection(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
                if (writeBytes == -1) {
                    perror("Error storing updated deposit money in account record!");
                    unlockCriticalSection(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock);
                write(connFD, "Amount deposited successfully!^", strlen("Amount deposited successfully!^"));
                read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

                getBalance(connFD);
                unlockCriticalSection(&semOp);
                return true;
            } else
                writeBytes = write(connFD, "Invalid Amount!^", strlen("Invalid Amount!^"));
        } else
            write(connFD, "Your account is deactivated!^", strlen("Your account is deactivated!^"));
        
        read(connFD, readBuffer, sizeof(readBuffer));
        unlockCriticalSection(&semOp);
    } else {
        // Getting account details process failed
        unlockCriticalSection(&semOp);
        return false;
    }
}

bool withdraw(int connFD) {
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;
    struct Account account;
    account.accountNumber = loggedInCustomer.accountNumber;
    long int withdrawAmount = 0;

    // Lock the critical section
    struct sembuf semOp;
    lockCriticalSection(&semOp);
    if (getAccountDetails(connFD, &account)) {
        if (account.isActive) {
            writeBytes = write(connFD, "Amount to withdraw:- ", strlen("Amount to withdraw:- "));
            if (writeBytes == -1) {
                perror("Error writing withdrar amount message to client!");
                unlockCriticalSection(&semOp);
                return false;
            }

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            if (readBytes == -1) {
                perror("Error reading withdraw amount from client!");
                unlockCriticalSection(&semOp);
                return false;
            }

            withdrawAmount = atol(readBuffer);
            // Amount can't be zero or more than account balance
            if (withdrawAmount != 0 && account.balance - withdrawAmount >= 0) {
                int newTransactionID = writeTransactionToFile(account.accountNumber, account.balance, account.balance - withdrawAmount, 0);
                writeTransactionToArray(account.transactions, newTransactionID);

                account.balance -= withdrawAmount;
                int accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
                off_t offset = lseek(accountFileDescriptor, account.accountNumber * sizeof(struct Account), SEEK_SET);

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
                if (lockingStatus == -1) {
                    perror("Error obtaining write lock on account record!");
                    unlockCriticalSection(&semOp);
                    return false;
                }

                writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
                if (writeBytes == -1) {
                    perror("Error writing updated balance into account file!");
                    unlockCriticalSection(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFileDescriptor, F_SETLK, &lock);

                write(connFD, "Amount withdrawn successfully!^", strlen("Amount withdrawn successfully!^"));
                read(connFD, readBuffer, sizeof(readBuffer));
                getBalance(connFD);
                unlockCriticalSection(&semOp);
                return true;
            } else
                writeBytes = write(connFD, "Invalid amount / Low Balance^", strlen("Invalid amount / Low Balance^"));
        } else
            write(connFD, "Your account is deactivated!^", strlen("Your account is deactivated!^"));
        read(connFD, readBuffer, sizeof(readBuffer));
        unlockCriticalSection(&semOp);
    } else {
        // Failed while getting account information
        unlockCriticalSection(&semOp);
        return false;
    }
}

bool getBalance(int connFD) {
    char buffer[1000];
    struct Account account;
    account.accountNumber = loggedInCustomer.accountNumber;

    if (getAccountDetails(connFD, &account)) {
        bzero(buffer, sizeof(buffer));
        if (account.isActive) {
            sprintf(buffer, "You have ₹ %ld money in bank account!^", account.balance);
            write(connFD, buffer, strlen(buffer));
        } else
            write(connFD, "Your account is deactivated!^", strlen("Your account is deactivated!^"));
        read(connFD, buffer, sizeof(buffer));
    } else {
        // Error while getting balance
        return false;
    }
}

bool changePassword(int connFD) {
    // Taking buffers for communication
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000], hashedPassword[1000];
    char newPassword[1000];

    // Lock the critical section
    struct sembuf semOp = {0, -1, SEM_UNDO};
    int semopStatus = semop(semIdentifier, &semOp, 1);
    if (semopStatus == -1) {
        perror("Error while locking critical section");
        return false;
    }

    writeBytes = write(connFD, "Old Password:- ", strlen("Old Password:- "));
    if (writeBytes == -1) {
        perror("Error writing old password message to client!");
        unlockCriticalSection(&semOp);
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error reading old password response from client");
        unlockCriticalSection(&semOp);
        return false;
    }

    if (strcmp(readBuffer, loggedInCustomer.loginPassword) == 0) {
        // Password matches with old password
        writeBytes = write(connFD, "New Password:- ", strlen("New Password:- "));
        if (writeBytes == -1) {
            perror("Error writing new password message to client!");
            unlockCriticalSection(&semOp);
            return false;
        }
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error reading new password response from client");
            unlockCriticalSection(&semOp);
            return false;
        } else if (readBytes < 8) {
            writeBytes = write(connFD, "Password length should be 8 or more!^", strlen("Password length should be 8 or more!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            unlockCriticalSection(&semOp);
            return false;
		}

        strcpy(newPassword, readBuffer);
        writeBytes = write(connFD, "Re-Enter New Password:- ", strlen("Re-Enter New Password:- "));
        if (writeBytes == -1) {
            perror("Error writing re-enter new password message to client!");
            unlockCriticalSection(&semOp);
            return false;
        }

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error reading new password reenter response from client");
            unlockCriticalSection(&semOp);
            return false;
        }

        if (strcmp(readBuffer, newPassword) == 0) {
            // When new password and re-entered password matches
            strcpy(loggedInCustomer.loginPassword, newPassword);

            int customerFileDescriptor = open(CUSTOMER_FILE, O_WRONLY);
            if (customerFileDescriptor == -1) {
                perror("Error opening customer file!");
                unlockCriticalSection(&semOp);
                return false;
            }

            off_t offset = lseek(customerFileDescriptor, loggedInCustomer.id * sizeof(struct Customer), SEEK_SET);
            if (offset == -1) {
                perror("Error seeking to the customer record!");
                unlockCriticalSection(&semOp);
                return false;
            }

            struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
            int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
            if (lockingStatus == -1) {
                perror("Error obtaining write lock on customer record!");
                unlockCriticalSection(&semOp);
                return false;
            }

            writeBytes = write(customerFileDescriptor, &loggedInCustomer, sizeof(struct Customer));
            if (writeBytes == -1) {
                perror("Error storing updated customer password into customer record!");
                unlockCriticalSection(&semOp);
                return false;
            }

            lock.l_type = F_UNLCK;
            lockingStatus = fcntl(customerFileDescriptor, F_SETLK, &lock);
            close(customerFileDescriptor);

            writeBytes = write(connFD, "Password successfully changed!^", strlen("Password successfully changed!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

            unlockCriticalSection(&semOp);
            return true;
        } else {
            // New & reentered passwords don't match
            writeBytes = write(connFD, "The new password and the reentered passwords don't seem to pass!^", strlen("The new password and the reentered passwords don't seem to pass!^"));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        }
    } else {
        // Password doesn't match with old password
        writeBytes = write(connFD, "The entered password doesn't seem to match with the old password!^", strlen("The entered password doesn't seem to match with the old password!^"));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
    }

    unlockCriticalSection(&semOp);
    return false;
}

bool lockCriticalSection(struct sembuf *semOp) {
    semOp->sem_flg = SEM_UNDO;
    semOp->sem_op = -1;
    semOp->sem_num = 0;
    int semopStatus = semop(semIdentifier, semOp, 1);
    if (semopStatus == -1) {
        perror("Error while locking critical section");
        return false;
    }
    return true;
}

bool unlockCriticalSection(struct sembuf *semOp) {
    semOp->sem_op = 1;
    int semopStatus = semop(semIdentifier, semOp, 1);
    if (semopStatus == -1) {
        perror("Error while operating on semaphore!");
        _exit(1);
    }
    return true;
}

void writeTransactionToArray(int *transactionArray, int ID) {
    // Checking if there is any space left for new transaction id
    int iter = 0;
    while (transactionArray[iter] != -1)
        iter++;

    if (iter >= MAX_TRANSACTIONS) {
        // No space
        for (iter = 1; iter < MAX_TRANSACTIONS; iter++)
            // Discarding the oldest transaction by shifting the transaction id's
            transactionArray[iter - 1] = transactionArray[iter];
        transactionArray[iter - 1] = ID;
    } else {
        // If space is available
        transactionArray[iter] = ID;
    }
}

int writeTransactionToFile(int accountNumber, long int oldBalance, long int newBalance, bool operation) {
    struct Transaction newTransaction;
    newTransaction.accountNumber = accountNumber;
    newTransaction.oldBalance = oldBalance;
    newTransaction.newBalance = newBalance;
    newTransaction.operation = operation;
    newTransaction.transactionTime = time(NULL);
    ssize_t readBytes, writeBytes;

    int transactionFileDescriptor = open(TRANSACTION_FILE, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);

    // Get most recent transaction number
    off_t offset = lseek(transactionFileDescriptor, -sizeof(struct Transaction), SEEK_END);
    if (offset >= 0) {
        // There exists at least one transaction record
        struct Transaction prevTransaction;
        readBytes = read(transactionFileDescriptor, &prevTransaction, sizeof(struct Transaction));

        newTransaction.transactionId = prevTransaction.transactionId + 1;
    } else
        // No transaction records exist
        newTransaction.transactionId = 0;

    writeBytes = write(transactionFileDescriptor, &newTransaction, sizeof(struct Transaction));
    return newTransaction.transactionId;
}
#endif
