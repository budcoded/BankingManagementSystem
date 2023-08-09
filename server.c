/**
 * @file server.c
 * Name:- Ajay Kumar
 * Roll Number:- MT2022007
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "./admin_functions.h"
#include "./customer_functions.h"

void main() {
    // Internet socket address structure
    struct sockaddr_in address;
    // IPv4
    address.sin_family = AF_INET;
    // Port address for socket structure
    address.sin_port = htons(8081);
    // Internet address
    // INADDR_ANY --> Address to accept any incoming messages
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    // Creating socket via socket system call
    // SOCK_STREAM --> Sequenced, reliable, connection-based byte streams.
    int socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);

    // Binds socket address to local address
    int socketBindStatus = bind(socketFileDescriptor, (struct sockaddr *)&address, sizeof(address));
    
    // Listen system call
    // preparing to accept connections on socket file descriptor
    // Here only 10 connection requests will be queued after that all requests will be refused
    int socketListenStatus = listen(socketFileDescriptor, 10);

    int clientSize;
    while (1) {
        clientSize = (int)sizeof(address);
        // Await a connection on socket File Descriptor
        // When a connection arrives, open a new socket to communicate with it
        // sets address to the address of the connecting peer
        // and return the new socket's descriptor
        int connectionFileDescriptor = accept(socketFileDescriptor, (struct sockaddr *)&address, &clientSize);
        if (!fork()) {
            // Child Area
            printf("Client is now connected to the server!\n");

            // Taking buffers for communication
            char readBuffer[1000], writeBuffer[1000];
            ssize_t readBytes, writeBytes;

            // showing welcome message to the client
            writeBytes = write(connectionFileDescriptor, "Welcome to the Bank!\n1. Admin\t2. Customer\nPress any other number to exit!", strlen("Welcome to the Bank!\n1. Admin\t2. Customer\nPress any other number to exit!"));
            
            // reading input from the client
            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connectionFileDescriptor, readBuffer, sizeof(readBuffer));
            if (readBytes == -1)
                perror("Error while reading from client!");
            else if (readBytes == 0)
                printf("No data was sent by the client!");
            else {
                // Converting string to integer
                int userChoice = atoi(readBuffer);
                switch (userChoice) {
                case 1:
                    adminOperationHandler(connectionFileDescriptor);
                    break;
                case 2:
                    customerOperationHandler(connectionFileDescriptor);
                    break;
                default:
                    break;
                }
            }

            printf("Terminating connection with the client!\n");
            close(connectionFileDescriptor);
            _exit(0);
        }
    }
    close(socketFileDescriptor);
}