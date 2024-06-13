#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define mirror_port 7001


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


int main(int argc, char const *argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr, mirror_addr; // this part deals with connection with server

    char buffer[1024] = {0};
    char command[1024] = {0};
    char gf[2048]={0};
    int valid_syntax;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to server.\n");
    printf("Enter a command (or 'quit' to exit):\n");

    while (1) {
        valid_syntax = 1;
        memset(buffer, 0, sizeof(buffer));
        memset(command, 0, sizeof(command));
        fgets(buffer, sizeof(buffer), stdin);
        memcpy(gf, buffer, sizeof(buffer));
        // Remove newline character
        buffer[strcspn(buffer, "\n")] = 0;

        // Parse command
        char* token = strtok(buffer, " ");
        if (token == NULL) {
            valid_syntax = 0;
        } else if (strcmp(token, "findfile") == 0) { // function deals with findfile command
            char* filename = strtok(NULL, " ");
            if (filename == NULL) {
                valid_syntax = 0;
           
                } else {
            sprintf(command, "findfile %s", filename);
        }
    } else if (strcmp(token, "sgetfiles") == 0) {  // function deals with sgetfiles command
        char* size1 = strtok(NULL, " ");
        char* size2 = strtok(NULL, " ");
        if (size1 == NULL || size2 == NULL || atoi(size1) < 0 || atoi(size2) < 0) {
            valid_syntax = 0;
        } else {
            char* unzip = strtok(NULL, " ");
            if (unzip != NULL && strcmp(unzip, "-u") == 0) {
                sprintf(command, "sgetfiles %s %s -u", size1, size2);
            } else {
                sprintf(command, "sgetfiles %s %s", size1, size2);
            }
        }
    } else if (strcmp(token, "dgetfiles") == 0) {  //function deals with dgetfiles command
        char* date1 = strtok(NULL, " ");
        char* date2 = strtok(NULL, " ");
        if (date1 == NULL || date2 == NULL || !check_valid_date(date1) || !check_valid_date(date2)) {
            valid_syntax = 0;
        } else {
            char* unzip = strtok(NULL, " ");
            if (unzip != NULL && strcmp(unzip, "-u") == 0) {
                sprintf(command, "dgetfiles %s %s -u", date1, date2);
            } else {
                sprintf(command, "dgetfiles %s %s", date1, date2);
            }
        }
    } else if (strcmp(token, "getfiles") == 0) {  //function deals with getfiles command
       
        gf[strcspn(gf, "\n")]='\0';
        sprintf(command, gf);
        printf("%s", command);
    } else if (strcmp(token, "gettargz") == 0) { //function deals with gettargz command
        gf[strcspn(gf, "\n")]='\0';
        sprintf(command, gf);
        printf("%s", command);
    } else if (strcmp(token, "quit") == 0) {
        sprintf(command, "quit");
    } else {
        valid_syntax = 0;
    }

    if (!valid_syntax) {
        printf("Invalid syntax. Please try again.\n");
        continue;
    }

    // Send command to server
    send(sockfd, command, strlen(command), 0);

    // Handle response from server
    //handle_response(sockfd);
    char response[1024]={0};
    int valueread=read(sockfd, response, sizeof(response)); // redirection to mirror
    printf("%s\n", response);
    response[strcspn(response, "\n")] = '\0';
    if (strcmp(response, "7001") == 0)
    {
        close(sockfd);   // closing the current server connection
        printf("In here\n");
        // Create a new socket for the mirror server
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        memset(&mirror_addr, '\0', sizeof(mirror_addr));
        mirror_addr.sin_family = AF_INET;
        mirror_addr.sin_port = htons(mirror_port);
        mirror_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        // Connect to the mirror server
        if (connect(sockfd, (struct sockaddr *)&mirror_addr, sizeof(mirror_addr)) == -1) {
            perror("connect");
            exit(EXIT_FAILURE);
        }
     
    }
    else 
    printf("%s\n", response);
    if (strcmp(command, "quit") == 0) {
        break;
    }

    printf("Enter a command (or 'quit' to exit):\n");
}

close(sockfd);
printf("Connection closed.\n");

return 0;
}