#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <syslog.h>

#define PORT "9000"

bool i_am_done = false;
int sockfd = -1;
FILE *file = NULL;
const char *filename = "/var/tmp/aesdsocketdata";

void signalHandler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        syslog(LOG_INFO, "Caught signal, exitin");
        if (sockfd != -1) {
            close(sockfd);
        }
        if (file != NULL) {
            fclose(file);
        }
        remove(filename);
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    openlog("MyServer", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    // Set signal handlers
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);

    bool daemon_enabled = false;
    if (1 < argc) {
        if (strcmp(argv[1], "-d") == 0) {
            daemon_enabled = true;
        }
    }

    // The socket file descriptor
    sockfd = socket(PF_INET, SOCK_STREAM, 0);

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
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
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

    if (daemon_enabled) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid > 0) {
            // Parent process: Print child PID and exit
            exit(EXIT_SUCCESS);
        }

        // Child process: Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    // Open the file to write the data into that
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

        // Accept the incoming request
        struct sockaddr client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int new_socket = accept(sockfd, &client_addr, &client_addr_size);
        if (new_socket == -1) {
            perror("An error to accept the request");
            exit(EXIT_FAILURE);
        }
        // Log accepted connection with client IP address
        const struct sockaddr_in *addr_in = (struct sockaddr_in *)&client_addr;
        char client_ip[INET_ADDRSTRLEN];
        // Convert IPv4 address to string
        inet_ntop(AF_INET, &(addr_in->sin_addr), client_ip, INET_ADDRSTRLEN);
        // Log the IPv4 address
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // receive data
        char buffer[1024] = {0};
        int bytes_received = 0;
        do {
            bytes_received = recv(new_socket, buffer, sizeof(buffer), 0);
            if (bytes_received == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
            
            // append received data to a file
            fseek(file, 0, SEEK_END); // Move the file position indicator to the end of the file
            fwrite(buffer, sizeof(char), bytes_received, file);
        } while(buffer[bytes_received - 1] != '\n');

        // the communication is finished, send back the result and close
        char line[1024];
        rewind(file); // Rewind the file to the beginning
        while (fgets(line, sizeof(line), file) != NULL) { // Read file line by line and send over socket
            if (send(new_socket, line, strlen(line), 0) == -1) {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
        }
        // Close the connection
        close(new_socket);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    return 0;
}