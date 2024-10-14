#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>
#include <mutex>

std::mutex clientListMutex;

TCPsocket init(const char* host, Uint16 port)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDLNet_Init();

    IPaddress ip;
    SDLNet_ResolveHost(&ip, host, port);

    TCPsocket server = SDLNet_TCP_Open(&ip);

    return server;
}

void cleanup(TCPsocket &server)
{
    SDLNet_TCP_Close(server);
    SDLNet_Quit();
    SDL_Quit();
}

void printSocketInfo(TCPsocket client) {
    // Retrieve the client's address
    IPaddress* clientIP = SDLNet_TCP_GetPeerAddress(client);
    // Convert IP from network byte order to host byte order
    Uint32 host = SDLNet_Read32(&clientIP->host);
    Uint16 port = SDLNet_Read16(&clientIP->port);

    // Print the client's IP address and port
    printf("Client Connected: IP Address: %d.%d.%d.%d, Port: %d\n",
        (host >> 24) & 0xFF,
        (host >> 16) & 0xFF,
        (host >> 8) & 0xFF,
        host & 0xFF,
        port);
}

void handleClient(TCPsocket client, char* serverMsg)
//void handleClient(TCPsocket client)
{
    char buffer[1024];
    //const char* response = "Server received your message.";

    while (true) {
        if(client)
        {
            int received = SDLNet_TCP_Recv(client, buffer, sizeof(buffer));
            if (received > 0) {
                buffer[received - 1] = '\0';  // Null-terminate the received data
                printf("Received: %s\n", buffer);
                printSocketInfo(client);

                // Check for the exit command
                if (strcmp(buffer, "quit") == 0) {
                    printf("Client requested to exit. Closing connection.\n");
                    break;
                }

                // Send response to the client
                //SDLNet_TCP_Send(client, response, strlen(response) + 1);

                
                //Server message is not empty
                if(serverMsg[0] != '\0')
                    SDLNet_TCP_Send(client, serverMsg, sizeof(serverMsg));
                else
                {
                    //Send default message if server did not send a message
                    const char* response = "Server received your message.";
                    SDLNet_TCP_Send(client, response, strlen(response ) + 1);
                    //SDLNet_TCP_Send(client, response, sizeof(response));
                }
            } else {
                // Client disconnected or error occurred
                printf("Client Disconnected\n");
                break;
            }
        }
    }

    SDLNet_TCP_Close(client);

    std::lock_guard<std::mutex> lock(clientListMutex);
    // Handle client removal from your data structures if necessary
}

void networkLoop(bool &serverLoop, TCPsocket server, std::vector<std::thread> &clientThreads, char *serverInput)
//void networkLoop(bool serverLoop, TCPsocket server, std::vector<std::thread> &clientThreads, char *serverInput)
{
    while (serverLoop) {
        TCPsocket client = SDLNet_TCP_Accept(server);
        if (client) {
            printf("Client Connected\n");


            // Start a new thread to handle communication with the client
            //std::thread clientThread(handleClient, client);
            std::thread clientThread(handleClient, client, serverInput);
            clientThreads.push_back(std::move(clientThread));
        }
        else {
            //printf("Searching for connection\n");
            auto currentTime = std::chrono::system_clock::now();

            // Calculate a time point 5 seconds into the future
            auto wakeUpTime = currentTime + std::chrono::seconds(1);

            // Sleep until the specified time
            std::this_thread::sleep_until(wakeUpTime);
        }
    }

}


//void serverInputFunction(bool serverLoop, char* serverInput)
void serverInputFunction(bool &serverLoop, char* serverInput)
{
    while(serverLoop)
    {
        printf("Server's Main Thread\n Type 'quit' to close the server and exit the program\n");

        memset(serverInput, 0, sizeof(serverInput));
        // Read user input
        if (fgets(serverInput, sizeof(serverInput), stdin) == NULL) {
            break;  // Exit the loop on EOF or error
        }

        // Remove newline character at the end of the input
        size_t len = strlen(serverInput);
        if (len > 0 && serverInput[len - 1] == '\n') {
            serverInput[len - 1] = '\0';
        }

        if (strcmp(serverInput, "exit") == 0) {
            printf("Server requested to exit. Closing connection.\n");
            serverLoop = false;
            break;  // Exit the loop if the user enters 'quit'
        }
    }
}

int main(int argc, char* argv[]) {
    TCPsocket server = init(NULL, 8080);

    bool serverLoop = true;
    char serverInput[1024];

    //memset(serverInput, 0, sizeof(serverInput));
    std::thread serverInputThread(serverInputFunction, std::ref(serverLoop), serverInput);
    //std::thread serverInputThread(serverInputFunction, serverLoop, serverInput);


    // A vector to store threads for each client
    std::vector<std::thread> clientThreads;

    networkLoop(serverLoop, server, clientThreads, serverInput);

    serverInputThread.join();

    // Join all the client threads before cleaning up
    for (std::thread& thread : clientThreads) {
        thread.join();
    }

    cleanup(server);
    return 0;
}

