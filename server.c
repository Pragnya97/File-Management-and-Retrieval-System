#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>



#define PORT 8080
#define _XOPEN_SOURCE 500
#define BUFSIZE 1024
#define MIRROR_PORT 7001


void search_directory(char* path, char* filename, char* command) {
    DIR* dir;                                               // this function is used for searching file in all subdirectories 
    struct dirent* entry;
    struct stat st;

    // Open the directory
    if ((dir = opendir(path)) == NULL) {
        printf("Unable to open directory %s\n", path);
        return;
    }

    // Traverse the directory tree
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path to the file or directory
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Check if the entry is a file or a directory
        if (lstat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recurse into the subdirectory
                search_directory(full_path, filename, command);
            } else if (S_ISREG(st.st_mode)) {
                // Check if the file has the desired name
                if (strcmp(entry->d_name, filename) == 0) {
                    time_t c_time = st.st_ctime;
                    sprintf(command, "%s, %ld, %s\n", full_path, st.st_size, ctime(&c_time));
                    closedir(dir);
                    return;
                }
            }
        } else {
            printf("Unable to get information about file %s\n", full_path);
        }
    }

    // Close the directory
    closedir(dir);
}

int check_valid_date(char* date) {
    // Check that the date has the format YYYY-MM-DD
    int year, month, day;
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) {
        return 0;
    }
    if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }
    return 1;
}

int check_valid_filelist(char* filelist) {
    // Check that the file list contains at least one file
    if (strlen(filelist) == 0) {
        return 0;
    }
    return 1;
}

int check_valid_extensions(char* extensions) {
    // Check that the extensions list contains at least one extension
    if (strlen(extensions) == 0) {
        return 0;
    }
    return 1;
}

void processclient(int sockfd) {
    char buffer[1024] = {0};
    char command[1024] = {0};
    char gf[1024]={0};

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        memset(command, 0, sizeof(command));
        memcpy(gf, buffer, sizeof(buffer));

        int valread = read(sockfd, buffer, sizeof(buffer));
        buffer[valread] = '\0';

        // Parse command
        char* token = strtok(buffer, " ");
        if (token == NULL) {
            sprintf(command, "Invalid syntax. Please try again.\n");
        } else if (strcmp(token, "findfile") == 0) {  // execcution of findfile command
            char* filename = strtok(NULL, " ");
            if (filename == NULL) {
                sprintf(command, "Invalid syntax. Please try again.\n");
            } else {
                char root_path[1024];
                sprintf(root_path, "/root/");

                // Search for the file in the root directory
                char command_buf[BUFSIZE];
                sprintf(command_buf, "find %s -maxdepth 1 -name %s -printf \"%%s,%%Tc\n\"",
                        root_path, filename);
                FILE* fp = popen(command_buf, "r");
                char line[BUFSIZE];
                if (fgets(line, BUFSIZE, fp) != NULL) {
                    // File found in root directory
                    sprintf(command, "%s, %s", filename, line);
                } else {
                    // Search for the file in all subdirectories
                    sprintf(command_buf, "find %s -name %s -printf \"%%s,%%Tc\n\"", root_path, filename);
                    fp = popen(command_buf, "r");
                    if (fgets(line, BUFSIZE, fp) != NULL) {
                        // File found in subdirectory
                        sprintf(command, "%s, %s", filename, line);
                    } else {
                        // File not found
                        sprintf(command, "File not found.\n");
                    }
                }
                pclose(fp);
                }
        } else if (strcmp(token, "sgetfiles") == 0) { //executiion of sgetfiles command
            char* size1_str = strtok(NULL, " ");
            char* size2_str = strtok(NULL, " ");
            char* unzip_flag = strtok(NULL, " ");

            if (size1_str == NULL || size2_str == NULL) {
                sprintf(command, "Invalid syntax. Please try again.\n");
            } else {
                int size1 = atoi(size1_str);
                int size2 = atoi(size2_str);
                if (size1 < 0 || size2 < 0 || size1 > size2) {
                    sprintf(command, "Invalid size range. Please try again.\n");
                } else {
                    char root_path[1024];
                    sprintf(root_path, "/root/");

                    // Find files matching the size range
                    char command_buf[BUFSIZE];
                    sprintf(command_buf, "find %s -type f -size +%dk -size -%dk -print0 | xargs -0 tar -czf temp.tar.gz",
                            root_path, size1, size2);
                    system(command_buf);

                    // Check if the unzip flag is provided
                    if (unzip_flag != NULL && strcmp(unzip_flag, "-u") == 0) {
                        // Unzip the file in the current directory
                        system("tar -xzf temp.tar.gz -C /root/Shabbu");
                        sprintf(command, "Files retrieved and unzipped successfully.\n");
                    } else {
                        sprintf(command, "Files retrieved successfully. Use the -u flag to unzip.\n");
                    }
                }
            }
        } else if (strcmp(token, "dgetfiles") == 0) {  // execution of dgetfiles command
            char* date1_str = strtok(NULL, " ");
            char* date2_str = strtok(NULL, " ");
            char* unzip_flag = strtok(NULL, " ");

            if (date1_str == NULL || date2_str == NULL) {
                sprintf(command, "Invalid syntax. Please try again.\n");
            } else {
                char root_path[1024];
                sprintf(root_path, "/root/");

                // Find files matching the date range
                char command_buf[BUFSIZE];
                sprintf(command_buf, "find %s -type f -newermt \"%s\" ! -newermt \"%s\" -print0 | xargs -0 tar -czf temp.tar.gz",
                        root_path, date1_str, date2_str);
                system(command_buf);

                // Check if the unzip flag is provided
                if (unzip_flag != NULL && strcmp(unzip_flag, "-u") == 0) {
                    // Unzip the file in the current directory
                     system("tar -xzf temp.tar.gz -C /root/Shabbu");

                    sprintf(command, "Files retrieved and unzipped successfully.\n");
                } else {
                    sprintf(command, "Files retrieved successfully. Use the -u flag to unzip.\n");
                }
            }
        }else if (strcmp(token, "getfiles") == 0) { //execution of getfiles command
            char* file1 = strtok(NULL, " ");
            char* file2 = strtok(NULL, " ");
            char* file3 = strtok(NULL, " ");
            char* file4 = strtok(NULL, " ");
            char* file5 = strtok(NULL, " ");
            char* file6 = strtok(NULL, " ");

            char* unzip_flag = strtok(NULL, " ");
            int uzip=0;
            if(strcmp(file1, "-u")==0 || strcmp(file2, "-u")==0 || strcmp(file3, "-u")==0 || strcmp(file4, "-u")==0 || strcmp(file5, "-u")==0 || strcmp(file6, "-u")==0 ) 
            uzip=1;
            
            char root_path[1024];
            sprintf(root_path, "/root/");

            // Check if any of the specified files are present
            char command_buf[BUFSIZE];
            sprintf(command_buf, "find %s -type f \\( -iname \"%s\" -o -iname \"%s\" -o -iname \"%s\" -o -iname \"%s\" -o -iname \"%s\" -o -iname \"%s\" \\) -print0 | xargs -0 tar -czf temp.tar.gz",
                    root_path, file1, file2, file3, file4, file5, file6);
            sprintf(command, command_buf);
            sprintf(command, "%s", command_buf);
            int status = system(command_buf);

            // Check if the files were found
            if (status == 0) {
                // Check if the unzip flag is provided
                if ((unzip_flag != NULL )&& (strcmp(unzip_flag, "-u") == 0)) {
                    // Unzip the file in the current directory
                    system("tar -xzf temp.tar.gz -C /root/Shabbu");

                    sprintf(command, "Files retrieved and unzipped successfully.\n");
                } else {
                    if(uzip==1)
                    system("tar -xzf temp.tar.gz -C /root/Shabbu");
                    else
                    sprintf(command, "Files retrieved successfully. Use the -u flag to unzip %s %s %s", unzip_flag, file2, command_buf);
                    //printf("%s\n", unzip_flag);
                }
            } else {
                sprintf(command, "No file found.\n");
            }
    } else if (strcmp(token, "gettargz") == 0) { //execution of gettargz command
        char* extension1 = strtok(NULL, " ");
        char* extension2 = strtok(NULL, " ");
        char* extension3 = strtok(NULL, " ");
        char* extension4 = strtok(NULL, " ");
        char* extension5 = strtok(NULL, " ");
        char* extension6 = strtok(NULL, " ");
        char* unzip_flag = strtok(NULL, " ");
         int uzip=0;
        if(strcmp(extension1, "-u")==0 || strcmp(extension2, "-u")==0 || strcmp(extension3, "-u")==0 || strcmp(extension4, "-u")==0 || strcmp(extension5, "-u")==0 || strcmp(extension6, "-u")==0 ) 
        uzip=1;
        char root_path[1024];
       sprintf(root_path, "/root/");
      

        // Check if any of the specified files are present
        char command_buf[BUFSIZE];
        sprintf(command_buf, "find %s -type f \\( ", root_path);
        if (extension1 != NULL) sprintf(command_buf + strlen(command_buf), "-iname \"*.%s\" -o ", extension1);
        if (extension2 != NULL) sprintf(command_buf + strlen(command_buf), "-iname \"*.%s\" -o ", extension2);
        if (extension3 != NULL) sprintf(command_buf + strlen(command_buf), "-iname \"*.%s\" -o ", extension3);
        if (extension4 != NULL) sprintf(command_buf + strlen(command_buf), "-iname \"*.%s\" -o ", extension4);
        if (extension5 != NULL) sprintf(command_buf + strlen(command_buf), "-iname \"*.%s\" -o ", extension5);
        if (extension6 != NULL) sprintf(command_buf + strlen(command_buf), "-iname \"*.%s\" -o ", extension6);
        sprintf(command_buf + strlen(command_buf), "-false \\) -print0 | xargs -0 tar -czf temp.tar.gz");
        
        int status = system(command_buf);

        // Check if the files were found
        if (status == 0) {
            // Check if the unzip flag is provided
            if (strcmp(gf, "-u") == 0) {
                // Unzip the file in the current directory
               system("tar -xzf temp.tar.gz -C /root/Shabbu");

                sprintf(command, "Files retrieved and unzipped successfully.\n %s %s", extension2, command_buf);
            } else {
                if(uzip==1)
                 
                { system("tar -xzf temp.tar.gz -C /root/Shabbu");
                  break;
                }
                else
                sprintf(command, "Files retrieved successfully. Use the -u flag to unzip.\n");
            }
        } else {
            sprintf(command, "No file found.\n");
        }
    }else if (strcmp(token, "quit") == 0) {
            sprintf(command, "Goodbye.\n");
            break;
        } else {
            sprintf(command, "Invalid syntax. Please try again.\n");
        }

        // Send response to client
        send(sockfd, command, strlen(command), 0);
    }

    close(sockfd);
    exit(0);
}

void redirect_to_mirror(int client_fd) { //redirection function to mirror
    char redirect_msg[1024];
    snprintf(redirect_msg, 1024, "%d\n", MIRROR_PORT);
    send(client_fd, redirect_msg, strlen(redirect_msg), 0);
    close(client_fd);
}

int main(int argc, char const *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    int active_clients=0;

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("New client connected. Forking child process...\n");
        if (active_clients < 4 || (active_clients > 7 && active_clients % 2 == 0)) { ///load balancing from server to mirror
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {  // Child process
            close(server_fd);
            processclient(new_socket);
        } else {  // Parent process
            close(new_socket);
            while (waitpid(-1, NULL, WNOHANG) > 0);  // Clean up zombie processes
        }
    }
    else{
        // redirecting to mirror server
        printf("Redirecting to mirror\n");
            redirect_to_mirror(new_socket);
    }
    active_clients++; //counter for no of connections
    }

    return 0;
}
