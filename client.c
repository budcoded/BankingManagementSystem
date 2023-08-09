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
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    // Opens a connection on socket file descriptor to peer at address
    int connectStatus = connect(socketFD, (struct sockaddr *)&address, sizeof(address));
    if (connectStatus == -1) {
        perror("Error while connecting with the server!");
        close(socketFD);
        _exit(0);
    }

    // Buffer for communication with server
    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;
    char tempBuffer[1000];

    do {
        // Clearing the Buffers
        bzero(readBuffer, sizeof(readBuffer));
        bzero(tempBuffer, sizeof(tempBuffer));

        readBytes = read(socketFD, readBuffer, sizeof(readBuffer));
        // Handling signals sent by the server
        if (readBytes == -1)
            perror("Error while reading from client socket!");
        else if (readBytes == 0)
            printf("No error received from server! Closing the connection to the server now!\n");
        else if (strchr(readBuffer, '^') != NULL) {
            // Not reading anything from the client
            strncpy(tempBuffer, readBuffer, strlen(readBuffer) - 1);
            printf("%s\n", tempBuffer);
            writeBytes = write(socketFD, "^", strlen("^"));
            if (writeBytes == -1) {
                perror("Error while writing to client socket!");
                break;
            }
        } else if (strchr(readBuffer, '$') != NULL) {
            // Server sending signal to close the connection
            strncpy(tempBuffer, readBuffer, strlen(readBuffer) - 2);
            printf("%s\n", tempBuffer);
            printf("Closing the connection to the server now!\n");
            break;
        } else {
            bzero(writeBuffer, sizeof(writeBuffer));
            if (strchr(readBuffer, '#') != NULL)    // Taking password Input
                strcpy(writeBuffer, getpass(readBuffer));
            else {  // Taking input from the client
                printf("%s\n", readBuffer);
                scanf("%[^\n]%*c", writeBuffer);
            }

            writeBytes = write(socketFD, writeBuffer, strlen(writeBuffer));
            if (writeBytes == -1) {
                perror("Error while writing to client socket!");
                printf("Closing the connection to the server now!\n");
                break;
            }
        }
    } while (readBytes > 0);

    close(socketFD);
}