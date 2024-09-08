#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define PDF_PORT 9087      // Port number on which the server will listen
#define BUFFER_SIZE 1024   // Buffer size for reading and writing data

// Function prototypes
void save_pdf_file(int client_sock, char *filename, char *dest_path);
void delete_file(char *filepath);
void send_file(int client_sock, char *filepath);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Create a socket for the server
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to any available network interface
    server_addr.sin_port = htons(PDF_PORT);    // Set port number

    // Bind the socket to the address and port
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_sock, 3) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Spdf server listening on port %d...\n", PDF_PORT);

    // Infinite loop to accept and handle client connections
    while (1) {
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
            perror("Accept failed");
            continue;
        }
        
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;

        // Read the command and file details from the client
        bytes_read = read(client_sock, buffer, BUFFER_SIZE);
        buffer[bytes_read] = '\0';  // Null-terminate the string

        // Check if the command is `ufile`, `rmfile`, or `dfile`
        if (strncmp(buffer, "ufile", 5) == 0) {
            char filename[BUFFER_SIZE];
            char dest_path[BUFFER_SIZE];
            sscanf(buffer, "ufile %s %s", filename, dest_path);
            save_pdf_file(client_sock, filename, dest_path);
        } else if (strncmp(buffer, "rmfile", 6) == 0) {
            char filepath[BUFFER_SIZE];
            sscanf(buffer, "rmfile %s", filepath);
            delete_file(filepath);
        } else if (strncmp(buffer, "dfile", 5) == 0) {
            char filepath[BUFFER_SIZE];
            sscanf(buffer, "dfile %s", filepath);
            send_file(client_sock, filepath);
        } else {
            printf("Unknown command: %s\n", buffer);
        }

        close(client_sock);  // Close the client socket
    }

    close(server_sock);  // Close the server socket
    return 0;
}

// Function to save a PDF file received from the client
void save_pdf_file(int client_sock, char *filename, char *dest_path) {
    char newPath[256];  // Buffer to hold the new path with "spdf" replacement
    char *replacePos;

    // Find the substring "/smain/"
    replacePos = strstr(dest_path, "/smain/");
    if (replacePos != NULL) {
        // Replace "/smain/" with "/spdf/"
        strncpy(newPath, dest_path, replacePos - dest_path);
        newPath[replacePos - dest_path] = '\0';  // Null-terminate the string
        sprintf(newPath + (replacePos - dest_path), "/spdf%s", replacePos + 6);
    } else {
        // If "/smain/" not found, copy the original path
        strcpy(newPath, dest_path);
    }
    printf("Saving %s to %s in Spdf server.\n", filename, newPath);

    // Construct the full file path
    char filepath[BUFFER_SIZE];
    snprintf(filepath, BUFFER_SIZE, "%s/%s", newPath, filename);
    
    // Open the file for writing
    int file_fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("Failed to open file for writing");
        return;
    }
    
    char data[BUFFER_SIZE];
    ssize_t bytes;
    
    // Read data from the client and write it to the file
    while ((bytes = read(client_sock, data, BUFFER_SIZE)) > 0) {
        if (write(file_fd, data, bytes) != bytes) {
            perror("Failed to write to file");
            close(file_fd);
            return;
        }
    }
    close(file_fd);
}

// Function to delete a file based on the received filepath
void delete_file(char *filepath) {
    char newPath[256];  // Buffer to hold the new path with "spdf" replacement
    char *replacePos;

    // Find the substring "/smain/"
    replacePos = strstr(filepath, "/smain/");
    if (replacePos != NULL) {
        // Replace "/smain/" with "/spdf/"
        strncpy(newPath, filepath, replacePos - filepath);
        newPath[replacePos - filepath] = '\0';  // Null-terminate the string
        sprintf(newPath + (replacePos - filepath), "/spdf%s", replacePos + 6);
    } else {
        // If "/smain/" not found, copy the original path
        strcpy(newPath, filepath);
    }
    printf("Filename: %s", filepath);
    printf("Attempting to delete file: %s\n", newPath);

    // Attempt to remove the file
    if (remove(newPath) == 0) {
        printf("File deleted successfully: %s\n", newPath);
    } else {
        perror("Failed to delete file");
    }
}

// Function to send a file to the client
void send_file(int client_sock, char *filepath) {
    char newPath[256];  // Buffer to hold the new path with "spdf" replacement
    char *replacePos;

    // Find the substring "/smain/"
    replacePos = strstr(filepath, "/smain/");
    if (replacePos != NULL) {
        // Replace "/smain/" with "/spdf/"
        strncpy(newPath, filepath, replacePos - filepath);
        newPath[replacePos - filepath] = '\0';  // Null-terminate the string
        sprintf(newPath + (replacePos - filepath), "/spdf%s", replacePos + 6);
    } else {
        // If "/smain/" not found, copy the original path
        strcpy(newPath, filepath);
    }

    // Open the file for reading
    int file_fd = open(newPath, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file for reading");
        char response[] = "File does not exist or cannot be accessed.";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    printf("Sending file: %s\n", newPath);

    // Read the file and send it to the client
    ssize_t bytes_read;
    char file_buffer[BUFFER_SIZE];
    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        if (send(client_sock, file_buffer, bytes_read, 0) != bytes_read) {
            perror("Failed to send file");
            break;
        }
    }

    if (bytes_read < 0) {
        perror("Failed to read file");
    }

    // Log file send completion
    printf("File sent successfully: %s\n", filepath);

    close(file_fd);
}
