/**
 * @file server.c
 * Name:- Ajay Kumar
 * Roll Number:- MT2022007
 */

#ifndef TRANSACTIONS
#define TRANSACTIONS

#include <time.h>

struct Transaction {
    int transactionId;  // Transaction's id
    int accountNumber;  // Account number related to the transaction
    bool operation; // 0 -> withdraw, 1 -> deposit
    long int oldBalance;    // Balance before the transaction
    long int newBalance;    // Balance after the transaction
    time_t transactionTime; // Time of the transaction
};

#endif