#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

typedef struct dirent dirent;

const long KB_100 = 102400;

struct stat buff;

void check_arguments(int argc);
int check_extension(char *file);
int check_file_c(char *file);
int check_directory(char *file);
void open_dir(char *path, char *options);
void execute_options(dirent *entry, char *options);
void print_access_rights();
int check_file_size();
void check_options(char *options);
int check_option_g_active(char *options);
int check_option_p_active(char *options);
void create_symlink_file_size_smaller_than_100KB(dirent *entry, char *new_path);
void get_file_without_ext(char *file, char *file_without_ext);
void print_process_status(int pid, int status);
int check_other_options_active(char *options);

int main(int argc, char* argv[]) {
    check_arguments(argc);
    check_options(argv[2]);

    open_dir(argv[1], argv[2]);
}

void open_dir(char *path, char *options) {
    DIR *dir = opendir(path);

    if (!dir) {
        perror("Open directory");
        exit(EXIT_FAILURE);
    }

    dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char new_path[300] = "";
        sprintf(new_path, "%s/%s", path, entry->d_name);

        if (stat(new_path, &buff) == 1) {
            perror("Stat");
            exit(1);
        }

        if (check_file_c(entry->d_name) == 1) {                             // daca nu e fisier c
            if (check_directory(entry->d_name) == 0) {                      // daca este director
                open_dir(new_path, options);
            }

            continue;
        }

        if (check_option_g_active(options) == 1) {                              // daca optiunea g nu este activa
            execute_options(entry, options);
            create_symlink_file_size_smaller_than_100KB(entry, new_path);

            continue;
        }

        pid_t pid;
        int wstat;

        if (check_option_p_active(options) == 1) {                              // daca optiunea p nu este activa
            if ((pid = fork()) < 0) {
                perror("Gcc fork");
                exit(1);
            }

            if (pid == 0) {
                char file_executable[300] = "";
                get_file_without_ext(new_path, file_executable);
                strcat(file_executable, "_exec");
                execlp("gcc", "gcc", "-Wall", "-o", file_executable, new_path, NULL);
            }
        } else {
            int pipe_child_to_parent[2];

            if (pipe(pipe_child_to_parent) < 0) {
                perror("PIPE_CTP:");
                exit(EXIT_FAILURE);
            }

            if ((pid = fork()) < 0) {
                perror("Gcc fork");
                exit(1);
            }

            if (pid == 0) {
                char file_executable[300] = "";
                get_file_without_ext(new_path, file_executable);
                strcat(file_executable, "_exec");

                int pipe_parent_to_child[2];
                int pid2;

                if (pipe(pipe_parent_to_child) < 0) {
                    perror("PIPE_PTC:");
                    exit(EXIT_FAILURE);
                }

                if ((pid2 = fork()) < 0) {
                    perror("Fork error:REDIRECT");
                    exit(EXIT_FAILURE);
                }

                if (pid2 == 0) {
                    close(pipe_parent_to_child[1]);
                    close(pipe_child_to_parent[0]);

                    FILE *stream = fdopen(pipe_parent_to_child[0], "r");

                    char line[500] = "";
                    char all_errors[5000] = "";

                    while (fgets(line, 500, stream)) {
                        if (strstr(line, "error") || strstr(line, "warning")) {
                            strcat(all_errors, line);
                        }
                    }

                    all_errors[strlen(all_errors)] = '\0';

                    if ((dup2(pipe_child_to_parent[1], 1)) < 0) {
                        perror("DUP2");
                        exit(EXIT_FAILURE);
                    }

                    printf("%s", all_errors);

                    close(pipe_parent_to_child[0]);
                    close(pipe_child_to_parent[1]);
                    exit(0);
                }

                close(pipe_parent_to_child[0]);

                if ((dup2(pipe_parent_to_child[1], 2)) < 0){
                    perror("DUP2");
                    exit(EXIT_FAILURE);
                }

                close(pipe_parent_to_child[1]);
                close(pipe_child_to_parent[0]);
                close(pipe_child_to_parent[1]);
                execlp("gcc", "gcc", "-Wall", "-o", file_executable, new_path, NULL);
            }

            FILE *stream = fdopen(pipe_child_to_parent[0], "r");
            char error_line[500];
            int error_counter = 0;
            int warning_counter = 0;
            double result;

            close(pipe_child_to_parent[1]);
            while (fgets(error_line, 500, stream)) {
                if (strstr(error_line, "error")) {
                    error_counter++;
                }

                if (strstr(error_line, "warning")) {
                    warning_counter++;
                }
            }
            close(pipe_child_to_parent[0]);

            if (error_counter >= 1) {
                result = 1;
            } else if (warning_counter == 0) {
                result = 10;
            } else if (warning_counter <= 10) {
                result = 2 + 8 * (10 - warning_counter) / 10.0;
            } else {
                result = 2;
            }

            printf("Rezultatul este: %.2f\n", result);
        }

        if (wait(&wstat) == -1 && errno != 0) {
            perror("Gcc process");
        }
        print_process_status(pid, WEXITSTATUS(wstat));

        if (check_other_options_active(options) == 0) {
            if ((pid = fork()) < 0) {
                perror("Options fork");
                exit(1);
            }

            if (pid == 0) {
                execute_options(entry, options);
                exit(0);
            }

            if (wait(&wstat) == -1) {
                perror("Options process");
            }
            print_process_status(pid, WEXITSTATUS(wstat));
        }

        if ((pid = fork()) < 0) {
            perror("Symlink fork");
            exit(1);
        }

        if (pid == 0) {
            create_symlink_file_size_smaller_than_100KB(entry, new_path);
            exit(0);
        }

        if (wait(&wstat) == -1) {
            perror("Symlink process");
        }
        print_process_status(pid, WEXITSTATUS(wstat));

        printf("\n");
    }
    closedir(dir);
}

void execute_options(dirent *entry, char *options) {
    for (int i = 1; i < strlen(options); i++) {
        switch (options[i]) {
            case 'n':
                printf("File: %s\n", entry->d_name);
                break;
            case 'u':
                printf("User identifier: %u\n", buff.st_uid);
                break;
            case 'a':
                print_access_rights();
                break;
            case 'd':
                printf("Size: %ld bytes\n", buff.st_size);
                break;
            case 'c':
                printf("Links counter: %ld\n", buff.st_nlink);
                break;
            case 'g':
            case 'p':
                continue;
            default:
                printf("Wrong option\n");
                exit(1);
        }
    }
}

void print_access_rights() {
    printf("ACCESS RIGHTS:\n");
    printf("USER:\n");

    if ((buff.st_mode & S_IRUSR) == 0) {
        printf("Read: NO\n");
    } else {
        printf("Read: YES\n");
    }

    if ((buff.st_mode & S_IWUSR) == 0) {
        printf("Write: NO\n");
    } else {
        printf("Write: YES\n");
    }

    if ((buff.st_mode & S_IXUSR) == 0) {
        printf("Execute: NO\n");
    } else {
        printf("Execute: YES\n");
    }

    printf("GROUP:\n");

    if ((buff.st_mode & S_IRGRP) == 0) {
        printf("Read: NO\n");
    } else {
        printf("Read: YES\n");
    }

    if ((buff.st_mode & S_IWGRP) == 0) {
        printf("Write: NO\n");
    } else {
        printf("Write: YES\n");
    }

    if ((buff.st_mode & S_IXGRP) == 0) {
        printf("Execute: NO\n");
    } else {
        printf("Execute: YES\n");
    }

    printf("OTHERS:\n");

    if ((buff.st_mode & S_IROTH) == 0) {
        printf("Read: NO\n");
    } else {
        printf("Read: YES\n");
    }

    if ((buff.st_mode & S_IWOTH) == 0) {
        printf("Write: NO\n");
    } else {
        printf("Write: YES\n");
    }

    if ((buff.st_mode & S_IXOTH) == 0) {
        printf("Execute: NO\n");
    } else {
        printf("Execute: YES\n");
    }
}

void check_arguments(int argc) {
    if (argc < 3) {
        printf("No arguments\n");
        exit(1);
    }
}

void check_options(char *options) {
    for (int i = 0; i < strlen(options); i++) {
        int counter = 1;

        for (int j = i + 1; j < strlen(options); j++) {
            if (options[i] == options[j]) {
                counter++;
            }

            if (counter > 1) {
                printf("Cannot write the same option twice.\n");
                exit(1);
            }
        }
    }
}

int check_other_options_active(char *options) {
    return strlen(options) <= 2;
}

int check_extension(char *file) {
    char *ext = strchr(file, '.');

    if (ext != NULL && (strcmp(ext, ".c") == 0)) {
        return 0;
    }

    return 1;
}

int check_file_c(char *file) {
    if (S_ISREG(buff.st_mode) && (check_extension(file) == 0)) {
        return 0;
    }

    return 1;
}

int check_directory(char *file) {
    if (S_ISDIR(buff.st_mode) && (strcmp(file, ".") != 0) && (strcmp(file, "..") != 0)) {
        return 0;
    }

    return 1;
}

int check_file_size() { // return 0 for files smaller than 100KB
    return buff.st_size >= KB_100;
}

void get_file_without_ext(char *file, char *file_without_ext) {
    strcpy(file_without_ext, file);

    int i;
    for (i = 0; file[i] != '.'; i++) {}
    file_without_ext[i] = '\0';
}

int check_option_g_active(char *options) {
    char *g = strchr(options, 'g');

    return g == NULL;
}

int check_option_p_active(char *options) {
    char *p = strchr(options, 'p');

    return p == NULL;
}

void create_symlink_file_size_smaller_than_100KB(dirent *entry, char *new_path) {
    if (check_file_size() == 0) {
        char new_path_without_ext[300] = "";
        get_file_without_ext(new_path, new_path_without_ext);
        if (symlink(entry->d_name, new_path_without_ext) != 0) {
            perror("Symlink");
        }
    }
}

void print_process_status(int pid, int status) {
    printf("The child process having the pid %d finished with code %d.\n", pid, status);
}