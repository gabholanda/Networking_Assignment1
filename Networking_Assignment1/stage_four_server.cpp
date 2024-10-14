#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>
#include <list>
#include <mutex>
#include <map>
#include <chrono>

#undef main
std::map<TCPsocket, std::chrono::steady_clock::time_point> clientMessageTimestamps;

std::mutex clientListMutex;
std::list<TCPsocket> clients;
const int MAX_CLIENTS = 4;

TCPsocket init(const char* host, Uint16 port)
{
	SDL_Init(SDL_INIT_EVERYTHING);
	SDLNet_Init();

	IPaddress ip;
	SDLNet_ResolveHost(&ip, host, port);

	TCPsocket server = SDLNet_TCP_Open(&ip);

	return server;
}

void cleanup(TCPsocket& server)
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
	const int delayInSeconds = 5; // Set the message delay to 5 seconds
	//const char* response = "Server received your message.";

	while (true) {
		if (client)
		{
			int received = SDLNet_TCP_Recv(client, buffer, sizeof(buffer));
			if (received > 0) {
				buffer[received - 1] = '\0';  // Null-terminate the received data

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
				for (TCPsocket otherClient : clients)
				{
					if (client != otherClient)
					{
						SDLNet_TCP_Send(otherClient, buffer, sizeof(buffer));
					}
				}

				// Check for the exit command
				if (strcmp(buffer, "quit") == 0) {
					printf("Client requested to exit. Closing connection.\n");
					break;
				}

				// Send response to the client
				//SDLNet_TCP_Send(client, response, strlen(response) + 1);

				//Server message is not empty
				if (serverMsg[0] != '\0')
					SDLNet_TCP_Send(client, serverMsg, sizeof(serverMsg));
				else
				{
					//Send default message if server did not send a message
					const char* response = "Server received your message.";
					SDLNet_TCP_Send(client, response, strlen(response) + 1);
					//SDLNet_TCP_Send(client, response, sizeof(response));
				}
			}
			else {
				// Client disconnected or error occurred
				printf("Client Disconnected\n");
				break;
			}
		}
	}

	SDLNet_TCP_Close(client);

	// Handle client removal from your data structures if necessary
	// It will call unlock automatically at the destructor of lock_guard
	std::lock_guard<std::mutex> lock(clientListMutex);
	clients.remove(client);
	clientMessageTimestamps.erase(client);
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


//void serverInputFunction(bool serverLoop, char* serverInput)
void serverInputFunction(bool& serverLoop, char* serverInput)
{
	while (serverLoop)
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
