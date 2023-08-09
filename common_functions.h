/**
 * @file server.c
 * Name:- Ajay Kumar
 * Roll Number:- MT2022007
 */

#ifndef COMMON_FUNCTIONS
#define COMMON_FUNCTIONS

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "./account.h"
#include "./customer.h"
#include "./transaction.h"
#include "./helper.h"

bool loginHandler(bool isAdmin, int connFD, struct Customer *customerPtr);
bool getAccountDetails(int connFD, struct Account *customerAccount);
bool getCustomerDetails(int connFD, int customerId);
bool getTransactionDetails(int connFD, int accountNumber);

bool loginHandler(bool isAdmin, int connFD, struct Customer *customerPtr) {
    // Taking buffers for communication
    char readBuffer[1000], writeBuffer[1000];
    char tempBuffer[1000];
    ssize_t readBytes, writeBytes;

    struct Customer customer;

    int customerId = -1;

    bzero(writeBuffer, sizeof(writeBuffer));
    if (isAdmin)
        strcpy(writeBuffer, "-----> Welcome Admin <-----\n**Please Enter Your Credentials**");
    else
        strcpy(writeBuffer, "---> Welcome Customer <---\n**Please Enter Your Credentials**");

    strcat(writeBuffer, "\n\n");
    strcat(writeBuffer, "Login Id: ");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    
    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error while reading the Login Id from client!");
        return false;
    }

    bool userFound = false;
    if (isAdmin) {  // If logging in as Admin
        if (strcmp(readBuffer, ADMIN_LOGIN_ID) == 0)
            userFound = true;
    } else {    // if logging in as Customer
        // Extracting the customer's name and customer's id from Login Id
        bzero(tempBuffer, sizeof(tempBuffer));
        strcpy(tempBuffer, readBuffer);
        strtok(tempBuffer, "-");
        char *str = strtok(NULL, "-");
        customerId = atoi((str == NULL) ? "-1" : str);

        // Opening customer's database file
        int customerFileFd = open(CUSTOMER_FILE, O_RDONLY);
        if (customerFileFd == -1) {
            perror("Error opening customer file!");
            return false;
        }

        // Taking the pointer to specific customer
        off_t offset = lseek(customerFileFd, customerId * sizeof(struct Customer), SEEK_SET);
        if (offset >= 0) {
            // Applying read lock on the required customer structure
            // Using record locking
            struct flock lock = {F_RDLCK, SEEK_SET, customerId * sizeof(struct Customer), sizeof(struct Customer), getpid()};
            int lockingStatus = fcntl(customerFileFd, F_SETLKW, &lock);
            if (lockingStatus == -1) {
                perror("Error obtaining read lock on customer record!");
                return false;
            }

            readBytes = read(customerFileFd, &customer, sizeof(struct Customer));
            if (readBytes == -1) {
                perror("Error reading customer record from file!");
            }

            // unlocking the record
            lock.l_type = F_UNLCK;
            fcntl(customerFileFd, F_SETLK, &lock);

            // Confirming the customer's username
            if (strcmp(customer.loginUsername, readBuffer) == 0)
                userFound = true;

            close(customerFileFd);
        } else {
            writeBytes = write(connFD, "Sorry, No Customer Found.$", strlen("Sorry, No Customer Found.$"));
            return false;
        }
    }

    if (userFound) {
        bzero(writeBuffer, sizeof(writeBuffer));
        writeBytes = write(connFD, "Password: \n# ", strlen("Password: \n# "));

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        if (readBytes == 1) {
            perror("Error reading password from the client!");
            return false;
        }

        // Confirming the password for admin and customer
        if (isAdmin) {
            if (strcmp(readBuffer, ADMIN_PASSWORD) == 0)
                return true;
        } else {
            if (strcmp(readBuffer, customer.loginPassword) == 0) {
                *customerPtr = customer;
                return true;
            }
        }

        bzero(writeBuffer, sizeof(writeBuffer));
        writeBytes = write(connFD, "Wrong Password!$", strlen("Wrong Password!$"));
    } else {
        bzero(writeBuffer, sizeof(writeBuffer));
        writeBytes = write(connFD, "Provided Login Id Doesn't Exist!$", strlen("Provided Login Id Doesn't Exist!$"));
    }
    return false;
}

bool getAccountDetails(int connFd, struct Account *customerAccount) {
    // Taking buffers for communication
    char readBuffer[1000], writeBuffer[1000];
    char tempBuffer[1000];
    ssize_t readBytes, writeBytes;

    int accountNumber;
    struct Account account;
    int accountFileDescriptor;

    // Using as Admin -> so user has to provide account number
    if (customerAccount == NULL) {
        writeBytes = write(connFd, "Account Number: ", strlen("Account Number: "));

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFd, readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error reading account number from client!");
            return false;
        }
        accountNumber = atoi(readBuffer);
    } else  // If customer is using this function -> sending account number with the structure
        accountNumber = customerAccount->accountNumber;

    // Opening account database file
    accountFileDescriptor = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFileDescriptor == -1) {
        // Account record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Account Not Found!");
        strcat(writeBuffer, "^");
        perror("Error opening account file in getAccountDetails!");
        writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));

        readBytes = read(connFd, readBuffer, sizeof(readBuffer));
        return false;
    }

    // Moving the pointer to provided account number
    int offset = lseek(accountFileDescriptor, accountNumber * sizeof(struct Account), SEEK_SET);
    if (offset == -1 && errno == EINVAL) {
        // Account record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Account Not Found!");
        strcat(writeBuffer, "^");
        perror("Error seeking to account record in getAccountDetails!");
        writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFd, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    } else if (offset == -1) {
        perror("Error while seeking to required account record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
    int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1) {
        perror("Error obtaining read lock on account record!");
        return false;
    }

    readBytes = read(accountFileDescriptor, &account, sizeof(struct Account));
    if (readBytes == -1) {
        perror("Error reading account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(accountFileDescriptor, F_SETLK, &lock);

    // Filling customer's global structure
    if (customerAccount != NULL) {
        *customerAccount = account;
        return true;
    }

    // Showing customer's information on client's terminal
    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "## Account Details ##\n\tAccount Number: %d\n\tAccount Type: %s\n\tAccount Status: %s", account.accountNumber, (account.isAccountRegular ? "Regular" : "Joint"), (account.isActive) ? "Active" : "Deactived");
    if (account.isActive) {
        sprintf(tempBuffer, "\n\tAccount Balance:₹ %ld", account.balance);
        strcat(writeBuffer, tempBuffer);
    }

    sprintf(tempBuffer, "\n\tPrimary Owner's Id: %d", account.owners[0]);
    strcat(writeBuffer, tempBuffer);
    if (account.owners[1] != -1) {
        sprintf(tempBuffer, "\n\tSecondary Owner's Id: %d", account.owners[1]);
        strcat(writeBuffer, tempBuffer);
    }

    strcat(writeBuffer, "\n^");
    writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));

    readBytes = read(connFd, readBuffer, sizeof(readBuffer));
    return true;
}

bool getCustomerDetails(int connFd, int customerId) {
    // Taking buffers for communication
    char readBuffer[1000], writeBuffer[10000];
    char tempBuffer[1000];
    ssize_t readBytes, writeBytes;

    struct Customer customer;
    int customerFileDescriptor;

    if (customerId == -1) {
        writeBytes = write(connFd, "Enter Customer Id: ", strlen("Enter Customer Id: "));

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFd, readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error getting customer Id from client!");
            return false;
        }
        customerId = atoi(readBuffer);
        if (customerId == 0) {
            for (int i = 0; i < strlen(readBuffer); i++) {
                char ch = readBuffer[i];
                if (ch < '0' || ch > '9') {
                    bzero(writeBuffer, sizeof(writeBuffer));
                    strcpy(writeBuffer, "Invalid Input!");
                    strcat(writeBuffer, "^");
                    writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
                    readBytes = read(connFd, readBuffer, sizeof(readBuffer));
                    return false;
                }
            }
        }
        
        /*if (readBuffer[0] != '0' && customerId == 0) {
            bzero(writeBuffer, sizeof(writeBuffer));
            strcpy(writeBuffer, "Customer Not Found!");
            strcat(writeBuffer, "^");
            writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
            readBytes = read(connFd, readBuffer, sizeof(readBuffer));
            return false;
        }*/
    }

    customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFileDescriptor == -1) {
        // Customer File doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Customer Not Found!");
        strcat(writeBuffer, "^");
        writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFd, readBuffer, sizeof(readBuffer));
        return false;
    }

    int offset = lseek(customerFileDescriptor, customerId * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL) {
        // Customer record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Customer Not Found!");
        strcat(writeBuffer, "^");
        writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFd, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    } else if (offset == -1) {
        perror("Error while seeking to required customer record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
    int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1) {
        perror("Error while obtaining read lock on the Customer file!");
        return false;
    }

    readBytes = read(customerFileDescriptor, &customer, sizeof(struct Customer));
    if (readBytes == -1) {
        perror("Error reading customer record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(customerFileDescriptor, F_SETLK, &lock);

    bzero(writeBuffer, sizeof(writeBuffer));
    if (customer.id == customerId)
        sprintf(writeBuffer, "## Customer Details ##\n\tCustomer Id: %d\n\tName: %s\n\tGender: %c\n\tAge: %d\n\tAccount Number: %d\n\tLogin Id: %s", customer.id, customer.name, customer.gender, customer.age, customer.accountNumber, customer.loginUsername);
    else
        sprintf(writeBuffer, "\nCustomer with Id: %d not found.", customerId);
    strcat(writeBuffer, "\n\nRedirecting to the main menu...^");
    writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
    readBytes = read(connFd, readBuffer, sizeof(readBuffer)); // Dummy read
    return true;
}

bool getTransactionDetails(int connFd, int accountNumber) {
    // Taking buffers for communication...
    char readBuffer[1000], writeBuffer[10000], tempBuffer[1000];
    ssize_t readBytes, writeBytes;
    struct Account account;

    // If Admin Login the taking account number from the user
    if (accountNumber == -1) {
        // Get the accountNumber
        writeBytes = write(connFd, "Account Number: ", strlen("Account Number: "));

        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connFd, readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error reading account number response from client!");
            return false;
        }
        account.accountNumber = atoi(readBuffer);
    } else
        account.accountNumber = accountNumber;

    if (getAccountDetails(connFd, &account)) {
        int iter;

        struct Transaction transaction;
        struct tm transactionTime;

        bzero(writeBuffer, sizeof(readBuffer));
        int transactionFileDescriptor = open(TRANSACTION_FILE, O_RDONLY);
        if (transactionFileDescriptor == -1) {
            perror("Error while opening transaction file: ");
            write(connFd, "No transactions were performed on this account by the customer!^", strlen("No transactions were performed on this account by the customer!^"));
            read(connFd, readBuffer, sizeof(readBuffer)); // Dummy read
            return false;
        }

        // Showing all the transactions made by the customer...
        for (iter = 0; iter < MAX_TRANSACTIONS && account.transactions[iter] != -1; iter++) {
            int offset = lseek(transactionFileDescriptor, account.transactions[iter] * sizeof(struct Transaction), SEEK_SET);
            if (offset == -1) {
                perror("Error while seeking to required transaction record!");
                return false;
            }

            struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Transaction), getpid()};
            int lockingStatus = fcntl(transactionFileDescriptor, F_SETLKW, &lock);
            if (lockingStatus == -1) {
                perror("Error obtaining read lock on transaction record!");
                return false;
            }

            readBytes = read(transactionFileDescriptor, &transaction, sizeof(struct Transaction));
            if (readBytes == -1) {
                perror("Error reading transaction record from file!");
                return false;
            }

            lock.l_type = F_UNLCK;
            fcntl(transactionFileDescriptor, F_SETLK, &lock);

            transactionTime = *localtime(&(transaction.transactionTime));

            bzero(tempBuffer, sizeof(tempBuffer));
            sprintf(tempBuffer, "---> Transaction:- %d - \n\tDate: %d:%d %d/%d/%d\n\tOperation: %s\n\tBalance: \n\t\tBefore: %ld\n\t\tAfter: %ld\n\t\tDifference : %ld\n", (iter + 1), transactionTime.tm_hour, transactionTime.tm_min, transactionTime.tm_mday, (transactionTime.tm_mon + 1), (transactionTime.tm_year + 1900), (transaction.operation ? "Deposit" : "Withdraw"), transaction.oldBalance, transaction.newBalance, (transaction.newBalance - transaction.oldBalance));
            if (strlen(writeBuffer) == 0)
                strcpy(writeBuffer, tempBuffer);
            else
                strcat(writeBuffer, tempBuffer);
        }
        close(transactionFileDescriptor);

        if (strlen(writeBuffer) == 0) {
            write(connFd, "No transactions were performed on this account by the customer!^", strlen("No transactions were performed on this account by the customer!^"));
            read(connFd, readBuffer, sizeof(readBuffer));
            return false;
        } else {
            strcat(writeBuffer, "^");
            writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
            read(connFd, readBuffer, sizeof(readBuffer));
        }
    }
}
#endif