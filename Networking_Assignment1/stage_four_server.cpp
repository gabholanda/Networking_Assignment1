#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>
#include <mutex>
#undef main

std::mutex clientListMutex;
const int MAX_CLIENTS = 4; // Maximum number of clients allowed
std::vector<TCPsocket> clients; // Vector to store client sockets

TCPsocket init(const char* host, Uint16 port) {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDLNet_Init();
    IPaddress ip;
    SDLNet_ResolveHost(&ip, host, port);
    TCPsocket server = SDLNet_TCP_Open(&ip);
    return server;
}

void cleanup(TCPsocket& server) {
    SDLNet_TCP_Close(server);
    SDLNet_Quit();
    SDL_Quit();
}

void printSocketInfo(TCPsocket client) {
    IPaddress* clientIP = SDLNet_TCP_GetPeerAddress(client);
    Uint32 host = SDLNet_Read32(&clientIP->host);
    Uint16 port = SDLNet_Read16(&clientIP->port);
    printf("Client Connected: IP Address: %d.%d.%d.%d, Port: %d\n",
        (host >> 24) & 0xFF,
        (host >> 16) & 0xFF,
        (host >> 8) & 0xFF,
        host & 0xFF,
        port);
}

void handleClient(TCPsocket client, char* serverMsg) {
    char buffer[1024];
    while (true) {
        int received = SDLNet_TCP_Recv(client, buffer, sizeof(buffer));
        if (received > 0) {
            buffer[received - 1] = '\0'; // Null-terminate the received data
            printf("Received: %s\n", buffer);
            printSocketInfo(client);

            if (strcmp(buffer, "quit") == 0) {
                printf("Client requested to exit. Closing connection.\n");
                break;
            }

            if (serverMsg[0] != '\0') {
                SDLNet_TCP_Send(client, serverMsg, sizeof(serverMsg));
            }
            else {
                const char* response = "Server received your message.";
                SDLNet_TCP_Send(client, response, strlen(response) + 1);
            }
        }
        else {
            printf("Client Disconnected\n");
            break;
        }
    }

    SDLNet_TCP_Close(client);
    std::lock_guard<std::mutex> lock(clientListMutex);
    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

void networkLoop(bool& serverLoop, TCPsocket server, std::vector<std::thread>& clientThreads, char* serverInput) {
    while (serverLoop) {
        TCPsocket client = SDLNet_TCP_Accept(server);
        if (client) {
            std::lock_guard<std::mutex> lock(clientListMutex);
            if (clients.size() < MAX_CLIENTS) {
                printf("Client Connected\n");
                clients.push_back(client);
                std::thread clientThread(handleClient, client, serverInput);
                clientThreads.push_back(std::move(clientThread));
            }
            else {
                const char* fullMessage = "Server is full. Please wait for a free slot.";
                SDLNet_TCP_Send(client, fullMessage, strlen(fullMessage) + 1);
                SDLNet_TCP_Close(client);
                printf("Server full. Rejected connection.\n");
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void serverInputFunction(bool& serverLoop, char* serverInput) {
    while (serverLoop) {
        printf("Server's Main Thread\nType 'quit' to close the server and exit the program\n");
        memset(serverInput, 0, sizeof(serverInput));
        if (fgets(serverInput, sizeof(serverInput), stdin) == NULL) {
            break;
        }
        size_t len = strlen(serverInput);
        if (len > 0 && serverInput[len - 1] == '\n') {
            serverInput[len - 1] = '\0';
        }
        if (strcmp(serverInput, "exit") == 0) {
            printf("Server requested to exit. Closing connection.\n");
            serverLoop = false;
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    TCPsocket server = init(NULL, 8080);
    bool serverLoop = true;
    char serverInput[1024];
    std::thread serverInputThread(serverInputFunction, std::ref(serverLoop), serverInput);
    std::vector<std::thread> clientThreads;
    networkLoop(serverLoop, server, clientThreads, serverInput);
    serverInputThread.join();
    for (std::thread& thread : clientThreads) {
        thread.join();
    }
    cleanup(server);
    return 0;
}
