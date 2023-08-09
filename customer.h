/**
 * @file server.c
 * Name:- Ajay Kumar
 * Roll Number:- MT2022007
 */

#ifndef CUSTOMER_RECORD
#define CUSTOMER_RECORD

struct Customer {
    int id; // Customer id
    char name[30];  // Cunstomer name
    char gender;    // M -> Male, F -> Female, O -> Others
    int age;    // Customer age
    char loginUsername[30]; // Customer's login id
    char loginPassword[30]; // Customer's login password
    int accountNumber;  // Customer's account number
};

#endif