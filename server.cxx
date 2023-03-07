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

struct pollstuff {
        int fd_count = 0;
        int fd_size = 5;
        struct pollfd *pollfds;
};

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void terminate(int signum)
{
    if (signum == 2)
        std::cout << std::endl;
    std::cout << "Received signal " << signum << ", shutting down" << std::endl;
    running = false;
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

    // Container for poll file descriptors
    std::vector<pollstuff> pollstuffs;

    // Create a NetManager for each interface
    std::vector<NetManager*> netManagers;
    for (auto &interface : interfaces)
    {
        NetManager *manager = new NetManager(interface.c_str(), port);
        if (manager->init())
        {
            netManagers.push_back(manager);
            struct pollstuff p;
            p.pollfds = (struct pollfd*)malloc(sizeof *p.pollfds * p.fd_size);
            p.pollfds[0].fd = manager->getTcpListenSocket();
            p.pollfds[0].events = POLLIN;
            p.fd_count = 1;
            pollstuffs.push_back(p);
        }
    }

    // Game loop
    while (running)
    {
        //std::cout << "Server loop restart" << std::endl;
        for (auto &p : pollstuffs)
        {
            int poll_count = poll(p.pollfds, p.fd_count, 50);

            // Uh oh, something went wong
            if (poll_count == -1)
            {
                perror("poll");
                return -1;
            }

            // Nothing to process, so just continue to the next pollfds if any
            if (poll_count == 0)
                continue;

            for (int i = 0; i < p.fd_count; i++)
            {
                // Check if a socket is ready to read
                if (p.pollfds[i].revents & POLLIN)
                {
                    // If it's out listening socket, accept the client
                    // TODO: See if we need to match the fd with the TCP listener
                    if (i == 0)
                    {
                        struct sockaddr_storage remoteIP;
                        socklen_t remoteIPLen;
                        int cs = accept(p.pollfds[i].fd, (struct sockaddr *)&remoteIP, &remoteIPLen);

                        if (cs == -1)
                        {
                            perror("accept");
                        }
                        else
                        {
                            // If we are out of room, expand it a bit
                            if (p.fd_count == p.fd_size)
                            {
                                p.fd_size += 10;
                                p.pollfds = (struct pollfd*)realloc(p.pollfds, sizeof(*p.pollfds) * (p.fd_size));
                            }

                            p.pollfds[p.fd_count].fd = cs;
                            p.pollfds[p.fd_count].events = POLLIN;
                            p.fd_count++;

                            char str[INET6_ADDRSTRLEN];
                            inet_ntop(remoteIP.ss_family, get_in_addr((struct sockaddr*)&remoteIP), str, INET_ADDRSTRLEN);
                            std::cout << "New TCP connection from " << str << " on socket " << cs << std::endl;
                        }
                    }

                    // If not a listener, it's a client
                    else
                    {
                        int sender_fd = p.pollfds[i].fd;

                        char buf[1024] = {0};

                        int nbytes = recv(sender_fd, buf, sizeof buf, 0);

                        if (nbytes <= 0)
                        {
                            if (nbytes == 0)
                            {
                                std::cout << "socket " << sender_fd << " has disconnected" << std::endl;
                            }
                            else
                            {
                                perror("recv");
                            }

                            // Close the socket
                            close(sender_fd);

                            // Remove an index from the set by copying the ones from the end over this one
                            p.pollfds[i] = p.pollfds[p.fd_count-1];
                            p.fd_count--;
                        }
                        else
                        {
                            std::cout << "Received data: " << buf << std::endl;
                        }
                    }
                }
            }

        }
        // Sleep a bit
        usleep(100000);
    }

    // Delete all the pollstuff
    for (auto &p : pollstuffs)
    {
        free(p.pollfds);
        p.pollfds = nullptr;
    }

    // Shut down all the NetManagers
    for (auto &netManager : netManagers)
    {
        delete netManager;
        netManager = nullptr;
    }

    // Thanks for all the fish!
    std::cout << "Goodbye!" << std::endl;

    return 0;
}
