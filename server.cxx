/**
 * Copyright (c) 2023 Scott Wichser
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include "NetManager.h"

#include <vector>
#include <string>
#include <iostream>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

bool running = true;



void terminate(int signum)
{
    if (signum == 2)
        std::cout << std::endl;
    std::cout << "Received signal " << signum << ", shutting down" << std::endl;
    running = false;
}

// Get sockaddr, IPv4 or IPv6:
void * get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void acceptConnection(struct sockaddr* remoteIP, int socket)
{
    char ipstr[INET6_ADDRSTRLEN];
    inet_ntop(remoteIP->sa_family, get_in_addr(remoteIP), ipstr, INET_ADDRSTRLEN);

    if (remoteIP->sa_family == AF_INET)
        std::cout << "Accepted IPv4 TCP connection from " << ipstr << " on socket " << socket << std::endl;
    else
        std::cout << "Accepted IPv6 TCP connection from [" << ipstr << "] on socket " << socket << std::endl;
}

void handleMessageReceived(const char *data)
{
    std::cout << "Received data: " << data << std::endl;
}

int main()
{
    // Set up signal handling
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = terminate;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);

    // Set up requested interfaces as string values
    std::vector<std::string> interfaces;
    interfaces.push_back("0.0.0.0");
    interfaces.push_back("::");

    // The port to use
    const char *port = "5154";

    // Create a NetManager and bind each interface
    NetManager *netManager = new NetManager(port);
    for (auto &interface : interfaces)
    {
        if (netManager->bind(interface.c_str()))
        {
            std::cout << "Listening on " << interface << " port " << port << std::endl;
        }
        else
        {
            std::cerr << "Failed to bind to " << interface << " port " << port << ": ";
            perror("");
        }
    }
    netManager->addAcceptCallback(acceptConnection);
    netManager->setMessageReceivedCallback(handleMessageReceived);

    // Game loop
    while (running)
    {
        netManager->process();

        // Sleep a bit
        usleep(100000);
    }

    // Shut down NetManager
    delete netManager;
    netManager = nullptr;

    // Thanks for all the fish!
    std::cout << "Goodbye!" << std::endl;

    return 0;
}
