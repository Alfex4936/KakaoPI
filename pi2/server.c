#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0

#define PORT 8888
#define MAX_CLIENT 3

void enable_keepalive(int sock)
{
    int yes = 1;
    setsockopt(
        sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int));

    int idle = 120;
    setsockopt(
        sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int));

    int interval = 10;
    setsockopt(
        sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int));

    int maxpkt = 5;
    setsockopt(
        sock, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int));
}

int main(int argc, char const *argv[])
{
    puts("=== PI 2 TCP SERVER");

    int opt = TRUE;
    int master_socket, addrlen, new_socket, client_socket[MAX_CLIENT], activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;

    char buffer[1025]; // data buffer of 1K

    // set of socket descriptors
    fd_set readfds;

    // a message
    char *message = "hello\r\n";

    // initialise all client_socket[] to 0 so not checked
    for (i = 0; i < MAX_CLIENT; i++)
    {
        client_socket[i] = 0;
    }

    // create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this SO_KEEPALIVE |
    if (setsockopt(master_socket, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, (char *)&opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // enable_keepalive(master_socket);

    // setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))

    // type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    // try to specify maximum of 5 pending connections for the master socket
    if (listen(master_socket, 5) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while (TRUE)
    {
        // clear the socket set
        FD_ZERO(&readfds);

        // add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // add child sockets to set
        for (i = 0; i < MAX_CLIENT; i++)
        {
            // socket descriptor
            sd = client_socket[i];

            // if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            // highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            printf("select error");
        }

        // If something happened on the master socket ,
        // then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            // if buffer is not none, clear
            memset(buffer, 0, strlen(buffer));
            if ((new_socket = accept(master_socket,
                                     (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // enable_keepalive(new_socket);

            // inform user of socket number - used in send and receive commands
            printf("\n===== New connection: socket fd = %d , ip = %s , port = %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            // send new connection greeting message
            if (send(new_socket, message, strlen(message), 0) != strlen(message))
            {
                perror("send");
            }

            puts("Welcome message sent successfully!");

            // add new socket to array of sockets
            for (i = 0; i < MAX_CLIENT; i++)
            {
                // if position is empty
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    enable_keepalive(client_socket[i]);
                    printf("Adding to list of sockets as socket[%d]\n", i);
                    break;
                }
            }
        }

        // printf("OK Else\n");

        // else its some IO operation on some other socket
        for (i = 0; i < MAX_CLIENT; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET(sd, &readfds))
            {
                // Check if it was for closing , and also read the
                // incoming message
                if ((valread = read(sd, buffer, 1024)) == 0)
                {
                    // printf("Came here...\n");
                    // Somebody disconnected , get his details and print
                    getpeername(sd, (struct sockaddr *)&address,
                                (socklen_t *)&addrlen);
                    printf("\n===== Host disconnected: ip = %s, port = %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    close(sd);
                    client_socket[i] = 0;
                }
                else
                {
                    for (i = 0; i < MAX_CLIENT; i++)
                    {
                        if (client_socket[i] == 0)
                        {
                            printf("socket[%d] is something wrong\n", i);
                            sleep(3);
                            continue;
                        }
                    }
                    buffer[valread] = '\0';
                    if (strlen(buffer) == 0)
                    {
                        continue;
                    }
                    printf("TCP SERVER: Sending... %s\n", buffer);
                    if (strncmp(buffer, "print", 5) != 0)
                    {
                        send(client_socket[0], buffer, strlen(buffer), 0);
                        send(client_socket[1], buffer, strlen(buffer), 0);
                        send(client_socket[2], buffer, strlen(buffer), 0);
                    }
                    else
                    {
                        send(client_socket[2], buffer, strlen(buffer), 0);
                    }
                }
            }
        }
    }
    return 0;
}
