/* bzflag
 * Copyright (c) 1993-2023 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* interface header */
#include "NetManager.h"

#include <string.h>
#include "network.h"
#include <iostream>

const int udpBufSize = 128000;

NetManager::NetManager(const char* port) : port(port), fd_count(0), fd_size(14), numInterfaces(0), messageReceivedCallback(nullptr)
{
    // Allocate memory for the initial size of the pollfds
    fds = (struct pollfd *)malloc(sizeof *fds * fd_size);
    memset(fds, 0, sizeof *fds * fd_size);
}

NetManager::~NetManager()
{
    int i;

    // TODO: Do we need to close the client sockets too?
    //for(i = numInterfaces * 2; i < fd_count; ++i)
        //close(fds[i].fd);

    // Close the TCP and UDP listening sockets
    for (i = 0; i < numInterfaces * 2; ++i)
        close(fds[i].fd);

    free(fds);
    fds = nullptr;
}

bool NetManager::bind(const char* address)
{
    //struct sockaddr_storage addr;
    int tcpSocket, udpSocket;
    struct addrinfo hints, *res;
    int r;

#if defined(_WIN32)
    BOOL opt;
#else
    int opt;
#endif

    // Set the lookup hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

    if ((r = getaddrinfo(address, port, &hints, &res)) != 0)
    {
        nerror("couldn't look up bind interface information via getaddrinfo");
        return false;
    }


    tcpSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (tcpSocket == -1)
    {
        nerror("couldn't make connect socket");
        freeaddrinfo(res);
        return false;
    }

#ifdef SO_REUSEADDR
    // Enable socket reuse
    opt = optOn;
    if (setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, (SSOType)&opt, sizeof(opt)) < 0)
    {
        nerror("serverStart: setsockopt SO_REUSEADDR");
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }
#endif
    // On IPv6 interfaces, set it to use IPv6 only
    opt = optOn;
    if (res->ai_family == AF_INET6 && setsockopt(tcpSocket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof opt) == -1)
    {
        nerror("serverStart: setsockopt IPV6_ONLY");
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }

    if (::bind(tcpSocket, res->ai_addr, res->ai_addrlen) == -1)
    {
        nerror("couldn't bind TCP connect socket");
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }

    if (listen(tcpSocket, 5) == -1)
    {
        nerror("couldn't make connect socket queue");
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }

    // TODO: Check if we should run getaddrinfo again with the other protocol

    // we open a udp socket on the same port
    if ((udpSocket = (int) socket(res->ai_family, SOCK_DGRAM, 0)) < 0)
    {
        nerror("couldn't make UDP connect socket");
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }

    // increase send/rcv buffer size
    if (setsockopt(udpSocket, SOL_SOCKET, SO_SNDBUF, (SSOType) &udpBufSize, sizeof(int)) == -1)
    {
        nerror("couldn't increase UDP send buffer size");
        close(udpSocket);
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }
    if (setsockopt(udpSocket, SOL_SOCKET, SO_RCVBUF, (SSOType) &udpBufSize, sizeof(int)) == -1)
    {
        nerror("couldn't increase UDP receive buffer size");
        close(udpSocket);
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }

    // On IPv6 interfaces, set it to use IPv6 only
    opt = optOn;
    if (res->ai_family == AF_INET6 && setsockopt(udpSocket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof opt) == -1)
    {
        nerror("serverStart: setsockopt IPV6_ONLY");
        close(udpSocket);
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }

    if (::bind(udpSocket, res->ai_addr, res->ai_addrlen) == -1)
    {
        nerror("couldn't bind UDP listen port");
        close(udpSocket);
        close(tcpSocket);
        freeaddrinfo(res);
        return false;
    }

    freeaddrinfo(res);

    // don't buffer info, send it immediately
    BzfNetwork::setNonBlocking(udpSocket);

    // Add the two new sockets to our pollfds
    fds[fd_count].fd = tcpSocket;
    fds[fd_count++].events = POLLIN;
    fds[fd_count].fd = udpSocket;
    fds[fd_count++].events = POLLIN;
    numInterfaces += 1;

    return true;
}

bool NetManager::process()
{
    int pollCount = poll(fds, fd_count, 50);

    // Uh oh, something went wong
    if (pollCount == -1)
    {
        perror("poll");
        return false;
    }

    // Nothing to process
    if (pollCount == 0)
        return true;

    for (int i = 0; i < fd_count; i++)
    {
        // Check if a socket is ready to read
        if (fds[i].revents & POLLIN)
        {
            // If it's out listening socket, accept the client
            // TODO: See if we need to match the fd with the TCP listener
            if (i < 2 * numInterfaces - 1 && i % 2 == 0)
            {
                struct sockaddr_storage remoteIP;
                socklen_t remoteIPLen = sizeof remoteIP;
                int cs = accept(fds[i].fd, (struct sockaddr *)&remoteIP, &remoteIPLen);

                if (cs == -1)
                {
                    perror("accept");
                }
                else
                {
                    // If we are out of room, expand it a bit
                    if (fd_count == fd_size)
                    {
                        const int fd_size_increase = 10;
                        struct pollfd* new_fds;
                        new_fds = (struct pollfd*)realloc(fds, sizeof(*fds) * (fd_size + fd_size_increase));
                        if (new_fds == nullptr)
                            return false;
                        else
                        {
                            // TODO: Need to test if this actually initializes the extra allocated memory
                            memset(fds + (sizeof *fds * fd_size), 0, sizeof *fds * fd_size_increase);
                            fd_size += fd_size_increase;
                            fds = new_fds;
                        }
                    }

                    // Set socket to non-blocking
                    //fcntl(cs, F_SETFL, O_NONBLOCK);

                    fds[fd_count].fd = cs;
                    fds[fd_count++].events = POLLIN;


                    for (auto acceptCallback : acceptCallbacks)
                        acceptCallback((struct sockaddr *)&remoteIP, cs);
                }
            }

            // If not a listener, it's a client
            else
            {
                char buf[1024] = {0};

                int nbytes = recv(fds[i].fd, buf, sizeof buf, 0);

                if (nbytes <= 0)
                {
                    if (nbytes == 0)
                    {
                        std::cout << "socket " << fds[i].fd << " has disconnected" << std::endl;
                    }
                    else
                    {
                        perror("recv");
                    }

                    // Close the socket
                    close(fds[i].fd);

                    // Remove an index from the set by copying the ones from the end over this one
                    fds[i] = fds[fd_count--];
                }
                else
                {
                    if (messageReceivedCallback != nullptr)
                        messageReceivedCallback(buf);
                }
            }
        }
    }

    return true;
}

void NetManager::addAcceptCallback(std::function<void(struct sockaddr *, int)> callback)
{
    acceptCallbacks.push_back(callback);
}

void NetManager::setMessageReceivedCallback(std::function<void(const char *)> callback)
{
    messageReceivedCallback = callback;
}


/* Local Variables: ***
 * mode: C++ ***
 * tab-width: 8 ***
 * c-basic-offset: 2 ***
 * indent-tabs-mode: t ***
 * End: ***
 * ex: shiftwidth=2 tabstop=8
 */
