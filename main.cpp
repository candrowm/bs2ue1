#include <iostream>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include <thread>
#include <mutex>

#define PORT 9999
#define MAXTHREADS 50


int threadFileDescriptors[MAXTHREADS] = {};
pthread_cond_t threadConditions[MAXTHREADS] = {};
pthread_mutex_t threadMutexes[MAXTHREADS] = {};


char buffer[1025];  //data buffer of 1


struct arg_struct {
    int fd;
};


int findPositionInArray(int val, int *arr, int size) {
    int i;
    for (i = 0; i < size; i++) {
        if (arr[i] == val)
            return i;
    }
    return -1;
}

void *socketThread(void *arguments) {
    struct arg_struct *args = (struct arg_struct *) arguments;

    std::cout << "New Thread with fd: " << args->fd << "\n";

    int threadFileDescriptor = args->fd;

    int positionOfClient = findPositionInArray(threadFileDescriptor, threadFileDescriptors, MAXTHREADS);


    while (true) {
        pthread_mutex_lock(&threadMutexes[positionOfClient]);
        pthread_cond_wait(&threadConditions[positionOfClient], &threadMutexes[positionOfClient]);
        // printf("Got SocketThread Cond \n");
        pthread_mutex_unlock(&threadMutexes[positionOfClient]);

        try {
            send(threadFileDescriptor, buffer, strlen(buffer), 0);
        } catch (std::string exception) {
            threadFileDescriptors[positionOfClient] = 0;
            pthread_exit(NULL);
        }
    }

}


void writeMessageToAllUsers(std::string buffer) {

    for (int i = 0; i < MAXTHREADS + 1; i++) {
        if (threadFileDescriptors[i] > 0) {
            pthread_mutex_lock(&threadMutexes[i]);
            // printf("SocketThread Cond fire\n");
            pthread_cond_signal(&threadConditions[i]);
            pthread_mutex_unlock(&threadMutexes[i]);
        }
    }
}

void *masterSocketThread(void *ptr) {
    int master_socket, addrlen, new_socket, activity, valread, sd;
    fd_set readfds;
    int max_sd;
    struct sockaddr_in address;
    char *message = "Welcome!\n";

    //initialise all client_socket[] to 0 so not checked
    for (int i = 0; i < MAXTHREADS; i++) {
        threadFileDescriptors[i] = 0;
    }


    //create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }



    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Wait for connections ...");

    for (;;) {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for (int i = 0; i < MAXTHREADS; i++) {
            //socket descriptor
            sd = threadFileDescriptors[i];

            //if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            //highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);


        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n", new_socket,
                   inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            //send new connection greeting message
            if (send(new_socket, message, strlen(message), 0) != strlen(message)) {
                perror("send");
            }


            //add new socket to array of sockets
            for (int i = 0; i < MAXTHREADS; i++) {
                //if position is empty
                if (threadFileDescriptors[i] == 0) {

                    //printf("Adding new socket to list %d\n", i);

                    pthread_mutex_t new_mutex = PTHREAD_MUTEX_INITIALIZER;
                    pthread_cond_t new_cond = PTHREAD_COND_INITIALIZER;

                    struct arg_struct args;
                    args.fd = new_socket;

                    threadFileDescriptors[i] = new_socket;
                    threadMutexes[i] = new_mutex;
                    threadConditions[i] = new_cond;

                    pthread_t clientSocketThread;
                    int err = pthread_create(&clientSocketThread, NULL, socketThread, &args);

                    break;
                }
            }


        } else
            //else its some IO operation on some other socket :) --> New message from client or cliebt disconnected
            for (int i = 0; i < MAXTHREADS; i++) {
                sd = threadFileDescriptors[i];

                if (FD_ISSET(sd, &readfds)) {
                    //Check if it was for closing , and also read the incoming message
                    if ((valread = read(sd, buffer, 1024)) == 0) {
                        //Somebody disconnected , get his details and print
                        getpeername(sd, (struct sockaddr *) &address, (socklen_t *) &addrlen);

                        std::string ipFromSender = inet_ntoa(address.sin_addr);
                        std::string portFromSender = std::to_string(ntohs(address.sin_port));
                        std::string messageFromSender = ipFromSender + ":" + portFromSender + " disconnected \n";


                        strcpy(buffer, messageFromSender.c_str());
                        std::cout << buffer;

                        //Close the socket and mark as 0 in list for reuse
                        close(sd);
                        threadFileDescriptors[i] = 0;

                        writeMessageToAllUsers(buffer);
                    }
                        //Echo back the message that came in
                    else {
                        getpeername(sd, (struct sockaddr *) &address, (socklen_t *) &addrlen);
                        buffer[valread] = '\0';

                        std::string ipFromSender = inet_ntoa(address.sin_addr);
                        std::string portFromSender = std::to_string(ntohs(address.sin_port));

                        std::string messageFromSender = ipFromSender + ":" + portFromSender + ": " + buffer;

                        strcpy(buffer, messageFromSender.c_str());

                        std::cout << buffer;
                        writeMessageToAllUsers(buffer);
                    }
                }
            }
    }
}


void *serverChatThread(void *ptr) {
    while (true) {
        std::string serverMessage = "";

        //mtx.lock();
        std::getline(std::cin, serverMessage);

        std::string portOfServer = std::to_string(PORT);

        std::string ipFromSender = "Server";
        std::string portFromSender = PORT + "";
        std::string messageFromSender = ipFromSender + ":" + portOfServer + ": " + serverMessage + " \n";

        strcpy(buffer, messageFromSender.c_str());
        //std::cout << buffer << "\n";

        if (strlen(buffer) > 0) {
            writeMessageToAllUsers(buffer);
        }
    }
}



int main(int argc, char *argv[]) {

    pthread_t serverSocketThread;
    int errorServerSocketThread = pthread_create(&serverSocketThread, NULL, masterSocketThread, NULL);

    pthread_t chatServerThread;
    int errorChatServerThread = pthread_create(&chatServerThread, NULL, serverChatThread, NULL);

    pthread_join(serverSocketThread, NULL);

    if (errorServerSocketThread != 0 || errorChatServerThread != 0) {
        printf("\ncan't create thread :[%s]", strerror(errorChatServerThread));
    } else {
        printf("\n Thread created successfully\n");
    }

    return 0;
}