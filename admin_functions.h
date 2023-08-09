/**
 * @file server.c
 * Name:- Ajay Kumar
 * Roll Number:- MT2022007
 */

#ifndef ADMIN_FUNCTIONS
#define ADMIN_FUNCTIONS

#include "./common_functions.h"
#include "./helper.h"

bool adminOperationHandler(int connFD);
bool addAccount(int connFD);
int addCustomer(int connFD, bool isPrimary, int newAccountNumber);
bool deleteAccount(int connFD);
bool modifyCustomerInfo(int connFD);

bool adminOperationHandler(int connFd) {
    if (loginHandler(true, connFd, NULL)) {
        // Taking buffer for communication
        ssize_t writeBytes, readBytes;
        char readBuffer[1000], writeBuffer[1000];
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Welcome Admin!\n");
        while (1) {
            strcat(writeBuffer, "\n");
            strcat(writeBuffer, "1 -> Get Customer Details\n2 -> Get Account Details\n3 -> Get Transaction details\n4 -> Add Account\n5 -> Delete Account\n6 -> Modify Customer Information\n--> Press any other key to logout");
            writeBytes = write(connFd, writeBuffer, strlen(writeBuffer));
            bzero(writeBuffer, sizeof(writeBuffer));

            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connFd, readBuffer, sizeof(readBuffer));
            if (readBytes == -1) {
                perror("Error while reading client's choice from admin menu");
                return false;
            }

            int choice = atoi(readBuffer);
            switch (choice) {
            case 1:
                getCustomerDetails(connFd, -1);
                break;
            case 2:
                getAccountDetails(connFd, NULL);
                break;
            case 3:
                getTransactionDetails(connFd, -1);
                break;
            case 4:
                addAccount(connFd);
                break;
            case 5:
                deleteAccount(connFd);
                break;
            case 6:
                modifyCustomerInfo(connFd);
                break;
            default:
                writeBytes = write(connFd, "Logging out!$", strlen("Logging out!$"));
                return false;
            }
        }
    }
    else {
        // Admin Login Failed
        return false;
    }
    return true;
}

bool addAccount(int connFd) {
    // Taking buffers for communication with the client
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];
    struct Account newAccount, prevAccount;

    // Opening account database file
    int accountFD = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFD == -1 && errno == ENOENT) {
        // Account file was never created
        newAccount.accountNumber = 0;
    } else if (accountFD == -1) {
        perror("Error while opening account file");
        return false;
    } else {
        int offset = lseek(accountFD, -sizeof(struct Account), SEEK_END);
        if (offset == -1) {
            perror("Error seeking to last Account record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
        int lockingStatus = fcntl(accountFD, F_SETLKW, &lock);
        if (lockingStatus == -1) {
            perror("Error obtaining read lock on Account record!");
            return false;
        }

        readBytes = read(accountFD, &prevAccount, sizeof(struct Account));
        if (readBytes == -1) {
            perror("Error while reading Account record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(accountFD, F_SETLK, &lock);
        close(accountFD);
        // Every time the account number will be changed
        newAccount.accountNumber = prevAccount.accountNumber + 1;
    }

    writeBytes = write(connFd, "Type Of Account? 1 --> Regular Account & 2 --> Joint Account", strlen("Type Of Account? 1 --> Regular Account & 2 --> Joint Account"));

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFd, &readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error reading account type response from client!");
        return false;
    }

    newAccount.isAccountRegular = atoi(readBuffer) == 1 ? true : false;
    newAccount.owners[0] = addCustomer(connFd, true, newAccount.accountNumber);

    if (newAccount.isAccountRegular)
        newAccount.owners[1] = -1;
    else
        newAccount.owners[1] = addCustomer(connFd, false, newAccount.accountNumber);

    newAccount.isActive = true;
    newAccount.balance = 0;

    memset(newAccount.transactions, -1, MAX_TRANSACTIONS * sizeof(int));

    accountFD = open(ACCOUNT_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (accountFD == -1) {
        perror("Error while creating / opening account file!");
        return false;
    }

    writeBytes = write(accountFD, &newAccount, sizeof(struct Account));
    if (writeBytes == -1) {
        perror("Error while writing Account record to file!");
        return false;
    }
    close(accountFD);

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%d", "The newly created account's number is: ", newAccount.accountNumber);
    strcat(writeBuffer, "\nRedirecting you to the main menu ...^");

    writeBytes = write(connFd, writeBuffer, sizeof(writeBuffer));
    readBytes = read(connFd, readBuffer, sizeof(read)); // Dummy read
    return true;
}

int addCustomer(int connFD, bool isPrimary, int newAccountNumber) {
    // Taking buffers for communication
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];
    struct Customer newCustomer, previousCustomer;

    // Opening the customer database file
    int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFileDescriptor == -1 && errno == ENOENT) {
        // Customer file was never created
        newCustomer.id = 0;
    } else if (customerFileDescriptor == -1) {
        perror("Error while opening customer file");
        return -1;
    } else {
        int offset = lseek(customerFileDescriptor, -sizeof(struct Customer), SEEK_END);
        if (offset == -1) {
            perror("Error seeking to last Customer record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};
        int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1) {
            perror("Error obtaining read lock on Customer record!");
            return false;
        }

        readBytes = read(customerFileDescriptor, &previousCustomer, sizeof(struct Customer));
        if (readBytes == -1) {
            perror("Error while reading Customer record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(customerFileDescriptor, F_SETLK, &lock);
        close(customerFileDescriptor);
        newCustomer.id = previousCustomer.id + 1;
    }

    if (isPrimary)
        sprintf(writeBuffer, "%s%s", "Primary Customer: \n", "Customer's Name?");
    else
        sprintf(writeBuffer, "%s%s", "Secondary Customer: \n", "Customer's Name?");

    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error reading customer name response from client!");
        return false;
    }

    strcpy(newCustomer.name, readBuffer);
    writeBytes = write(connFD, "Customer's Gender?\nEnter M --> Male, F --> Female, O --> Others", strlen("Customer's Gender?\nEnter M --> Male, F --> Female, O --> Others"));

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error reading customer gender response from client!");
        return false;
    }

    if (readBuffer[0] == 'M' || readBuffer[0] == 'F' || readBuffer[0] == 'O')
        newCustomer.gender = readBuffer[0];
    else {
        writeBytes = write(connFD, "You've entered a wrong gender choice!\n\nYou'll now be redirected to the main menu!^", strlen("It seems you've enter a wrong gender choice!\nYou'll now be redirected to the main menu!^"));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    bzero(writeBuffer, sizeof(writeBuffer));
    strcpy(writeBuffer, "Customer's age: ");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error reading customer age response from client!");
        return false;
    }

    int customerAge = atoi(readBuffer);
    if (customerAge == 0) {
        // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "It seems you have passed a sequence of alphabets when a number was expected or you have entered an invalid number!\n\nYou'll now be redirected to the main menu!^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    newCustomer.age = customerAge;
    newCustomer.accountNumber = newAccountNumber;

    strcpy(newCustomer.loginUsername, newCustomer.name);
    strcat(newCustomer.loginUsername, "-");
    sprintf(writeBuffer, "%d", newCustomer.id);
    strcat(newCustomer.loginUsername, writeBuffer);

    char password[1000];
    strcpy(password, CUSTOMER_DEFAULT_PASSWORD);
    strcpy(newCustomer.loginPassword, password);

    customerFileDescriptor = open(CUSTOMER_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);
    if (customerFileDescriptor == -1) {
        perror("Error while creating / opening customer file!");
        return false;
    }

    writeBytes = write(customerFileDescriptor, &newCustomer, sizeof(newCustomer));
    if (writeBytes == -1) {
        perror("Error while writing Customer record to file!");
        return false;
    }
    close(customerFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%s-%d\n%s%s", "The Auto-Generated Customer Login Id:- ", newCustomer.name, newCustomer.id, "The Auto-Generated Password:- ", CUSTOMER_DEFAULT_PASSWORD);
    strcat(writeBuffer, "^");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
    return newCustomer.id;
}

bool deleteAccount(int connFD) {
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Account account;

    writeBytes = write(connFD, "Account Number to be deleted?", strlen("Account Number to be deleted?"));

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error reading account number response from the client!");
        return false;
    }

    int accountNumber = atoi(readBuffer);
    int accountFileDescriptor = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFileDescriptor == -1) {
        // Account record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Account Not Found, Please Fill The Correct Information!");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    int offset = lseek(accountFileDescriptor, accountNumber * sizeof(struct Account), SEEK_SET);
    if (errno == EINVAL) {
        // Customer record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Account Not Found!");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    } else if (offset == -1) {
        perror("Error while seeking to required account record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
    int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1) {
        perror("Error obtaining read lock on Account record!");
        return false;
    }

    readBytes = read(accountFileDescriptor, &account, sizeof(struct Account));
    if (readBytes == -1) {
        perror("Error while reading Account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(accountFileDescriptor, F_SETLK, &lock);
    close(accountFileDescriptor);

    bzero(writeBuffer, sizeof(writeBuffer));
    if (!account.isActive) {
        strcpy(writeBuffer, "Account is already deleted!.\n\nRedirecting you to the main menu...^");
    } else if (account.balance == 0) {
        // No money, hence can close account
        account.isActive = false;
        accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
        if (accountFileDescriptor == -1) {
            perror("Error opening Account file in write mode!");
            return false;
        }

        offset = lseek(accountFileDescriptor, accountNumber * sizeof(struct Account), SEEK_SET);
        if (offset == -1) {
            perror("Error seeking to the Account!");
            return false;
        }

        lock.l_type = F_WRLCK;
        lock.l_start = offset;
        int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1) {
            perror("Error obtaining write lock on the Account file!");
            return false;
        }

        writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));
        if (writeBytes == -1) {
            perror("Error deleting account record!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(accountFileDescriptor, F_SETLK, &lock);
        strcpy(writeBuffer, "Account Deleted Successfully.\n\nRedirecting you to the main menu...^");
    } else
        // Account has some money ask customer to withdraw it
        strcpy(writeBuffer, "Account can't be deleted because it still has some money in it.\nRedirecting you to the main menu ...^");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    return true;
}

bool modifyCustomerInfo(int connFD) {
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];
    struct Customer customer;
    int customerID;
    off_t offset;
    int lockingStatus;

    writeBytes = write(connFD, "Customer Id who's information to edit: ", strlen("Customer Id who's information to edit: "));
    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error while reading customer ID from client!");
        return false;
    }

    customerID = atoi(readBuffer);

    int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);
    if (customerFileDescriptor == -1) {
        // Customer File doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Customer Not Found!");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        return false;
    }
    
    offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);
    if (errno == EINVAL) {
        // Customer record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "Customer Not Found!");
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        return false;
    } else if (offset == -1) {
        perror("Error while seeking to required customer record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};

    // Lock the record to be read
    lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1) {
        perror("Couldn't obtain lock on customer record!");
        return false;
    }

    readBytes = read(customerFileDescriptor, &customer, sizeof(struct Customer));
    if (readBytes == -1) {
        perror("Error while reading customer record from the file!");
        return false;
    }

    // Unlock the record
    lock.l_type = F_UNLCK;
    fcntl(customerFileDescriptor, F_SETLK, &lock);
    close(customerFileDescriptor);

    if (customerID != customer.id) {
        bzero(writeBuffer, sizeof(writeBuffer));
        sprintf(writeBuffer, "\nCustomer with Id: %d not found.", customerID);
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        return false;
    }

    writeBytes = write(connFD, "Which Information To Modify?\n1 --> Name, 2 --> Age, 3 --> Gender\nPress any other key to cancel", strlen("Which Information To Modify?\n1 --> Name, 2 --> Age, 3 --> Gender\nPress any other key to cancel"));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1) {
        perror("Error while getting customer modification menu choice from client!");
        return false;
    }

    int choice = atoi(readBuffer);
    if (choice == 0) {
        // A non-numeric string was passed to atoi
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "It seems you have passed a sequence of alphabets when a number was expected or you have entered an invalid number!\n\nYou'll now be redirected to the main menu!^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    switch (choice) {
    case 1:
        writeBytes = write(connFD, "Updated Name:- ", strlen("Updated Name:- "));
        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error while getting response for customer's new name from client!");
            return false;
        }
        strcpy(customer.name, readBuffer);
        break;
    case 2:
        writeBytes = write(connFD, "Updated Age:- ", strlen("Updated Age:- "));
        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error while getting response for customer's new age from client!");
            return false;
        }
        int updatedAge = atoi(readBuffer);
        if (updatedAge == 0) {
            // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
            bzero(writeBuffer, sizeof(writeBuffer));
            strcpy(writeBuffer, "It seems you have passed a sequence of alphabets when a number was expected or you have entered an invalid number!\nYou'll now be redirected to the main menu!^");
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            readBytes = read(connFD, readBuffer, sizeof(readBuffer));
            return false;
        }
        customer.age = updatedAge;
        break;
    case 3:
        writeBytes = write(connFD, "Updated Gender:- ", strlen("Updated Gender:- "));
        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));
        if (readBytes == -1) {
            perror("Error while getting response for customer's new gender from client!");
            return false;
        }
        customer.gender = readBuffer[0];
        break;
    default:
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, "It seems you've made an invalid menu choice\n\nYou'll now be redirected to the main menu!^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        readBytes = read(connFD, readBuffer, sizeof(readBuffer));
        return false;
    }

    customerFileDescriptor = open(CUSTOMER_FILE, O_WRONLY);
    if (customerFileDescriptor == -1) {
        perror("Error while opening customer file");
        return false;
    }
    offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);
    if (offset == -1) {
        perror("Error while seeking to required customer record!");
        return false;
    }

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1) {
        perror("Error while obtaining write lock on customer record!");
        return false;
    }

    writeBytes = write(customerFileDescriptor, &customer, sizeof(struct Customer));
    if (writeBytes == -1) {
        perror("Error while writing update customer info into file");
    }

    lock.l_type = F_UNLCK;
    fcntl(customerFileDescriptor, F_SETLKW, &lock);
    close(customerFileDescriptor);

    writeBytes = write(connFD, "Modification Successful!\n\nYou'll now be redirected to the main menu!^", strlen("Modification Successful!\n\nYou'll now be redirected to the main menu!^"));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    return true;
}
#endif