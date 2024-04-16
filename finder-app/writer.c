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
    fprintf(fileh, "%s", text_str);
    syslog(LOG_DEBUG, "Writing %s to %s", text_str, file_path);

    // close the file
    fclose(fileh);
}