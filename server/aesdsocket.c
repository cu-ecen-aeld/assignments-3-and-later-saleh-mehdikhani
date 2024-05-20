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

#include "queue.h"

#define PORT "9000"
#define INTERVAL_SECONDS 10

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
    bool thread_done;
};

struct mythread {
    pthread_t *id;
    struct thread_arg *arg;
};

// SLIST.
typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    struct mythread value;
    SLIST_ENTRY(slist_data_s) entries;
};
SLIST_HEAD(slisthead, slist_data_s) head;

// list of static functions
static void close_all_sockets();

static void signalHandler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        syslog(LOG_INFO, "Caught signal, exitin");
        close_all_sockets();   
    }
}

static void *thread_function(void *arg) {
    struct thread_arg *thread_arg = (struct thread_arg *)arg;

    // receive data
    char buffer[1024] = {0};
    int bytes_received = 0;

    pthread_mutex_lock(&(thread_arg->fileacc.lock));

    // receive data until when new line is received
    do {
        bytes_received = recv(thread_arg->socketfd, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            perror("recv");
            goto THREAD_EXIT;
        }
        
        // append received data to a file
        fseek(thread_arg->fileacc.file, 0, SEEK_END); // Move the file position indicator to the end of the file
        fwrite(buffer, sizeof(char), bytes_received, thread_arg->fileacc.file);
    } while(buffer[bytes_received - 1] != '\n');

    // the communication is finished, send back the result and close
    char line[1024];
    rewind(thread_arg->fileacc.file); // Rewind the file to the beginning
    while (fgets(line, sizeof(line), thread_arg->fileacc.file) != NULL) { // Read file line by line and send over socket
        if (send(thread_arg->socketfd, line, strlen(line), 0) == -1) {
            perror("Send failed");
            goto THREAD_EXIT;
        }
    }

THREAD_EXIT:
    pthread_mutex_unlock(&(thread_arg->fileacc.lock));
    // Close the connection
    close(thread_arg->socketfd);
    syslog(LOG_INFO, "Closed connection from %s", thread_arg->client_ip);

    thread_arg->thread_done = true;

    return NULL;
}

static void *thread_append_timestamp(void *arg) {
    struct tm *current_time;
    time_t raw_time;

    struct thread_arg *thread_arg = (struct thread_arg *)arg;

    while (1) {
        // Get the current time
        time(&raw_time);
        current_time = localtime(&raw_time);

        // Format the timestamp according to RFC 2822
        char timestamp[128];
        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %T %z", current_time);

        // Append the timestamp to the file
        pthread_mutex_lock(&(thread_arg->fileacc.lock));
        fseek(thread_arg->fileacc.file, 0, SEEK_END); // Move the file position indicator to the end of the file
        fprintf(thread_arg->fileacc.file, "%s\n", timestamp);
        pthread_mutex_unlock(&(thread_arg->fileacc.lock));

        // Sleep for the specified interval
        sleep(INTERVAL_SECONDS);
    }

    return NULL;
}

static void ip2str(struct sockaddr *addr, char *str) {
    const struct sockaddr_in *addr_in = (struct sockaddr_in *)addr; // Log accepted connection with client IP address
    inet_ntop(AF_INET, &(addr_in->sin_addr), str, INET_ADDRSTRLEN); // Convert IPv4 address to string
}

static void join_terminated_threads() {
    slist_data_t *datap = NULL;
    slist_data_t *datatmp = NULL;

    SLIST_FOREACH_SAFE(datap, &head, entries, datatmp) {
        if (datap->value.arg->thread_done == true) {
            pthread_join(*(datap->value.id), NULL);
            SLIST_REMOVE(&head, datap, slist_data_s, entries);
            free(datap->value.arg);
            free(datap->value.id);
            free(datap);
        }
    }
}

void close_all_sockets() {
    // close listening socket
    if (sockfd != -1) {
        close(sockfd);
    }

    // close open connections
    slist_data_t *datap = NULL;
    SLIST_FOREACH(datap, &head, entries) {
        if (datap->value.arg->socketfd != -1) {
            close(datap->value.arg->socketfd);
        }
    }
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

    // Initialize the list
    SLIST_INIT(&head);

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

    // create a thread to write timestamps
    struct thread_arg t_arg = {
        .fileacc = fileacc,
        .thread_done = false
    };
    pthread_t time_thread;
    pthread_create(&time_thread, NULL, thread_append_timestamp, &t_arg);

    while (!i_am_done) {
        slist_data_t *datap = NULL;

        // Listen for the incoming requests
        int lis_ret = listen(sockfd, 1);
        if (lis_ret == -1) {
            perror("Listen error");
            goto MAIN_EXIT;
        }

        // Accept the incoming request
        struct sockaddr client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int new_socket = accept(sockfd, &client_addr, &client_addr_size);
        if (new_socket == -1) {
            perror("An error to accept the request");
            goto MAIN_EXIT;
        }

        // Create thread for each connection
        struct thread_arg *thread_arg = malloc(sizeof(struct thread_arg));
        ip2str(&client_addr, thread_arg->client_ip);
        syslog(LOG_INFO, "Accepted connection from %s", thread_arg->client_ip); // Log the IPv4 address
        thread_arg->fileacc = fileacc;
        thread_arg->socketfd = new_socket;
        thread_arg->thread_done = false;
        pthread_t *new_thread = malloc(sizeof(pthread_t));
        pthread_create(new_thread, NULL, thread_function, thread_arg);

        // Add the thread info to the list
        datap = malloc(sizeof(slist_data_t));
        datap->value.id = new_thread;
        datap->value.arg = thread_arg;
        SLIST_INSERT_HEAD(&head, datap, entries);

        // Iterate on all threads to check if they are done
        join_terminated_threads();
    }

MAIN_EXIT:
    // wait until when all threads are done, then remove memory
    join_terminated_threads();
    // close files
    if (file != NULL) {
        fclose(file);
    }
    remove(filename);

    return 0;
}