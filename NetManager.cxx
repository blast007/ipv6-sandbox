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

NetManager::NetManager(const char* address, const char* port) : address(address), port(port), tcpListenSocket(-1), udpSocket(-1),
    pendingUDP(false)
{

}

NetManager::~NetManager()
{
    if (tcpListenSocket > 0)
        close(tcpListenSocket);
    if (udpSocket > 0)
        close(udpSocket);
}

bool NetManager::init()
{
    std::cout << "Initializing sockets for " << address << " on port " << port << std::endl;

    //struct sockaddr_storage addr;
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


    tcpListenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (tcpListenSocket == -1)
    {
        nerror("couldn't make connect socket");
        return false;
    }

#ifdef SO_REUSEADDR
    // Enable socket reuse
    opt = optOn;
    if (setsockopt(tcpListenSocket, SOL_SOCKET, SO_REUSEADDR, (SSOType)&opt, sizeof(opt)) < 0)
    {
        nerror("serverStart: setsockopt SO_REUSEADDR");
        close(tcpListenSocket);
        return false;
    }
#endif
    // On IPv6 interfaces, set it to use IPv6 only
    opt = optOn;
    if (res->ai_family == AF_INET6 && setsockopt(tcpListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof opt) == -1)
    {
        nerror("serverStart: setsockopt IPV6_ONLY");
        close(tcpListenSocket);
        return false;
    }

    if (bind(tcpListenSocket, res->ai_addr, res->ai_addrlen) == -1)
    {
        nerror("couldn't bind TCP connect socket");
        close(tcpListenSocket);
        return false;
    }

    if (listen(tcpListenSocket, 5) == -1)
    {
        nerror("couldn't make connect socket queue");
        close(tcpListenSocket);
        return false;
    }

    // TODO: Check if we should run getaddrinfo again with the other protocol

    // we open a udp socket on the same port
    if ((udpSocket = (int) socket(res->ai_family, SOCK_DGRAM, 0)) < 0)
    {
        nerror("couldn't make UDP connect socket");
        return false;
    }

    // increase send/rcv buffer size
    if (setsockopt(udpSocket, SOL_SOCKET, SO_SNDBUF, (SSOType) &udpBufSize, sizeof(int)) == -1)
    {
        nerror("couldn't increase UDP send buffer size");
        close(udpSocket);
        return false;
    }
    if (setsockopt(udpSocket, SOL_SOCKET, SO_RCVBUF, (SSOType) &udpBufSize, sizeof(int)) == -1)
    {
        nerror("couldn't increase UDP receive buffer size");
        close(udpSocket);
        return false;
    }

    // On IPv6 interfaces, set it to use IPv6 only
    opt = optOn;
    if (res->ai_family == AF_INET6 && setsockopt(udpSocket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof opt) == -1)
    {
        nerror("serverStart: setsockopt IPV6_ONLY");
        close(tcpListenSocket);
        return false;
    }

    if (bind(udpSocket, res->ai_addr, res->ai_addrlen) == -1)
    {
        nerror("couldn't bind UDP listen port");
        close(udpSocket);
        return false;
    }

    // don't buffer info, send it immediately
    BzfNetwork::setNonBlocking(udpSocket);

    return true;
}

int NetManager::getUdpSocket()
{
    return udpSocket;
}

void NetManager::setFd(fd_set *read_set, fd_set *write_set, int &maxFile)
{
    if (tcpListenSocket >= 0)
    {
        FD_SET((unsigned int)tcpListenSocket, read_set);
        if (tcpListenSocket > maxFile)
            maxFile = tcpListenSocket;
    }

    if (udpSocket >= 0)
    {
        FD_SET((unsigned int)udpSocket, read_set);
        if (udpSocket > maxFile)
            maxFile = udpSocket;
    }
}


/* Local Variables: ***
 * mode: C++ ***
 * tab-width: 8 ***
 * c-basic-offset: 2 ***
 * indent-tabs-mode: t ***
 * End: ***
 * ex: shiftwidth=2 tabstop=8
 */
