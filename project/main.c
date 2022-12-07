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

struct stat file_details;

void open_dir(char *path, char *options);

// checking methods
void check_arguments(int argc);
int check_extension(char *file);
int check_file_c(char *file);
int check_directory(char *file);
int check_file_size();
void check_options(char *options);
int check_option_g_active(char *options);
int check_option_p_active(char *options);
int check_other_options_active(char *options);

// utils methods
void get_file_without_ext(char *file, char *file_without_ext);
void print_process_status(int pid, int status);
char *contains_error(const char *line);
char *contains_warning(const char *line);

// requirements methods
void execute_options(dirent *file, char *options);
void print_access_rights();
void create_symlink_file_size_smaller_than_100KB(dirent *file, char *new_path);



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

    dirent *file;

    while ((file = readdir(dir)) != NULL) {
        char new_path[300] = "";
        sprintf(new_path, "%s/%s", path, file->d_name);

        if (stat(new_path, &file_details) == 1) {
            perror("Stat");
            exit(1);
        }

        if (check_file_c(file->d_name) == 1) {                             // daca nu e fisier c
            if (check_directory(file->d_name) == 0) {                      // daca este director
                open_dir(new_path, options);
            }

            continue;
        }

        if (check_option_g_active(options) == 1) {                              // daca optiunea g nu este activa
            execute_options(file, options);
            create_symlink_file_size_smaller_than_100KB(file, new_path);

            continue;
        }

        pid_t gcc_pid;
        int wait_status;

        if (check_option_p_active(options) == 1) {                              // daca optiunea p nu este activa
            if ((gcc_pid = fork()) < 0) {
                perror("Gcc fork");
                exit(EXIT_FAILURE);
            }

            if (gcc_pid == 0) {
                char file_executable[300] = "";
                get_file_without_ext(new_path, file_executable);
                strcat(file_executable, "_exec");
                execlp("gcc", "gcc", "-Wall", "-o", file_executable, new_path, NULL);
            }
        } else {
            int pipe_filter_to_parent[2];
            int pipe_gcc_to_filter[2];

            if (pipe(pipe_gcc_to_filter) < 0) {
                perror("PIPE_GTF:");
                exit(EXIT_FAILURE);
            }

            if (pipe(pipe_filter_to_parent) < 0) {
                perror("PIPE_FTP:");
                exit(EXIT_FAILURE);
            }

            if ((gcc_pid = fork()) < 0) {
                perror("Gcc fork");
                exit(EXIT_FAILURE);
            }

            if (gcc_pid == 0) {
                close(pipe_gcc_to_filter[0]);
                close(pipe_filter_to_parent[0]);
                close(pipe_filter_to_parent[1]);

                char file_executable[300] = "";
                get_file_without_ext(new_path, file_executable);
                strcat(file_executable, "_exec");

                if ((dup2(pipe_gcc_to_filter[1], 2)) < 0) {
                    perror("DUP2");
                    exit(EXIT_FAILURE);
                }

                execlp("gcc", "gcc", "-Wall", "-o", file_executable, new_path, NULL);
            }

            pid_t filter_pid;

            if ((filter_pid = fork()) < 0) {
                perror("Gcc fork");
                exit(EXIT_FAILURE);
            }

            if (filter_pid == 0) {
                close(pipe_gcc_to_filter[1]);
                close(pipe_filter_to_parent[0]);

                FILE *stream = fdopen(pipe_gcc_to_filter[0], "r");

                char line[500] = "";
                char all_errors[5000] = "";

                while (fgets(line, 500, stream)) {
                    if (contains_error(line) || contains_warning(line)) {
                        strcat(all_errors, line);
                    }
                }

                all_errors[strlen(all_errors)] = '\0';

                if ((dup2(pipe_filter_to_parent[1], 1)) < 0) {
                    perror("DUP2");
                    exit(EXIT_FAILURE);
                }

                printf("%s", all_errors);

                close(pipe_gcc_to_filter[0]);
                close(pipe_filter_to_parent[1]);
                exit(EXIT_SUCCESS);
            }

            close(pipe_filter_to_parent[1]);
            close(pipe_gcc_to_filter[0]);
            close(pipe_gcc_to_filter[1]);

            FILE *stream = fdopen(pipe_filter_to_parent[0], "r");
            char error_line[500];
            int error_counter = 0;
            int warning_counter = 0;
            double result;

            while (fgets(error_line, 500, stream)) {
                if (contains_error(error_line)) {
                    error_counter++;
                }

                if (contains_warning(error_line)) {
                    warning_counter++;
                }
            }

            close(pipe_filter_to_parent[0]);

            if (waitpid(filter_pid, &wait_status, 0) == -1 && errno != 0) {
                perror("Filter process");
            }
            print_process_status(filter_pid, WEXITSTATUS(wait_status));

            if (error_counter >= 1) {
                result = 1;
            } else if (warning_counter == 0) {
                result = 10;
            } else if (warning_counter <= 10) {
                result = 2 + 8 * (10 - warning_counter) / 10.0;
            } else {
                result = 2;
            }

            printf("Result: %.2f\n", result);
        }

        if (waitpid(gcc_pid, &wait_status, 0) == -1 && errno != 0) {
            perror("Gcc process");
        }
        print_process_status(gcc_pid, WEXITSTATUS(wait_status));

        if (check_other_options_active(options) == 0) {
            pid_t exec_options_pid;

            if ((exec_options_pid = fork()) < 0) {
                perror("Options fork");
                exit(EXIT_FAILURE);
            }

            if (exec_options_pid == 0) {
                execute_options(file, options);
                exit(EXIT_SUCCESS);
            }

            if (waitpid(exec_options_pid, &wait_status, 0) == -1 && errno != 0) {
                perror("Exec options process");
            }
            print_process_status(exec_options_pid, WEXITSTATUS(wait_status));
        }

        pid_t symlink_pid;

        if ((symlink_pid = fork()) < 0) {
            perror("Symlink fork");
            exit(EXIT_FAILURE);
        }

        if (symlink_pid == 0) {
            create_symlink_file_size_smaller_than_100KB(file, new_path);
            exit(EXIT_SUCCESS);
        }

        if (waitpid(symlink_pid, &wait_status, 0) == -1 && errno != 0) {
            perror("Symlink process");
        }
        print_process_status(symlink_pid, WEXITSTATUS(wait_status));

        printf("\n");
    }
    closedir(dir);
}

char *contains_warning(const char *line) { return strstr(line, "warning"); }

char *contains_error(const char *line) { return strstr(line, "error"); }

void execute_options(dirent *file, char *options) {
    for (int i = 1; i < strlen(options); i++) {
        switch (options[i]) {
            case 'n':
                printf("File: %s\n", file->d_name);
                break;
            case 'u':
                printf("User identifier: %u\n", file_details.st_uid);
                break;
            case 'a':
                print_access_rights();
                break;
            case 'd':
                printf("Size: %ld bytes\n", file_details.st_size);
                break;
            case 'c':
                printf("Links counter: %ld\n", file_details.st_nlink);
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

    if ((file_details.st_mode & S_IRUSR) == 0) {
        printf("Read: NO\n");
    } else {
        printf("Read: YES\n");
    }

    if ((file_details.st_mode & S_IWUSR) == 0) {
        printf("Write: NO\n");
    } else {
        printf("Write: YES\n");
    }

    if ((file_details.st_mode & S_IXUSR) == 0) {
        printf("Execute: NO\n");
    } else {
        printf("Execute: YES\n");
    }

    printf("GROUP:\n");

    if ((file_details.st_mode & S_IRGRP) == 0) {
        printf("Read: NO\n");
    } else {
        printf("Read: YES\n");
    }

    if ((file_details.st_mode & S_IWGRP) == 0) {
        printf("Write: NO\n");
    } else {
        printf("Write: YES\n");
    }

    if ((file_details.st_mode & S_IXGRP) == 0) {
        printf("Execute: NO\n");
    } else {
        printf("Execute: YES\n");
    }

    printf("OTHERS:\n");

    if ((file_details.st_mode & S_IROTH) == 0) {
        printf("Read: NO\n");
    } else {
        printf("Read: YES\n");
    }

    if ((file_details.st_mode & S_IWOTH) == 0) {
        printf("Write: NO\n");
    } else {
        printf("Write: YES\n");
    }

    if ((file_details.st_mode & S_IXOTH) == 0) {
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
    int g_active = 0;
    int p_active = 0;

    for (int i = 0; i < strlen(options); i++) {
        int counter = 1;

        if (options[i] == 'g') {
            g_active++;
        }

        if (options[i] == 'p') {
            p_active++;
        }

        for (int j = i + 1; j < strlen(options); j++) {
            if (options[i] == options[j]) {
                counter++;
            }

            if (counter > 1) {
                printf("Cannot write the same option twice.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (g_active == 0 && p_active > 0) {
        printf("Cannot use p without g.\n");
        exit(EXIT_FAILURE);
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
    if (S_ISREG(file_details.st_mode) && (check_extension(file) == 0)) {
        return 0;
    }

    return 1;
}

int check_directory(char *file) {
    if (S_ISDIR(file_details.st_mode) && (strcmp(file, ".") != 0) && (strcmp(file, "..") != 0)) {
        return 0;
    }

    return 1;
}

int check_file_size() {
    return file_details.st_size >= KB_100;
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

void create_symlink_file_size_smaller_than_100KB(dirent *file, char *new_path) {
    if (check_file_size() == 0) {
        char new_path_without_ext[300] = "";
        get_file_without_ext(new_path, new_path_without_ext);
        if (symlink(file->d_name, new_path_without_ext) != 0) {
            perror("Symlink");
        }
    }
}

void print_process_status(int pid, int status) {
    printf("The child process having the pid %d finished with code %d.\n", pid, status);
}