#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT "9000"

bool i_am_done = false;

int main(int argc, char *argv[]) {
    bool daemon = false;
    if (1 < argc) {
        if (strcmp(argv[1], "-d") == 0) {
            daemon = true;
        }
    }

    if (daemon) {
        printf("Daemon is set\n");
    }

    // The socket file descriptor
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);

    // The address to be used by the socket
    struct addrinfo *servinfo;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int addr_ret = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (addr_ret != 0) {
        perror("can't get the address info");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port
    int bind_ret = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (bind_ret != 0) {
        perror("Error to bind the socket and the address");
        exit(EXIT_FAILURE);
    }

    // release the memory used for servinfo
    freeaddrinfo(servinfo);

    // Open the file to write the data into that
    FILE *file;
    const char *filename = "/var/tmp/aesdsocketdata";
    file = fopen(filename, "a+");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    while (!i_am_done) {
        // Listen for the incoming requests
        int lis_ret = listen(sockfd, 1);
        if (lis_ret == -1) {
            perror("Listen error");
            exit(EXIT_FAILURE);
        }
        printf("Server listening on port %s...\n", PORT);

        // Accept the incoming request
        struct sockaddr client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int new_socket = accept(sockfd, &client_addr, &client_addr_size);
        if (new_socket == -1) {
            perror("An error to accept the request");
            exit(EXIT_FAILURE);
        }

        // receive data
        char buffer[1024] = {0};
        int bytes_received = 0;
        do {
            bytes_received = recv(new_socket, buffer, sizeof(buffer), 0);
            if (bytes_received == -1) {
                perror("recv");
                goto EXIT;
            }

            printf("Received data: %s", buffer);
            
            // append received data to a file
            fseek(file, 0, SEEK_END); // Move the file position indicator to the end of the file
            fwrite(buffer, sizeof(char), bytes_received, file);
            //fprintf(file, "%s", buffer);
        } while(buffer[bytes_received - 1] != '\n');

        // the communication is finished, send back the result and close
        char line[1024];
        rewind(file); // Rewind the file to the beginning
        while (fgets(line, sizeof(line), file) != NULL) { // Read file line by line and send over socket
            if (send(new_socket, line, strlen(line), 0) == -1) {
                perror("Send failed");
                goto EXIT;
            }
        }
        // Close the connection
        close(new_socket);
    }

    // Close the sockets and exit
EXIT:
    close(sockfd);
    fclose(file);
    

    printf("closing the application");
    return 0;
}