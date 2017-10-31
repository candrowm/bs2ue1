#include <iostream>

#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write

#include <thread>

#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#include <mutex>

#define TRUE   1
#define FALSE  0
#define PORT 9999

int opt = TRUE;
int master_socket , addrlen , new_socket , client_socket[30] , max_clients = 30 , activity, i , valread , sd;
int max_sd;
struct sockaddr_in address;
char buffer[1025];  //data buffer of 1K

std::mutex mtx;

void sendMessageToAnUser(int sd) {
    send(sd , buffer , strlen(buffer) , 0 );
}

void sendMessageToAllUsers(char buffer[]) {
    for(int i = 0; i < max_clients; i++){
        sd = client_socket[i];

        //if valid socket descriptor then send message
        if(sd > 0){
            std::thread messageToAnUserThread(sendMessageToAnUser,sd);
            messageToAnUserThread.join();
            // sendMessageToAnUser(sd);

        }
    }
    std::string emptyString = "";
    strcpy(buffer, emptyString.c_str());
}


int serverListen()
{

    //set of socket descriptors
    fd_set readfds;

    //a message
    char *message = "ECHO Daemon v1.0 \r\n";

    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }



    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while(TRUE)
    {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++)
        {
            //socket descriptor
            sd = client_socket[i];

            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);

            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR))
        {
            printf("select error");
        }

        //If something happened on the master socket , then its an incoming connection
        // --> es gibt was beim master socket zu lesen, da neue Nachricht
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) != strlen(message) )
            {
                perror("send");
            }

            puts("Welcome message sent successfully");

            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++)
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);

                    break;
                }
            }
        }

        //else its some IO operation on some other socket :)
        // --> Es gibt was zu lesen: Entweder neue Nachricht von einem Client oder ein Client ist disconected

        bool newMessage=false;

        for (i = 0; i < max_clients; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET( sd , &readfds))
            {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , buffer, 1024)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    //printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    std::string ipFromSender = inet_ntoa(address.sin_addr);
                    std::string portFromSender = std::to_string(ntohs(address.sin_port));
                    std::string messageFromSender = ipFromSender+":"+portFromSender+" hat den Chat verlassen";

                    strcpy(buffer, messageFromSender.c_str());
                    std::cout << buffer << "\n";

                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;
                }

                    //Echo back the message that came in
                else
                {
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    buffer[valread] = '\0';
                    // std::cout << inet_ntoa(address.sin_addr) <<":"<< ntohs(address.sin_port) << " schrieb: " << buffer << "\n";


                    std::string ipFromSender = inet_ntoa(address.sin_addr);
                    std::string portFromSender = std::to_string(ntohs(address.sin_port));

                    ;                   std::string messageFromSender = ipFromSender+":"+portFromSender+ " schrieb:" + buffer+ " \n";

                    strcpy(buffer, messageFromSender.c_str());

                    //mtx.lock();
                    std::cout << buffer << "\n";
                    // mtx.unlock();



                    // send(sd , buffer , strlen(buffer) , 0 );
                }
            }
        }
        if(strlen(buffer)>0){
            sendMessageToAllUsers(buffer);
        }

    }

    return 0;
}




void serverChat()
{
    while(true){
        std::string vielleicht= "";
        //std::cin >> vielleicht;
        //std::cout << "EINGABE WAR: :" << vielleicht;

        //mtx.lock();
        std::getline(std::cin,vielleicht);

        std::string ipFromSender = "SERVER";
        std::string portFromSender = "PORT" ;
        std::string messageFromSender = ipFromSender+":"+portFromSender+ " schrieb:" + vielleicht + " \n";

        strcpy(buffer, messageFromSender.c_str());
        std::cout << buffer << "\n";
        // mtx.unlock();

        if(strlen(buffer)>0){
            sendMessageToAllUsers(buffer);
        }
    }


}

int client(){
    int sock;
    struct sockaddr_in server;
    char message[1000] , server_reply[2000];

    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( 8888 );

    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return 1;
    }

    puts("Connected\n");

    //keep communicating with server
    while(1)
    {
        printf("Enter message : ");
        scanf("%s" , message);

        //Send some data
        if( send(sock , message , strlen(message) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }

        //Receive a reply from the server
        if( recv(sock , server_reply , 2000 , 0) < 0)
        {
            puts("recv failed");
            break;
        }

        puts("Server reply :");
        puts(server_reply);
    }

    close(sock);
    return 0;

}



int main(int argc , char *argv[]){
    std::thread serverListenThread(serverListen);
    std::thread serverChatThread(serverChat);
    serverListenThread.join();
    serverChatThread.join();
    return 0;
}