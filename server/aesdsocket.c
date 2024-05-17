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
#include <pthread.h>

#define PORT "9000"

bool i_am_done = false;
int sockfd = -1;
FILE *file = NULL;
const char *filename = "/var/tmp/aesdsocketdata";

struct file_access {
    FILE *file;
    pthread_mutex_t lock;
};

struct thread_arg {
    struct file_access fileacc;
    int socketfd;
    char client_ip[INET_ADDRSTRLEN];
};

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

void *thread_function(void *arg) {
    struct thread_arg *thread_arg = (struct thread_arg *)arg;

    printf("new thread [%ld] created\n", pthread_self());

    // receive data
    char buffer[1024] = {0};
    int bytes_received = 0;

    pthread_mutex_lock(&(thread_arg->fileacc.lock));
    do {
        bytes_received = recv(thread_arg->socketfd, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        printf("bytes received: %d\n", bytes_received);
        
        // append received data to a file
        fseek(thread_arg->fileacc.file, 0, SEEK_END); // Move the file position indicator to the end of the file
        fwrite(buffer, sizeof(char), bytes_received, thread_arg->fileacc.file);
    } while(buffer[bytes_received - 1] != '\n');

    printf("new data with size %ld received\n", strlen(buffer));

    // the communication is finished, send back the result and close
    char line[1024];
    rewind(thread_arg->fileacc.file); // Rewind the file to the beginning
    while (fgets(line, sizeof(line), thread_arg->fileacc.file) != NULL) { // Read file line by line and send over socket
        if (send(thread_arg->socketfd, line, strlen(line), 0) == -1) {
            perror("Send failed");
            exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_unlock(&(thread_arg->fileacc.lock));

    // Close the connection
    close(thread_arg->socketfd);
    syslog(LOG_INFO, "Closed connection from %s", thread_arg->client_ip);

    printf("thread [%ld] is done\n", pthread_self());

    return NULL;
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
    // create a struct to access the file using mutex
    struct file_access fileacc;
    fileacc.file = file;
    if (pthread_mutex_init(&(fileacc.lock), NULL) != 0) {
        perror("Mutex init has failed\n");
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
        const struct sockaddr_in *addr_in = (struct sockaddr_in *)&client_addr; // Log accepted connection with client IP address

        // Create thread for each connection
        struct thread_arg *thread_arg = malloc(sizeof(struct thread_arg));
        inet_ntop(AF_INET, &(addr_in->sin_addr), thread_arg->client_ip, INET_ADDRSTRLEN); // Convert IPv4 address to string
        syslog(LOG_INFO, "Accepted connection from %s", thread_arg->client_ip); // Log the IPv4 address
        thread_arg->fileacc = fileacc;
        thread_arg->socketfd = new_socket;
        pthread_t *new_thread = malloc(sizeof(pthread_t));
        pthread_create(new_thread, NULL, thread_function, thread_arg);
    }

    sleep(100);

    return 0;
}