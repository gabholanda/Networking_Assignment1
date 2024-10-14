#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <thread>
#undef main
#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[32m"
#define COLOR_PINK "\033[35m"
#define COLOR_BLUE "\033[34m"
TCPsocket init(const char* host, Uint16 port)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDLNet_Init();

    IPaddress ip;
    SDLNet_ResolveHost(&ip, host, port);

    TCPsocket client = SDLNet_TCP_Open(&ip);

    return client;
}

void cleanup(TCPsocket& server)
{
    SDLNet_TCP_Close(server);
    SDLNet_Quit();
    SDL_Quit();
}


void networkLoop(TCPsocket& client, bool& clientLoop, const char* username)
{
    //char buffer[1024];
    char buffer[1024];
    //const char* username = "[Nahid]";

    std::string input;

    //bool clientLoop = true;

    std::cout << COLOR_BLUE << "\nEnter a message to send (or type 'quit' to exit): " << COLOR_RESET;
    while (clientLoop)
    {
        fflush(stdout);

        //SEND Data
        //####################################################################
        //####################################################################
        //
            /*
            // Read user input
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;  // Exit the loop on EOF or error
            }

            // Remove newline character at the end of the input
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            */
        std::cout << COLOR_BLUE << "\nEnter input (up to 1024 characters): " << COLOR_RESET;
        std::getline(std::cin, input);

        // If the input length exceeds 1024 characters, truncate it
        if (input.length() > 1024) {
            input = input.substr(0, 1024);  // Keep only the first 1024 characters
        }

        std::string new_message = std::string(COLOR_PINK) + username + COLOR_RESET + " : " + std::string(COLOR_BLUE) + input + COLOR_RESET;


        //std::cout << (new_message);



        //SDLNet_TCP_Send(client, buffer, sizeof(buffer));
        if (client)
            SDLNet_TCP_Send(client, new_message.c_str(), new_message.length() + 1);
        //SDLNet_TCP_Send(client, input.c_str(), input.length() + 1);

        if (strcmp(input.c_str(), "quit") == 0) {
            exit(1);
            break;  // Exit the loop if the user enters 'quit'
        }
        //END SEND Data
        //####################################################################
        //####################################################################

        //Recieve DATA
        //####################################################################
        //####################################################################
        /*
        int received = SDLNet_TCP_Recv(client, buffer, sizeof(buffer));
        if (received > 0) {

            buffer[received - 1] = '\0';  // Null-terminate the received data
            if(strcmp(buffer, "exit") == 0)
            {
                printf("SERVER WAnts to close\n");
                break;  // Exit the loop if the user enters 'quit'

            }
            else
                printf("SERVER: %s\n", buffer);
        }
        */
        //####################################################################
        //####################################################################
    }
}

void receive_output(TCPsocket client, bool& net_loop)
{
    //if(client)
    while (client)
    {
        char buffer[1024];
        int received = SDLNet_TCP_Recv(client, buffer, sizeof(buffer));
        if (received > 0) {

            buffer[received - 1] = '\0';  // Null-terminate the received data
            if (strcmp(buffer, "exit") == 0)
            {
                std::cout << COLOR_GREEN << "SERVER Wants to close" << COLOR_RESET << std::endl;
                net_loop = false;
                SDLNet_TCP_Close(client);
                exit(1);
                //break;  // Exit the loop if the user enters 'quit'

            }
            else
                std::cout << COLOR_GREEN << "\nSERVER: " << buffer << COLOR_RESET << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {

    //TCPsocket client = init("192.168.0.189", 8080);
    printf("Num Arguments %d\n", argc);

    TCPsocket client;
    bool net_loop = true;
    const char* username;

    if (argc >= 2)
    {
        printf("Argument Value %s\n", argv[1]);
        username = argv[2];
        client = init(argv[1], 8080);
    }
    else
    {
        printf("Default\n");
        printf("Argument Value %s\n", argv[0]);
        username = "[Student]";
        client = init("127.0.0.1", 8080);
    }

    std::thread outputThread(receive_output, std::ref(client), std::ref(net_loop));

    networkLoop(client, net_loop, username);
    cleanup(client);

    outputThread.join();

    return 0;
}
