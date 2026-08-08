#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

void createFilePath(char *path, char *name) {
    int index = strlen(path);
    path[index] = '/';
    index++;
    while (*name != '\0') {
        path[index] = *name;
        index++;
        name++;
    }

    path[index] = '\0';
}

void findFile(char *path, char *name, int has_exec, int exec_start, int argc, char *argv[]) {
    int fd = open(path, 0);
    if (fd < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return;
    }

    struct dirent de;
    if (st.type == T_DIR) {
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0) {
                continue;
            }
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                continue;
            }

            char new_path[512];
            strcpy(new_path, path);
            createFilePath(new_path, de.name);
            
            if (strcmp(de.name, name) == 0) {
                if (has_exec) {
                    int pid = fork();
                    if (pid < 0) {
                        fprintf(2, "find: fork failed\n");
                    }
                    else if (pid == 0) {
                        char *exec_args[MAXARG];
                        int idx = 0;

                        for (int i = exec_start; i < argc; i++) {
                            exec_args[idx++] = argv[i];
                        }
                        exec_args[idx++] = new_path;
                        exec_args[idx] = 0;

                        exec(exec_args[0], exec_args);
                        fprintf(2, "find: exec %s failed\n", exec_args[0]);
                        exit(1);
                    }
                    else {
                        wait(0);
                    }
                }
                else {
                    printf("%s\n", new_path);
                }
            }

            findFile(new_path, name, has_exec, exec_start, argc, argv);
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(2, "Usage: find <file_path> <file_name>\n");
        exit(1);
    }

    int has_exec = 0;
    int exec_start = -1;
    if (argc >= 5 && strcmp(argv[3], "-exec") == 0) {
        has_exec = 1;
        exec_start = 4;
    }

    findFile(argv[1], argv[2], has_exec, exec_start, argc, argv);
    exit(0);
}