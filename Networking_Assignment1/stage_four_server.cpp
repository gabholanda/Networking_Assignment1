#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <iostream>
#include <vector>
#include <list>
#include <thread>
#include <mutex>
#include <map>
#include <chrono>
#undef main

class Network {
public:
    Network();
    ~Network();

    TCPsocket init(const char* host, Uint16 port);
    void printSocketInfo(TCPsocket client);
    void handleClient(TCPsocket client, const char* serverMsg);
    void networkLoop(bool& serverLoop, const char* serverInput);
    void serverInputFunction(bool& serverLoop, char* serverInput);

private:
    std::mutex clientListMutex;
    std::list<TCPsocket> clients;
    std::vector<std::thread> clientThreads;
    const int MAX_CLIENTS = 4;
    TCPsocket server;
    std::map<TCPsocket, std::chrono::steady_clock::time_point> clientMessageTimestamps;
};

Network::Network() : server(nullptr) {}

Network::~Network() {
    if (server) {
        SDLNet_TCP_Close(server);
    }
    SDLNet_Quit();
    SDL_Quit();
}

TCPsocket Network::init(const char* host, Uint16 port) {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDLNet_Init();
    IPaddress ip;
    SDLNet_ResolveHost(&ip, host, port);
    server = SDLNet_TCP_Open(&ip);
    return server;
}

void Network::printSocketInfo(TCPsocket client) {
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

void Network::handleClient(TCPsocket client, const char* serverMsg) {
    char buffer[1024];
    const int delayInSeconds = 5; // Set the message delay to 5 seconds

    while (true) {
        int received = SDLNet_TCP_Recv(client, buffer, sizeof(buffer) - 1);
        if (received > 0) {
            buffer[received] = '\0';  // Null-terminate the received data
            auto now = std::chrono::steady_clock::now();

            // Check if the client is allowed to send another message
            auto it = clientMessageTimestamps.find(client);
            if (it != clientMessageTimestamps.end()) {
                // Check how much time has passed since the last message
                auto timeSinceLastMessage = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
                if (timeSinceLastMessage < delayInSeconds) {
                    const char* rateLimitMessage = "You're sending messages too quickly. Please wait before sending another message.";
                    SDLNet_TCP_Send(client, rateLimitMessage, strlen(rateLimitMessage) + 1);
                    continue; // Skip further processing
                }
            }

            // Update the client's last message timestamp
            clientMessageTimestamps[client] = now;

            printf("Received: %s\n", buffer);
            printSocketInfo(client);

            // Relaying received message
            {
                std::lock_guard<std::mutex> lock(clientListMutex);
                for (TCPsocket otherClient : clients) {
                    if (client != otherClient) {
                        SDLNet_TCP_Send(otherClient, buffer, strlen(buffer) + 1);
                    }
                }
            }

            // Check for the exit command
            if (strcmp(buffer, "quit") == 0) {
                printf("Client requested to exit. Closing connection.\n");
                break;
            }

            // Send server message
            if (serverMsg && serverMsg[0] != '\0') {
                SDLNet_TCP_Send(client, serverMsg, strlen(serverMsg) + 1);
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

    // Handle client removal from your data structures
    std::lock_guard<std::mutex> lock(clientListMutex);
    clients.remove(client);
    clientMessageTimestamps.erase(client);
}

void Network::networkLoop(bool& serverLoop, const char* serverInput) {
    while (serverLoop) {
        TCPsocket client = SDLNet_TCP_Accept(server);
        if (client) {
            std::lock_guard<std::mutex> lock(clientListMutex);
            if (clients.size() < MAX_CLIENTS) {
                printf("Client Connected\n");
                clients.push_back(client);
                std::thread clientThread(&Network::handleClient, this, client, serverInput);
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

    // Join all client threads before exiting
    for (std::thread& thread : clientThreads) {
        thread.join();
    }
}

void Network::serverInputFunction(bool& serverLoop, char* serverInput) {
    while (serverLoop) {
        printf("Server's Main Thread\nType 'quit' to close the server and exit the program\n");
        memset(serverInput, 0, sizeof(serverInput));
        if (fgets(serverInput, sizeof(serverInput), stdin) == NULL) {
            break; // Exit the loop on EOF or error
        }
        size_t len = strlen(serverInput);
        if (len > 0 && serverInput[len - 1] == '\n') {
            serverInput[len - 1] = '\0';
        }
        if (strcmp(serverInput, "exit") == 0) {
            printf("Server requested to exit. Closing connection.\n");
            serverLoop = false;
            break; // Exit the loop if the user enters 'quit'
        }
    }
}

int main(int argc, char* argv[]) {
    Network network;
    TCPsocket server = network.init(NULL, 8080);
    bool serverLoop = true;
    char serverInput[1024];
    std::thread serverInputThread(&Network::serverInputFunction, &network, std::ref(serverLoop), serverInput);
    network.networkLoop(serverLoop, serverInput);
    serverInputThread.join();
    return 0;
}
