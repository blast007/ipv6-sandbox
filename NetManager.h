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

#ifndef __NETMANAGER_H__
#define __NETMANAGER_H__

/* common header */
#include "common.h"

class NetManager {
    public:
        NetManager(const char* address, const char* port);
        ~NetManager();
        bool init();

        int getUdpSocket();

        // General function to support the select statement
        void setFd(fd_set *read_set, fd_set *write_set, int &maxFile);
        //bool isTcpFdSet(fd_set *read_set)
        bool isUdpFdSet(fd_set *read_set);

        /**
            udpReceive will try to get the next udp message received

            return the playerIndex if found, -1 when no player had an open udp
            connection or -2 when a Ping Code Request has been detected.

            buffer is the received message

            uaddr is the identifier of the remote address

            udpLinkRequest report if the received message is a valid udpLinkRequest
        */
        static int    udpReceive(char *buffer, struct sockaddr_in *uaddr,
                                 bool &udpLinkRequest);

        // Request if there is any buffered udp messages waiting to be sent
        bool   anyUDPPending()
        {
            return pendingUDP;
        };

    private:
        const char* address;
        const char* port;
        int tcpListenSocket;
        int udpSocket;

        bool pendingUDP;

#if defined(_WIN32)
        const BOOL optOn = TRUE;
#else
        const int optOn = 1;
#endif
};

#endif

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
