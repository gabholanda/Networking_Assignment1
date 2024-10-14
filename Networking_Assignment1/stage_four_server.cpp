#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <iostream>
#include <vector>
#include <list>
#include <thread>
#include <mutex>
#include <map>
#include <chrono>
#include <string>
#undef main

// ANSI escape codes for colors
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

class Network {
private:
	std::mutex clientListMutex;
	std::list<TCPsocket> clients;
	std::vector<std::thread> clientThreads;
	const int MAX_CLIENTS = 4;
	bool serverLoop = true;
	TCPsocket server;
	std::map<TCPsocket, std::chrono::steady_clock::time_point> clientMessageTimestamps;

public:
	Network() : server(nullptr) {}

	~Network() {
		if (server) {
			SDLNet_TCP_Close(server);
		}
		SDLNet_Quit();
		SDL_Quit();
	}

	TCPsocket init(const char* host, Uint16 port) {
		SDL_Init(SDL_INIT_EVERYTHING);
		SDLNet_Init();
		IPaddress ip;
		SDLNet_ResolveHost(&ip, host, port);
		server = SDLNet_TCP_Open(&ip);
		return server;
	}

	void printSocketInfo(TCPsocket client) {
		IPaddress* clientIP = SDLNet_TCP_GetPeerAddress(client);
		Uint32 host = SDLNet_Read32(&clientIP->host);
		Uint16 port = SDLNet_Read16(&clientIP->port);
		std::string info = "Client Connected: IP Address: " +
			std::to_string((host >> 24) & 0xFF) + "." +
			std::to_string((host >> 16) & 0xFF) + "." +
			std::to_string((host >> 8) & 0xFF) + "." +
			std::to_string(host & 0xFF) + ", Port: " +
			std::to_string(port);
		std::cout << COLOR_RED << info << COLOR_RESET << std::endl;  // Use macro for red text
	}

	void handleClient(TCPsocket client, const char* serverMsg) {
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

				std::string receivedMsg = "Received: " + std::string(buffer);
				std::cout << COLOR_RED << receivedMsg << COLOR_RESET << std::endl;  // Use macro for red text
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
					std::cout << COLOR_RED << "Client requested to exit. Closing connection." << COLOR_RESET << std::endl;  // Use macro for red text
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
				std::cout << COLOR_RED << "Client Disconnected" << COLOR_RESET << std::endl;  // Use macro for red text
				break;
			}
		}

		SDLNet_TCP_Close(client);

		// Handle client removal from your data structures
		std::lock_guard<std::mutex> lock(clientListMutex);
		clients.remove(client);
		clientMessageTimestamps.erase(client);
	}

	void networkLoop(const char* serverInput) {
		while (serverLoop) {
			if (!server)
			{
				serverLoop = false;
				continue;
			}
			TCPsocket client = SDLNet_TCP_Accept(server);
			if (client) {
				std::lock_guard<std::mutex> lock(clientListMutex);
				if (clients.size() < MAX_CLIENTS) {
					std::cout << COLOR_RED << "Client Connected" << COLOR_RESET << std::endl;  // Use macro for red text
					clients.push_back(client);
					std::thread clientThread(&Network::handleClient, this, client, serverInput);
					clientThreads.push_back(std::move(clientThread));
				}
				else {
					const char* fullMessage = "Server is full. Please wait for a free slot.";
					SDLNet_TCP_Send(client, fullMessage, strlen(fullMessage) + 1);
					SDLNet_TCP_Close(client);
					std::cout << COLOR_RED << "Server full. Rejected connection." << COLOR_RESET << std::endl;  // Use macro for red text
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

	void serverInputFunction(char* serverInput) {
		while (serverLoop) {
			std::cout << COLOR_RED "Server's Main Thread\nType 'quit' to close the server and exit the program\n" COLOR_RESET;
			memset(serverInput, 0, sizeof(serverInput));
			if (fgets(serverInput, sizeof(serverInput), stdin) == NULL) {
				break; // Exit the loop on EOF or error
			}
			size_t len = strlen(serverInput);
			if (len > 0 && serverInput[len - 1] == '\n') {
				serverInput[len - 1] = '\0';
			}
			if (strcmp(serverInput, "quit") == 0) { // Changed from "exit" to "quit"
				std::cout << COLOR_RED << "Server requested to exit. Closing connection." << COLOR_RESET << std::endl;  // Use macro for red text

				const char buffer[1024] = "Server is closing connection. You will be disconnected";
				for (TCPsocket client : clients) {
					SDLNet_TCP_Send(client, buffer, strlen(buffer) + 1);
				}
				serverLoop = false;
				break; // Exit the loop if the user enters 'quit'
			}
		}
	}
};

int main(int argc, char* argv[]) {
	Network network;
	TCPsocket server = network.init(NULL, 8080);
	char serverInput[1024];
	std::thread serverInputThread(&Network::serverInputFunction, &network, serverInput);
	network.networkLoop(serverInput);
	serverInputThread.join();
	return 0;
}
