//Headers File
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h> // Include this for 'stat'

#define PORT 9077
#define BUFFER_SIZE 1024
#define MAX_PATH_LEN 4096 // Define this as per your requirement
#define SPDF_PORT 9087  // Define the port for Spdf server
//Global Functions declarations
void prcclient(int client_sock);
void handle_pdf(char *filename, char *dest_path);
void handle_txt(char *filename, char *dest_path);
void save_c_Files(int client_sock, char *filename, char *dest_path);
void send_file_to_client(int client_sock, char *file_path);
void request_file_from_server(char *server_ip, int port, char *filename, int client_sock);
void handle_dfile(int client_sock, char *filename);
void request_deletion_from_server(char *server_ip, int port, char *filename, int client_sock); // Declare this here
void create_tar(int client_sock, const char *root_path, const char *tar_name, const char *file_ext);

int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int opt = 1;
     // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Set socket options
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // Define server address and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
// Start listening for incoming connections
    if (listen(server_sock, 3) < 0)
    {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Smain server listening on port %d...\n", PORT);

    while (1)
    {
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len)) < 0)
        {
            perror("Accept failed");
            continue;
        }
// Fork a new process to handle the client request
        if (fork() == 0)
        {
            close(server_sock);
            prcclient(client_sock);
            close(client_sock);
            exit(0);
        }
        close(client_sock);
    }

    close(server_sock);
    return 0;
}

void prcclient(int client_sock)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char command[BUFFER_SIZE];
    char filename[BUFFER_SIZE];
    char dest_path[BUFFER_SIZE];

    // Read the command and file parameters
    bytes_read = read(client_sock, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0)
    {
        perror("Failed to read command");
        return;
    }
    buffer[bytes_read] = '\0';
    sscanf(buffer, "%s %s %s", command, filename, dest_path);
    printf("Command: %s\n", command);
    printf("Filename: %s\n", filename);
    printf("Destination Path: %s\n", dest_path);
//different command entered by the user.
    if (strcmp(command, "ufile") == 0)
    {
        if (strstr(filename, ".c"))
        {
            save_c_Files(client_sock, filename, dest_path);
            write(client_sock, "C file saved locally.\n", strlen("C file saved locally.\n"));
        }
        else if (strstr(filename, ".pdf"))
        {
            handle_pdf(filename, dest_path);
            write(client_sock, "PDF file forwarded to Spdf.\n", strlen("PDF file forwarded to Spdf.\n"));
        }
        else if (strstr(filename, ".txt"))
        {
            handle_txt(filename, dest_path);
            write(client_sock, "TXT file forwarded to Stext.\n", strlen("TXT file forwarded to Stext.\n"));
        }
        else
        {
            write(client_sock, "Unsupported file type.\n", strlen("Unsupported file type.\n"));
        }
    }
    else if (strncmp(command, "dfile", 5) == 0)
    {
        handle_dfile(client_sock, filename);
    }
    else if (strncmp(command, "rmfile", 6) == 0)
    {
        if (strstr(filename, ".c"))
        {
            if (remove(filename) == 0)
            {
                write(client_sock, "C file deleted locally.\n", strlen("C file deleted locally.\n"));
            }
            else
            {
                perror("Failed to delete file");
                write(client_sock, "Failed to delete C file.\n", strlen("Failed to delete C file.\n"));
            }
        }
        else if (strstr(filename, ".txt"))
        {
            // Send request to Stext for deleting .txt files
            request_deletion_from_server("127.0.0.1", 9079, filename, client_sock);
        }
        else if (strstr(filename, ".pdf"))
        {
            // Send request to Spdf for deleting .pdf files
            request_deletion_from_server("127.0.0.1", SPDF_PORT, filename, client_sock);
        }
        else if (strncmp(command, "dtar", 4) == 0)
        {
            if (strcmp(filename, ".c") == 0)
            {
                create_tar(client_sock, "home/harmand/Project/smain", "cfiles.tar", ".c");
            }
            else if (strcmp(filename, ".pdf") == 0)
            {
                request_file_from_server("127.0.0.1", SPDF_PORT, "pdf.tar", client_sock);
            }
            else if (strcmp(filename, ".txt") == 0)
            {
                request_file_from_server("127.0.0.1", 9079, "text.tar", client_sock);
            }
            else
            {
                write(client_sock, "Unsupported file type for tar.\n", strlen("Unsupported file type for tar.\n"));
            }
        }
        else
        {
            write(client_sock, "Unsupported file type for deletion.\n", strlen("Unsupported file type for deletion.\n"));
        }
    }
    else
    {
        write(client_sock, "Invalid command.\n", strlen("Invalid command.\n"));
    }
}
//function for requesting server to delete the specific files like.c .pdf
void request_deletion_from_server(char *server_ip, int port, char *filename, int client_sock)
{
    int server_sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address or address not supported");
        close(server_sock);
        return;
    }

    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(server_sock);
        return;
    }

    snprintf(buffer, BUFFER_SIZE, "rmfile %s", filename);
    send(server_sock, buffer, strlen(buffer), 0);

    ssize_t bytes_read = recv(server_sock, buffer, BUFFER_SIZE, 0);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        send(client_sock, buffer, bytes_read, 0);
    }

    close(server_sock);
}
//function to downloads c files to client pwd.
void send_file_to_client(int client_sock, char *file_path)
{
    struct stat st;
    if (stat(file_path, &st) != 0)
    {
        perror("File does not exist or cannot be accessed");
        char response[] = "File not found";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0)
    {
        perror("Failed to open file");
        char response[] = "File not found";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0)
    {
        ssize_t bytes_sent = 0;
        while (bytes_sent < bytes_read)
        {
            ssize_t result = send(client_sock, buffer + bytes_sent, bytes_read - bytes_sent, 0);
            if (result < 0)
            {
                perror("Failed to send file data");
                close(file_fd);
                return;
            }
            bytes_sent += result;
        }
    }

    if (bytes_read < 0)
    {
        perror("Failed to read file");
    }

    close(file_fd);
}
// function to request spdf and stext server to downloads files.
void request_file_from_server(char *server_ip, int port, char *filename, int client_sock)
{
    int server_sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address or address not supported");
        close(server_sock);
        return;
    }

    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(server_sock);
        return;
    }

    snprintf(buffer, BUFFER_SIZE, "dfile %s", filename);
    send(server_sock, buffer, strlen(buffer), 0);

    ssize_t bytes_read = recv(server_sock, buffer, BUFFER_SIZE, 0);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        send(client_sock, buffer, bytes_read, 0);
    }

    close(server_sock);
}
//function to uploads c files locally.
void save_c_Files(int client_sock, char *filename, char *dest_path)
{
    int file_fd;
    char file_path[MAX_PATH_LEN];
    snprintf(file_path, sizeof(file_path), "%s/%s", dest_path, filename);

    file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0)
    {
        perror("Failed to open file");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(client_sock, buffer, sizeof(buffer))) > 0)
    {
        if (write(file_fd, buffer, bytes_read) != bytes_read)
        {
            perror("Failed to write to file");
            close(file_fd);
            return;
        }
    }

    if (bytes_read < 0)
    {
        perror("Failed to read from socket");
    }

    close(file_fd);
}
//function to request spdf server to uploads files.
void handle_pdf(char *filename, char *dest_path)
{
    int spdf_sock;
    struct sockaddr_in spdf_addr;
    char buffer[BUFFER_SIZE];

    if ((spdf_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return;
    }

    spdf_addr.sin_family = AF_INET;
    spdf_addr.sin_port = htons(SPDF_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &spdf_addr.sin_addr) <= 0)
    {
        perror("Invalid address or address not supported");
        close(spdf_sock);
        return;
    }

    if (connect(spdf_sock, (struct sockaddr *)&spdf_addr, sizeof(spdf_addr)) < 0)
    {
        perror("Connection failed");
        close(spdf_sock);
        return;
    }

    snprintf(buffer, BUFFER_SIZE, "ufile %s %s", filename, dest_path);
    send(spdf_sock, buffer, strlen(buffer), 0);

    close(spdf_sock);
}
//function to uploads text files and stext to uploads files.
void handle_txt(char *filename, char *dest_path)
{
    int sttext_sock;
    struct sockaddr_in sttext_addr;
    char buffer[BUFFER_SIZE];

    if ((sttext_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return;
    }

    sttext_addr.sin_family = AF_INET;
    sttext_addr.sin_port = htons(9079); // Port for Stext server

    if (inet_pton(AF_INET, "127.0.0.1", &sttext_addr.sin_addr) <= 0)
    {
        perror("Invalid address or address not supported");
        close(sttext_sock);
        return;
    }

    if (connect(sttext_sock, (struct sockaddr *)&sttext_addr, sizeof(sttext_addr)) < 0)
    {
        perror("Connection failed");
        close(sttext_sock);
        return;
    }

    snprintf(buffer, BUFFER_SIZE, "ufile %s %s", filename, dest_path);
    send(sttext_sock, buffer, strlen(buffer), 0);

    close(sttext_sock);
}
//function to handle downloads files 
void handle_dfile(int client_sock, char *filename)
{
    if (strstr(filename, ".c"))
    {
        char file_path[MAX_PATH_LEN];
        snprintf(file_path, sizeof(file_path), "%s", filename);
        send_file_to_client(client_sock, file_path);
        printf("PATH : %s\n",file_path);
    }
    else if (strstr(filename, ".pdf"))
    {
        request_file_from_server("127.0.0.1", SPDF_PORT, filename, client_sock);
    }
    else if (strstr(filename, ".txt"))
    {
        request_file_from_server("127.0.0.1", 9079, filename, client_sock);
    }
    else
    {
        char response[] = "Unsupported file type for download.";
        send(client_sock, response, strlen(response), 0);
    }
}
// creating tar files for c files.
void create_tar(int client_sock, const char *root_path, const char *tar_name, const char *file_ext)
{
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "tar -cvf %s %s/*%s", tar_name, root_path, file_ext);

    system(command); // Create tar file

    char tar_path[MAX_PATH_LEN];
    snprintf(tar_path, sizeof(tar_path), "%s/%s", root_path, tar_name);
    send_file_to_client(client_sock, tar_path);

    // Clean up
    unlink(tar_path);
}
