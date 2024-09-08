#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_PORT 9077
#define BUFFER_SIZE 1024

// Function prototypes
void upload_file(int sockfd, const char *filename, const char *dest_path);
void download_file(int sockfd, const char *filename);
void remove_file(int sockfd, const char *filename);
void request_tar(int sockfd, const char *filetype);

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char command[BUFFER_SIZE];
    char filename[BUFFER_SIZE];
    char dest_path[BUFFER_SIZE];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Client24s$ ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            perror("Failed to read command");
            break;
        }

        // Remove newline character from the command
        command[strcspn(command, "\n")] = '\0';

        // Parse and handle commands
        if (strncmp(command, "ufile ", 6) == 0) {
            if (sscanf(command + 6, "%s %s", filename, dest_path) != 2) {
                printf("Invalid ufile command format.\n");
                continue;
            }
            upload_file(sockfd, filename, dest_path);
        } else if (strncmp(command, "dfile ", 6) == 0) {
            if (sscanf(command + 6, "%s", filename) != 1) {
                printf("Invalid dfile command format.\n");
                continue;
            }
            download_file(sockfd, filename);
        } else if (strncmp(command, "rmfile ", 7) == 0) {
            if (sscanf(command + 7, "%s", filename) != 1) {
                printf("Invalid rmfile command format.\n");
                continue;
            }
            remove_file(sockfd, filename);
        } else if (strncmp(command, "dtar ", 5) == 0) {
            if (sscanf(command + 5, "%s", filename) != 1) {
                printf("Invalid dtar command format.\n");
                continue;
            }
            request_tar(sockfd, filename);
        } else {
            printf("Invalid command.\n");
        }
    }

    close(sockfd);
    return 0;
}

// Function to upload a file to the server
void upload_file(int sockfd, const char *filename, const char *dest_path) {
    char buffer[BUFFER_SIZE];
    int file_fd;
    ssize_t bytes_read;

    // Validate input
    if (filename == NULL || dest_path == NULL || strlen(filename) == 0 || strlen(dest_path) == 0) {
        printf("Invalid filename or destination path.\n");
        return;
    }

    // Prepare the upload command
    snprintf(buffer, BUFFER_SIZE, "ufile %s %s", filename, dest_path);
    send(sockfd, buffer, strlen(buffer), 0);

    // Open the file for reading
    file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file for reading");
        return;
    }

    printf("Uploading file: %s\n", filename);

    // Read from the file and send data to the server
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (send(sockfd, buffer, bytes_read, 0) < 0) {
            perror("Failed to send file data");
            close(file_fd);
            return;
        }
    }

    if (bytes_read < 0) {
        perror("Failed to read file");
    }

    close(file_fd);
}

// Function to download a file from the server
void download_file(int sockfd, const char *filename) {
    char buffer[BUFFER_SIZE];
    int file_fd;
    ssize_t bytes_received;
    char local_filename[BUFFER_SIZE];

    // Validate input
    if (filename == NULL || strlen(filename) == 0) {
        printf("Invalid filename.\n");
        return;
    }

    // Extract the filename from the path
    const char *base_filename = strrchr(filename, '/');
    if (base_filename) {
        base_filename++;  // Skip the '/'
    } else {
        base_filename = filename;  // No path, use the filename as-is
    }

    snprintf(local_filename, sizeof(local_filename), "%s", base_filename);

    // Open the local file for writing
    file_fd = open(local_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("Failed to open file for writing");
        return;
    }

    // Send the download command to the server
    snprintf(buffer, BUFFER_SIZE, "dfile %s", filename);
    if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
        perror("Failed to send command to server");
        close(file_fd);
        return;
    }

    printf("Downloading file: %s\n", filename);

    // Receive the file data from the server and write to the local file
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        if (write(file_fd, buffer, bytes_received) < 0) {
            perror("Failed to write file data");
            close(file_fd);
            return;
        }
    }

    if (bytes_received < 0) {
        perror("Failed to receive file data");
    }

    close(file_fd);
    printf("File %s downloaded successfully\n", local_filename);
}

// Function to request the removal of a file from the server
void remove_file(int sockfd, const char *filename) {
    char buffer[BUFFER_SIZE];

    // Validate input
    if (filename == NULL || strlen(filename) == 0) {
        printf("Invalid filename.\n");
        return;
    }

    // Prepare the remove command
    snprintf(buffer, BUFFER_SIZE, "rmfile %s", filename);
    send(sockfd, buffer, strlen(buffer), 0);

    printf("Request to remove file %s sent.\n", filename);
}

// Function to request a tar file from the server
void request_tar(int sockfd, const char *filetype) {
    char buffer[BUFFER_SIZE];
    int file_fd;
    ssize_t bytes_received;
    char tar_filename[BUFFER_SIZE];
    ssize_t total_bytes_received = 0;
    int error_received = 0; // Flag to indicate if an error message is received

    // Validate input
    if (filetype == NULL || strlen(filetype) == 0) {
        printf("Invalid file type.\n");
        return;
    }

    // Determine the tar file name based on filetype
    if (strcmp(filetype, ".c") == 0) {
        snprintf(tar_filename, sizeof(tar_filename), "cfiles.tar");
    } else if (strcmp(filetype, ".pdf") == 0) {
        snprintf(tar_filename, sizeof(tar_filename), "pdf.tar");
    } else if (strcmp(filetype, ".txt") == 0) {
        snprintf(tar_filename, sizeof(tar_filename), "text.tar");
    } else {
        printf("Invalid file type for tar.\n");
        return;
    }

    // Send the dtar command to the server
    snprintf(buffer, BUFFER_SIZE, "dtar %s", filetype);
    if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
        perror("Failed to send dtar command to server");
        return;
    }

    // Open the tar file for writing
    file_fd = open(tar_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("Failed to open file for writing");
        return;
    }

    printf("Downloading tar file: %s\n", tar_filename);

    // Read from the socket and write to the file until the connection is closed
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        // Check for error messages from the server
        if (strstr(buffer, "Invalid command") != NULL || strstr(buffer, "error") != NULL) {
            printf("Server responded with an error: %s\n", buffer);
            error_received = 1;
            break;
        }

        // Write the received data to the file
        if (write(file_fd, buffer, bytes_received) < 0) {
            perror("Failed to write file data");
            close(file_fd);
            return;
        }
        total_bytes_received += bytes_received;
    }

    if (bytes_received < 0) {
        perror("Failed to receive file data");
    }

    close(file_fd);
    if (error_received) {
        printf("Failed to download %s.\n", tar_filename);
    } else {
        printf("Tar file %s downloaded successfully, %ld bytes received.\n", tar_filename, total_bytes_received);
    }
}
