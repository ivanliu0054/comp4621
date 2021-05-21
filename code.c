#include <arpa/inet.h> 
#include <fcntl.h>
#include <pthread.h>   
#include <unistd.h>    
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>



#define PATH  "C:\Users\ivan0\OneDrive\桌面" 
                                                        
#define PORT_NO 8888
#define BUFFER_SIZE 1024
#define CONNECTION_NUMBER 5

int thread_count = 0;	// Multiple threads work at the same time.
sem_t mutex;			// To manage thread_counter.

void html_parser(int socket, char *file_name)    // handle html files
{
    char *buffer;
    char *complete_route = (char *)malloc((strlen(PATH) + strlen(file_name)) * sizeof(char));
    FILE *fp;			
	
    strcpy(complete_route, PATH);		 // Merge the file name that requested and path of the root folder
    strcat(complete_route, file_name);

    fp = fopen(complete_route, "r");
    if (fp != NULL) //FILE FOUND
    {
        puts("File Found.");

        fseek(fp, 0, SEEK_END); // Find the file size.
        long read_file = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        send(socket, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n", 44, 0); // Send the header for succesful respond.
        buffer = (char *)malloc(read_file * sizeof(char)); 
        
        fread(buffer, read_file, 1, fp); // Read the html file to buffer.
        write (socket, buffer, read_file); // Send the content of the html file to client.
        free(buffer);
        
        fclose(fp);
    }
    else // If there is not such a file.
    {
        write(socket, "HTTP/1.0 404 Not Found\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body>404 File Not Found</body></html>", strlen("HTTP/1.0 404 Not Found\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body>404 File Not Found</body></html>"));
    }

    free(complete_route);
}

void jpeg_parser(int socket, char *file_name)
{
	char *buffer;
	char *complete_route = (char *)malloc((strlen(PATH) + strlen(file_name)) * sizeof(char));
	int fp;

	strcpy(complete_route, PATH); // Merge the file name that requested and path of the root folder
	strcat(complete_route, file_name);
	puts(complete_route);

	if ((fp = open(complete_route, O_RDONLY)) > 0) //FILE FOUND
	{
		puts("Image Found.");
		int bytes;
		char buffer[BUFFER_SIZE];

		send(socket, "HTTP/1.0 200 OK\r\nContent-Type: image/jpeg\r\n\r\n", 45, 0);
		while ((bytes = read(fp, buffer, BUFFER_SIZE)) > 0) // Read the file to buffer. If not the end of the file, then continue reading the file
			write(socket, buffer, bytes); // Send the file of the jpeg to client.
	}
	else // If there is not such a file.
	{
		write(socket, "HTTP/1.0 404 Not Found\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body>404 File Not Found</body></html>", strlen("HTTP/1.0 404 Not Found\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body>404 File Not Found</body></html>"));
	}

	free(complete_route);
	close(fp);
}

void *connection_parser(void *socket_descriptor)
{
    int request;
    char client_reply[BUFFER_SIZE], *line_of_request [3];
    char *file_name;
    char *extension;

    // Get the socket descriptor.
    int sock = *((int *)socket_descriptor);

    // Get the request.
    request = recv(sock, client_reply, BUFFER_SIZE, 0);

    sem_wait(&mutex);
    thread_count++; 

    if(thread_count > 5)   // If there is 5 request at the same time, other request will be refused.
    {
        char *message = "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body>System is busy right now.</body></html>";
        write(sock, message, strlen(message));
        thread_count--; 
        sem_post(&mutex);
        free(socket_descriptor);
        shutdown(sock, SHUT_RDWR);
        close(sock);
        sock = -1;
        pthread_exit(NULL);
    }
    sem_post(&mutex);

    if (request < 0) 
    {
        puts("Receive failed.");
    }
    else if (request == 0) // receive socket closed. Client disconnected upexpectedly.
    {
        puts("Client disconnected upexpectedly.");
    }
    else // Message received.
    {
        printf("%s", client_reply);
        line_of_request [0] = strtok(client_reply, " \t\n");
        if (strncmp(line_of_request [0], "GET\0", 4) == 0)
        {
            // Parsing the request header.
            line_of_request [1] = strtok(NULL, " \t");
            line_of_request [2] = strtok(NULL, " \t\n");

            if (strncmp(line_of_request [2], "HTTP/1.0", 8) != 0 && strncmp(line_of_request [2], "HTTP/1.1", 8) != 0) // Bad request if not HTTP 1.0 or 1.1
            {
                char *message = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<!doctype html><html><body>400 Bad Request</body></html>";
                write(sock, message, strlen(message));
            }
            else
            {
                char *data[2]; // For parsing the file name and extension.

                file_name = (char *)malloc(strlen(line_of_request [1]) * sizeof(char));
                strcpy(file_name, line_of_request [1]);
                puts(file_name);

                // Getting the file name and extension
                data[0] = strtok(file_name, ".");
                data[1] = strtok(NULL, "."); 

                if(strcmp(data[0], "/favicon") == 0 && strcmp(data[1], "ico")) // Discard the favicon.ico requests.
                {
                    sem_wait(&mutex);
                    thread_count--;
                    sem_post(&mutex);
                    free(socket_descriptor);
                    shutdown(sock, SHUT_RDWR);
                    close(sock);
                    sock = -1;
                    pthread_exit(NULL);
                }
                else if (data[0] == NULL || data[1] == NULL) // If there is not an extension in request or request to just localhost:8888/
                {
                    char *message = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n<!doctype html><html><body>400 Bad Request. (You need to request to jpeg and html files)</body></html>";
                    write(sock, message, strlen(message));
                }
                else
                {

                    if (strcmp(data[1], "html") != 0 && strcmp(data[1], "jpeg") != 0) // If the request is not to html or jpeg files, it will be respond 400 Bad Request
                    {
                        char *message = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\n<!doctype html><html><body>400 Bad Request. Not Supported File Type (Suppoerted File Types: html and jpeg)</body></html>";
                        write(sock, message, strlen(message));
                    }
                    else
                    {
                        if (strcmp(data[1], "html") == 0)
                        {
                            sem_wait(&mutex);						// Prevent two or more thread do some IO operation same time.
                            html_parser(sock, line_of_request [1]);
                            sem_post(&mutex);
                        }
                        else if (strcmp(data[1], "jpeg") == 0)
                        {
                            sem_wait(&mutex);						// Prevent two or more thread do some IO operation same time
                            jpeg_parser(sock, line_of_request [1]);
                            sem_post(&mutex);
                        }
                    }
                    free(extension);
                }
                free(file_name);
            }
        }
    }

    
    free(socket_descriptor);
    shutdown(sock, SHUT_RDWR);
    close(sock);
    sock = -1;
    sem_wait(&mutex);
    thread_count--;
    sem_post(&mutex);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    sem_init(&mutex, 0, 1); 
    int socket_descriptor, new_socket, c, *new_sock;
    struct sockaddr_in server, client;

	/* initialize server socket */
    socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_descriptor == -1)
    {
        puts("Could not create socket");
        return 1;
    }

	/* initialize server address (IP:port) */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT_NO);

	/* bind the socket to the server address */
    if (bind(socket_descriptor, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        puts("Binding failed");
        return 1;
    }

    listen(socket_descriptor, 20);

    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

	/* keep processing incoming requests */
    while ((new_socket = accept(socket_descriptor, (struct sockaddr *)&client, (socklen_t *)&c))) // Accept the connection.
    {
        puts("Connection accepted \n");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;

        if (pthread_create(&sniffer_thread, NULL, connection_parser, (void *)new_sock) < 0) // Create a thread for each request.
        {
            puts("Could not create thread");
            return 1;
        }   
    }

    return 0;
}
