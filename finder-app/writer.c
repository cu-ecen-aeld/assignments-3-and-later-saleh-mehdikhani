#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    openlog(NULL, 0, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Number of parameters is not two items");
        exit(1);
    }

    char *file_path = argv[1];
    char *text_str = argv[2];

    // open the file
    FILE *fileh = fopen(file_path, "w");
    int fopen_err = errno;
    if (fileh == NULL) {
        syslog(LOG_ERR, "Error to open the file: %s", strerror(fopen_err));
        exit(1);
    }

    // write to the file
    int num_written = fprintf(fileh, "%s", text_str);
    if (num_written < 0) {
        syslog(LOG_ERR, "Error to write to the file: %s", strerror(fopen_err));
        fclose(fileh);
        exit(1);
    }
    syslog(LOG_DEBUG, "Writing %s to %s", text_str, file_path);

    // close the file
    int close_err = fclose(fileh);
    if (close_err != 0) {
        syslog(LOG_ERR, "Error closing the file: %s", strerror(fopen_err));
        exit(1);
    }

    return 0;
}