#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int openFile(char *fileName) {
    int fd = open(fileName, O_RDONLY);
    if (fd < 0) {
        fprintf(2, "sixfive: cannot open %s\n", fileName);
        exit(1);
    }

    return fd;
}

int checkSeparator(char c) {
    return strchr(" -\r\t\n./,", c) != 0;
}

void findSixFive(int fd) {
    char c;
    int bytes_read;
    int current_num = 0;
    int number_full = 0;
    int is_valid = 1;

    while ((bytes_read = read(fd, &c, 1)) > 0) {
        if (!is_valid) {
            if (checkSeparator(c)) is_valid = 1;
        }
        else {
            if (c >= '0' && c <= '9') {
                current_num = (current_num * 10) + (c - '0');
                number_full = 1;
            }
            else {
                if (checkSeparator(c)) {
                    if (number_full) {
                        if (current_num % 5 == 0 || current_num % 6 == 0) {
                            printf("%d\n", current_num);
                        }
                    }
                }
                else {
                    is_valid = 0;
                }
                number_full = 0;
                current_num = 0;
            }
        }
    }

    if (bytes_read == 0 && number_full && is_valid) {
        if (current_num % 5 == 0 || current_num % 6 == 0) {
            printf("%d\n", current_num);
        }
    }
    else if (bytes_read < 0) {
        fprintf(2, "read error\n");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: sixfive <file name>.txt\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        int fd = openFile(argv[i]);
        findSixFive(fd);
        close(fd);
    }

    exit(0);
}