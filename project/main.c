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

    if (dir) {
        dirent *entry;

        while ((entry = readdir(dir)) != NULL) {
            char new_path[300]="";
            sprintf(new_path, "%s/%s", path, entry->d_name);

            if (stat(new_path, &buff) == 0) {
                if (check_file_c(entry->d_name) == 0) {
                    if (check_option_g_active(options) == 0) {
                        pid_t pid;
                        int wstat;
                        if ((pid = fork()) < 0) {
                            perror("Gcc fork");
                            exit(1);
                        }

                        if (check_option_p_active(options) == 0) {
                            int pipe_child_to_parent[2];

                            if(pipe(pipe_child_to_parent) < 0) {
                                perror("PIPE_CTP:");
                                exit(EXIT_FAILURE);
                            }

                            if (pid == 0) {
                                char file_executable[300] = "";
                                get_file_without_ext(new_path, file_executable);
                                strcat(file_executable, "_exec");

                                int pipe_parent_to_child[2];
                                int pid2;

                                if(pipe(pipe_parent_to_child) < 0) {
                                    perror("PIPE_PTC:");
                                    exit(EXIT_FAILURE);
                                }

                                if((pid2 = fork()) < 0) {
                                    perror("Fork error:REDIRECT");
                                    exit(EXIT_FAILURE);
                                }

                                if(pid2 == 0) {
                                    close(pipe_parent_to_child[1]);
                                    close(pipe_child_to_parent[0]);
                                    FILE *stream;
                                    char string[500] = "";
                                    char all_errors[5000] = "";

                                    stream = fdopen(pipe_parent_to_child[0], "r");

                                    while (fgets(string, 500, stream)) {
                                        if (strstr(string, "error") || strstr(string, "warning")) {
                                            strcat(all_errors, string);
                                        }
                                    }

                                    all_errors[strlen(all_errors)] = '\0';

//                                    FILE *fis = fopen("file.txt", "a");
//                                    fprintf(fis, "%s", all_errors);

//                                    write(pipe_child_to_parent[1], all_errors, strlen(all_errors));

                                    if ((dup2(pipe_child_to_parent[1], 1)) < 0) {
                                        perror("DUP2");
                                        exit(EXIT_FAILURE);
                                    }
//                                    fclose(fis);

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
                                execlp("gcc", "gcc", "-Wall", "-o", file_executable, new_path, NULL);
                            }

                            FILE *stream = fdopen(pipe_child_to_parent[0], "r");
                            char string[500];

                            close(pipe_child_to_parent[1]);
                            FILE *fis = fopen("file.txt", "a");
                            fprintf(fis, "%s\n", "aaaaabbb");
                            if (fgets(string, 500, stream) == NULL) {
                                fprintf(fis, "%s", "E null\n");
                            }
                            while (fgets(string, 500, stream)) {
                                fprintf(fis, "%s\n", "aaaaa");
                            }
                            fclose(fis);
                            close(pipe_child_to_parent[0]);
                        } else {
                            if (pid == 0) {
                                char file_executable[300] = "";
                                get_file_without_ext(new_path, file_executable);
                                strcat(file_executable, "_exec");
                                execlp("gcc", "gcc", "-Wall", "-o", file_executable, new_path, NULL);
                            }
                        }

                        if (wait(&wstat) == -1 && errno != 0) {
                            perror("Gcc process");
                        }
                        print_process_status(pid, WEXITSTATUS(wstat));

                        if (check_other_options_active(options)) {
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
                    } else {
                        execute_options(entry, options);
                        create_symlink_file_size_smaller_than_100KB(entry, new_path);
                    }
                    printf("\n");
                } else if (check_directory(entry->d_name) == 0) {
                    open_dir(new_path, options);
                }
            } else {
                perror("Stat");
                exit(1);
            }
        }
        closedir(dir);
    } else {
        perror("Open directory");
        exit(1);
    }
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
                continue;
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
    return strlen(options) > 2;
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