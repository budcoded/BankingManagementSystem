/**
 * @file server.c
 * Name:- Ajay Kumar
 * Roll Number:- MT2022007
 */

#ifndef ACCOUNT_RECORD
#define ACCOUNT_RECORD
#define MAX_TRANSACTIONS 10

struct Account {
    int accountNumber;  // Account number
    int owners[2];  // Customer id's of primary and secondary user
    // Regular account or Joint account
    bool isAccountRegular;  // Account regular or not
    bool isActive;  // Account activated or deactivated
    long int balance;   // Account balance
    int transactions[MAX_TRANSACTIONS]; // List of transactions id's
};

#endif