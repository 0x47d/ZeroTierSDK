/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include "SDK.h"
#include "SDK_XcodeWrapper.hpp"
#include "SDK_Signatures.h"
#include <sys/socket.h>

#define INTERCEPT_ENABLED   111
#define INTERCEPT_DISABLED  222

#include "SDK_ServiceSetup.hpp"

// Starts a ZeroTier service at the specified path
// This will only support SOCKS5 Proxy
extern "C" void start_service(const char * path) {
    init_service(INTERCEPT_DISABLED, path);
}

// Starts a ZeroTier service at the specified path and initializes the RPC mechanism
// This will allow direct API calls
extern "C" void start_service_and_rpc(const char * path, const char * nwid) {
    init_service_and_rpc(INTERCEPT_DISABLED, path, nwid);
}

// Joins a ZeroTier virtual network
extern "C" void zt_join_network(const char * nwid){
    join_network(nwid);
}

// Leaves a ZeroTier virtual network
extern "C" void zt_leave_network(const char * nwid){
    leave_network(nwid);
}

// Explicit ZT API wrappers
#if !defined(__IOS__)
    // This isn't available for iOS since function interposition isn't as reliable
    extern "C" void zts_init_rpc(const char *path, const char *nwid) {
        zt_init_rpc(path, nwid);
    }
#endif

extern "C" int zts_socket(SOCKET_SIG) {
    return zt_socket(socket_family, socket_type, protocol);
}
extern "C" int zts_connect(CONNECT_SIG) {
    return zt_connect(__fd, __addr, __len);
}
extern "C" int zts_bind(BIND_SIG){
    return zt_bind(sockfd, addr, addrlen);
}
extern "C" int zts_accept(ACCEPT_SIG) {
    return zt_accept(sockfd, addr, addrlen);
}
extern "C" int zts_listen(LISTEN_SIG) {
    return zt_listen(sockfd, backlog);
}
extern "C" int zts_setsockopt(SETSOCKOPT_SIG) {
    return zt_setsockopt(socket, level, option_name, option_value, option_len);
}
extern "C" int zts_getsockopt(GETSOCKOPT_SIG) {
    return zt_getsockopt(sockfd, level, optname, optval, optlen);
}
extern "C" int zts_close(CLOSE_SIG) {
    return zt_close(fd);
}
extern "C" int zts_getsockname(GETSOCKNAME_SIG) {
    return zt_getsockname(sockfd, addr, addrlen);
}
