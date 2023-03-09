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

#include <poll.h>
#include <vector>
#include <functional>

class NetManager {
    public:
        NetManager(const char* port);
        ~NetManager();

        // Bind to a new IP
        bool bind(const char* address);

        // Process network events
        bool process();

        static void * get_in_addr(struct sockaddr *sa);

        void addAcceptCallback(std::function<void(struct sockaddr *, int)> callback);
        void setMessageReceivedCallback(std::function<void(const char *)> callback);
    private:
        // Port to bind all interfaces on
        const char* port;

        // Socket descriptor information
        int fd_count;
        int fd_size;
        int numInterfaces;
        struct pollfd *fds;

        // Callbacks
        std::vector<std::function<void(struct sockaddr *, int)>> acceptCallbacks;
        std::function<void(const char *)> messageReceivedCallback;

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
