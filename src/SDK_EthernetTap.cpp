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

#include <algorithm>
#include <utility>
#include <dlfcn.h>
#include <sys/poll.h>
#include <stdint.h>
#include <utility>
#include <string>
#include <sys/resource.h>
#include <sys/syscall.h>

#include "SDK_EthernetTap.hpp"
#include "SDK_Utils.hpp"
#include "SDK.h"
#include "SDK_defs.h"
#include "SDK_Debug.h"

#if defined(SDK_LWIP) 
	#include "SDK_lwIP.hpp"
#elif defined(SDK_PICOTCP)
 	#include "SDK_picoTCP.hpp"
	#include "pico_stack.h"
	#include "pico_ipv4.h"
	#include "pico_icmp4.h"
	#include "pico_dev_tap.h"
	#include "pico_protocol.h"
	#include "pico_socket.h"
#elif defined(SDK_JIP)
	#include "SDK_jip.hpp"
#endif

#include "Utils.hpp"
#include "OSUtils.hpp"
#include "Constants.hpp"
#include "Phy.hpp"

// LWIP
#include "lwip/priv/tcp_priv.h"
#include "lwip/nd6.h"
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"

#include "lwip/tcp.h"
#include "lwip/init.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"


//#if !defined(__IOS__) && !defined(__ANDROID__) && !defined(__UNITY_3D__) && !defined(__XCODE__)
//    const ip_addr_t ip_addr_any = { IPADDR_ANY };
//#endif

namespace ZeroTier {

/*------------------------------------------------------------------------------
------------------------------- picoTCP callbacks ------------------------------
---------- This section represents the "driver" for the picoTCP stack ----------
------------------------------------------------------------------------------*/

	// Reference to the tap interface
	// This is needed due to the fact that there's a lot going on in the tap interface
	// that needs to be updated on each of the network stack's callbacks and not every
	// network stack provides a mechanism for storing a reference to the tap.
	//
	// In future releases this will be replaced with a new structure of static pointers that 
	// will make it easier to maintain multiple active tap interfaces
	static NetconEthernetTap *picotap;

    static int pico_eth_send(struct pico_device *dev, void *buf, int len);
    static int pico_eth_poll(struct pico_device *dev, int loop_score);

    // Initialize network stack's interfaces and assign addresses
	void pico_init_interface(NetconEthernetTap *tap, const InetAddress &ip)
	{
		picoTCP_stack *stack = tap->picostack;		
		DEBUG_INFO();
		if (std::find(picotap->_ips.begin(),picotap->_ips.end(),ip) == picotap->_ips.end()) {
			picotap->_ips.push_back(ip);
			std::sort(picotap->_ips.begin(),picotap->_ips.end());
		#if defined(SDK_IPV4)
			if(ip.isV4())
			{
				int id;
			    struct pico_ip4 ipaddr, netmask;
			    ipaddr.addr = *((u32_t *)ip.rawIpData());
			    netmask.addr = *((u32_t *)ip.netmask().rawIpData());
			    stack->__pico_ipv4_link_add(&(picotap->picodev), ipaddr, netmask);
			    picotap->picodev.send = pico_eth_send; // tx
			    picotap->picodev.poll = pico_eth_poll; // rx
			    uint8_t mac[PICO_SIZE_ETH];
			    picotap->_mac.copyTo(mac, PICO_SIZE_ETH);
			    DEBUG_ATTN("mac = %s", picotap->_mac.toString().c_str());
			    if( 0 != stack->__pico_device_init(&(picotap->picodev), "p0", mac)) {
			        DEBUG_ERROR("device init failed");
			        return;
			    }
			    // DEBUG_INFO("device initialized as ipv4_addr = %s", ipv4_str);
			   	// picostack->__pico_icmp4_ping("10.8.8.1", 20, 1000, 10000, 64, cb_ping);
			}
		#elif defined(SDK_IPV6)
			if(ip.isV6())
			{
				struct pico_ip6 ipaddr, netmask;
				char ipv6_str[INET6_ADDRSTRLEN], nm_str[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, ip.rawIpData(), ipv6_str, INET6_ADDRSTRLEN);
				inet_ntop(AF_INET6, ip.netmask().rawIpData(), nm_str, INET6_ADDRSTRLEN);
		    	stack->__pico_string_to_ipv6(ipv6_str, ipaddr.addr);
		    	stack->__pico_string_to_ipv6(nm_str, netmask.addr);
			    stack->__pico_ipv6_link_add(&(picotap->picodev), ipaddr, netmask);
			    picotap->picodev.send = pico_eth_send; // tx
			    picotap->picodev.poll = pico_eth_poll; // rx
			    uint8_t mac[PICO_SIZE_ETH];
			    picotap->_mac.copyTo(mac, PICO_SIZE_ETH);
			    DEBUG_ATTN("mac = %s", picotap->_mac.toString().c_str());
			    if( 0 != stack->__pico_device_init(&(picotap->picodev), "p0", mac)) {
			        DEBUG_ERROR("device init failed");
			        return;
			    }
			    DEBUG_ATTN("device initialized as ipv6_addr = %s", ipv6_str);
			}
		#endif
		}
	}
	
	// I/O thread loop
	void pico_loop(NetconEthernetTap *tap)
	{
		DEBUG_INFO();
		while(tap->_run)
		{
			tap->_phy.poll((unsigned long)std::min(100,200));
			usleep(500);
	        tap->picostack->__pico_stack_tick();
		}
	}

	// RX packets from network onto internal buffer
	// Also notifies the tap service that data can be read, buffer will be emptied by pico_handleRead()
	static void pico_cb_tcp_read(NetconEthernetTap *tap, struct pico_socket *s)
	{
		// TODO: Verify 
		DEBUG_INFO();
		Connection *conn = tap->getConnection(s);
		if(conn) {
			int r;
			do {
				//int avail = DEFAULT_TCP_RX_BUF_SZ - conn->rxsz;
				//if(avail) {
		            r = tap->picostack->__pico_socket_read(s, conn->rxbuf + (conn->rxsz), ZT_MAX_MTU);
		            tap->_phy.setNotifyWritable(conn->sock, true);
		            DEBUG_INFO("read=%d", r);
		            if (r > 0) {
		                conn->rxsz += r;
		            }
	        	//}
            	if (r < 0) {
                	exit(5);
        		} 
            }
        	while(r > 0);
        	return;
		}
		DEBUG_ERROR("invalid connection");
	}

	// TX packets from internal buffer to network
	static void pico_cb_tcp_write(NetconEthernetTap *tap, struct pico_socket *s)
	{
		Connection *conn = tap->getConnection(s);
		if(!conn)
			DEBUG_ERROR("invalid connection");
		if(!conn->txsz)
			return;
		DEBUG_INFO("txsz=%d bytes ready to be written", conn->txsz);

		// Only called from a locked context, no need to lock anything
		if(conn->txsz > 0) {
			int r = conn->txsz < ZT_MAX_MTU ? conn->txsz : ZT_MAX_MTU;
			if((r = tap->picostack->__pico_socket_write(s, &conn->txbuf, r)) < 0) {
				DEBUG_ERROR("unable to write to pico_socket=%p", (void*)s);
				return;
			}
			int sz = (conn->txsz)-r;
            if(sz)
                memmove(&conn->txbuf, (conn->txbuf+r), sz);
            conn->txsz -= r;
            int max = conn->type == SOCK_STREAM ? DEFAULT_TCP_TX_BUF_SZ : DEFAULT_UDP_TX_BUF_SZ;
            DEBUG_TRANS("[TCP TX] --->    :: {TX: %.3f%%, RX: %.3f%%, sock=%p} :: %d bytes",
                (float)conn->txsz / (float)max, (float)conn->rxsz / max, (void*)&conn->sock, r);
            return;
		}
	}

	// Main callback for TCP connections
	static void pico_cb_tcp(uint16_t ev, struct pico_socket *s)
    {
        Mutex::Lock _l(picotap->_tcpconns_m);
        Connection *conn = picotap->getConnection(s);
        if(!conn) {
        	DEBUG_ERROR("invalid connection");
        }
        if (ev & PICO_SOCK_EV_RD) {
            pico_cb_tcp_read(picotap, s);
        }

        // Accept connection (analogous to lwip_nc_accept)
        if (ev & PICO_SOCK_EV_CONN) {
            DEBUG_INFO("connection established with server, sock=%p", (void*)(conn->picosock));
            uint32_t peer;
			uint16_t port;
            struct pico_socket *client = picotap->picostack->__pico_socket_accept(s, &peer, &port);
            if(!client) {
				DEBUG_ERROR("there was an error accepting the connection, sock=%p", (void*)(conn->picosock));
			}

			ZT_PHY_SOCKFD_TYPE fds[2];
			if(socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
				if(errno < 0) {
					picotap->sendReturnValue(conn, -1, errno);
					DEBUG_ERROR("unable to create socketpair");
					return;
				}
			}
			Connection *newTcpConn = new Connection();
			picotap->_Connections.push_back(newTcpConn);
			newTcpConn->type = SOCK_STREAM;
			newTcpConn->sock = picotap->_phy.wrapSocket(fds[0], newTcpConn);
			newTcpConn->picosock = client;
			int fd = picotap->_phy.getDescriptor(conn->sock);
			if(sock_fd_write(fd, fds[1]) < 0) {
				DEBUG_ERROR("error sending new fd to client application");
			}
        }
        if (ev & PICO_SOCK_EV_FIN) {
            DEBUG_INFO("socket closed. Exit normally.");
            //picotap->__pico_timer_add(2000, compare_results, NULL);
        }
        if (ev & PICO_SOCK_EV_ERR) {
            DEBUG_INFO("socket error received" /*, strerror(pico_err)*/);
            //exit(1);
        }
        if (ev & PICO_SOCK_EV_CLOSE) {
            DEBUG_INFO("socket received close from peer - Wrong case if not all client data sent!");
            picotap->picostack->__pico_socket_close(s);
            picotap->closeConnection(conn);
            return;
        }
        if (ev & PICO_SOCK_EV_WR) {
            pico_cb_tcp_write(picotap, s);
        }
    }

    // Called when an incoming ping is received
    static void pico_cb_ping(struct pico_icmp4_stats *s)
    {   
    	DEBUG_INFO();
        char host[30];
        picotap->picostack->__pico_ipv4_to_string(host, s->dst.addr);
        if (s->err == 0) {
            printf("%lu bytes from %s: icmp_req=%lu ttl=%lu time=%lu ms\n", s->size,
                    host, s->seq, s->ttl, (long unsigned int)s->time);
        } else {
            printf("PING %lu to %s: Error %d\n", s->seq, host, s->err);
        }
    }

    // Sends data to the tap device (in our case, the ZeroTier service)
    static int pico_eth_send(struct pico_device *dev, void *buf, int len)
    {
        DEBUG_INFO("len=%d", len);
        struct eth_hdr *ethhdr;
        ethhdr = (struct eth_hdr *)buf;

        MAC src_mac;
        MAC dest_mac;
        src_mac.setTo(ethhdr->src.addr, 6);
        dest_mac.setTo(ethhdr->dest.addr, 6);

        picotap->_handler(picotap->_arg,picotap->_nwid,src_mac,dest_mac,
            Utils::ntoh((uint16_t)ethhdr->type),0, ((char*)buf) + sizeof(struct eth_hdr),len - sizeof(struct eth_hdr));
        return len;
    }

    // Receives data from the tap device and encapsulates it into a ZeroTier ethernet frame and places it in a locked memory buffer
    void pico_rx(NetconEthernetTap *tap, const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
	{
		// DEBUG_INFO();
		// Since picoTCP only allows the reception of frames from within the polling function, we
		// must enqueue each frame into a memory structure shared by both threads. This structure will
		Mutex::Lock _l(tap->_pico_frame_rxbuf_m);
		if(len > ((1024 * 1024) - tap->pico_frame_rxbuf_tot)) {
			DEBUG_ERROR("dropping packet (len = %d) - not enough space left on RX frame buffer", len);
			return;
		}
		//if(len != memcpy(pico_frame_rxbuf, data, len)) {
		//	DEBUG_ERROR("dropping packet (len = %d) - unable to copy contents of frame to RX frame buffer", len);
		//	return;
		//}

		// assemble new eth header
		struct eth_hdr ethhdr;
		from.copyTo(ethhdr.src.addr, 6);
		to.copyTo(ethhdr.dest.addr, 6);
		ethhdr.type = Utils::hton((uint16_t)etherType);
		int newlen = len+sizeof(struct eth_hdr);

		memcpy(tap->pico_frame_rxbuf + tap->pico_frame_rxbuf_tot, &newlen, sizeof(newlen));                      // size of frame
		memcpy(tap->pico_frame_rxbuf + tap->pico_frame_rxbuf_tot + sizeof(newlen), &ethhdr, sizeof(ethhdr));     // new eth header
		memcpy(tap->pico_frame_rxbuf + tap->pico_frame_rxbuf_tot + sizeof(newlen) + sizeof(ethhdr), data, len);  // frame data
		tap->pico_frame_rxbuf_tot += len + sizeof(len) + sizeof(ethhdr);
		// DEBUG_INFO("RX frame buffer %3f full", (float)pico_frame_rxbuf_tot / (float)(1024 * 1024));
		DEBUG_INFO("len=%d", len);
	}

	// Is called periodically by the stack, this removes data from the locked memory buffer and feeds it into the stack.
	// A maximum of 'loop_score' frames can be processed in each call
    static int pico_eth_poll(struct pico_device *dev, int loop_score)
    {
    	// DEBUG_EXTRA();
        // OPTIMIZATION: The copy logic and/or buffer structure should be reworked for better performance after the BETA
        // NetconEthernetTap *tap = (NetconEthernetTap*)netif->state;
        Mutex::Lock _l(picotap->_pico_frame_rxbuf_m);

        uint8_t *buf = NULL;
        uint32_t len = 0;
        struct eth_hdr ethhdr;
        unsigned char frame[ZT_MAX_MTU];

        while (picotap->pico_frame_rxbuf_tot > 0) {
            memset(frame, 0, sizeof(frame));

            unsigned int len = 0;
            memcpy(&len, picotap->pico_frame_rxbuf, sizeof(len)); // get frame len
            //DEBUG_EXTRA("reading frame len = %ld", len);
            memcpy(frame, picotap->pico_frame_rxbuf + sizeof(len), len); // get frame data
            memmove(picotap->pico_frame_rxbuf, picotap->pico_frame_rxbuf + sizeof(len) + len, ZT_MAX_MTU-(sizeof(len) + len));
            int rx_ret = picotap->picostack->__pico_stack_recv(dev, (uint8_t*)frame, len); 
            picotap->pico_frame_rxbuf_tot-=(sizeof(len) + len);
            //DEBUG_EXTRA("rx_ret = %d", rx_ret);
            //DEBUG_EXTRA("RX frame buffer %3f full", (float)(picotap->pico_frame_rxbuf_tot) / (float)(MAX_PICO_FRAME_RX_BUF_SZ));
            loop_score--;
        }
        //DEBUG_ATTN("loop_score = %d", loop_score);
        return loop_score;
    }

    // Creates a new pico_socket and Connection object to represent a new connection to be.
    static Connection *pico_handleSocket(PhySocket *sock, void **uptr, struct socket_st* socket_rpc)
    {
    	DEBUG_INFO();
    	struct pico_socket * psock;
		#if defined(SDK_IPV4)
			psock = picotap->picostack->__pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, &pico_cb_tcp);
		#elif defined(SDK_IPV6)
			psock = picotap->picostack->__pico_socket_open(PICO_PROTO_IPV6, PICO_PROTO_TCP, &pico_cb_tcp);
		#endif
		if(psock) {
			DEBUG_ATTN("psock = %p", (void*)psock);
			int yes = 1;
			//picostack->__pico_socket_setoption(psock, PICO_TCP_NODELAY, &yes);
			Connection * newConn = new Connection();
	        *uptr = newConn;
	        newConn->type = socket_rpc->socket_type;
	        newConn->sock = sock;
			newConn->local_addr = NULL;
			newConn->peer_addr = NULL;
			newConn->picosock = psock;
	        picotap->_Connections.push_back(newConn);
	        return newConn;
		}
		else {
			DEBUG_ERROR("failed to create pico_socket");
		}
		return NULL;
    }

    // Writes data from the I/O buffer to the network stack
    static void pico_handleWrite(Connection *conn)
    {
    	DEBUG_INFO();
		if(!conn || !conn->picosock) {
			DEBUG_ERROR(" invalid connection");
			return;
		}
		int r, max_write_len = conn->txsz < ZT_MAX_MTU ? conn->txsz : ZT_MAX_MTU;
	    if((r = picotap->picostack->__pico_socket_write(conn->picosock, &conn->txbuf, max_write_len)) < 0) {
	    	DEBUG_ERROR("unable to write to pico_socket(%p)", (void*)&(conn->picosock));
	    	return;
	    }
	    /*
	 	if(pico_err == PICO_ERR_EINVAL)
	 		DEBUG_ERROR("PICO_ERR_EINVAL - invalid argument");
		if(pico_err == PICO_ERR_EIO)
			DEBUG_ERROR("PICO_ERR_EIO - input/output error");
		if(pico_err == PICO_ERR_ENOTCONN)
			DEBUG_ERROR("PICO_ERR_ENOTCONN - the socket is not connected");
		if(pico_err == PICO_ERR_ESHUTDOWN)
			DEBUG_ERROR("PICO_ERR_ESHUTDOWN - cannot send after transport endpoint shutdown");
		if(pico_err == PICO_ERR_EADDRNOTAVAIL)
			DEBUG_ERROR("PICO_ERR_EADDRNOTAVAIL - address not available");
		if(pico_err == PICO_ERR_EHOSTUNREACH)
			DEBUG_ERROR("PICO_ERR_EHOSTUNREACH - host is unreachable");
		if(pico_err == PICO_ERR_ENOMEM)
			DEBUG_ERROR("PICO_ERR_ENOMEM - not enough space");
		if(pico_err == PICO_ERR_EAGAIN)
			DEBUG_ERROR("PICO_ERR_EAGAIN - resource temporarily unavailable");
		*/
	    // adjust buffer
	    int sz = (conn->txsz)-r;
	   	if(sz)
	   		memmove(&conn->txbuf, (conn->txbuf+r), sz);
		conn->txsz -= r;
	   	int max = conn->type == SOCK_STREAM ? DEFAULT_TCP_TX_BUF_SZ : DEFAULT_UDP_TX_BUF_SZ;
	    DEBUG_TRANS("[TCP TX] --->    :: {TX: %.3f%%, RX: %.3f%%, sock=%p} :: %d bytes",
	    	(float)conn->txsz / (float)max, (float)conn->rxsz / max, (void*)&conn->sock, r);
    }

    // Instructs the stack to connect to a remote host
    static void pico_handleConnect(PhySocket *sock, PhySocket *rpcSock, Connection *conn, struct connect_st* connect_rpc)
    {
    	DEBUG_INFO();
		if(conn->picosock) {
			struct sockaddr_in *addr = (struct sockaddr_in *) &connect_rpc->addr;
			pico_address paddr;
			int ret;
			// TODO: Rewrite this
			#if defined(SDK_IPV4)
				struct pico_ip4 zaddr;
    			struct sockaddr_in *in4 = (struct sockaddr_in*)&connect_rpc->addr;
    			char ipv4_str[INET_ADDRSTRLEN];
    			inet_ntop(AF_INET, &(in4->sin_addr), ipv4_str, INET_ADDRSTRLEN);
				picotap->picostack->__pico_string_to_ipv4(ipv4_str, &(zaddr.addr));
				DEBUG_ATTN("addr=%s:%d", ipv4_str, (uint16_t*)&(addr->sin_port));
				ret = picotap->picostack->__pico_socket_connect(conn->picosock, &zaddr, addr->sin_port);
			#elif defined(SDK_IPV6) // "fd56:5799:d8f6:1238:8c99:9322:30ce:418a"
				struct pico_ip6 zaddr;
				struct sockaddr_in6 *in6 = (struct sockaddr_in6*)&connect_rpc->addr;
				char ipv6_str[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, &(in6->sin6_addr), ipv6_str, INET6_ADDRSTRLEN);
		    	picotap->picostack->__pico_string_to_ipv6(ipv6_str, zaddr.addr);
		    	DEBUG_ATTN("addr=%s:%d", ipv6_str, (uint16_t*)&(addr->sin_port));
				ret = picotap->picostack->__pico_socket_connect(conn->picosock, &zaddr, addr->sin_port);
			#endif
			
			if(ret == PICO_ERR_EPROTONOSUPPORT) {
				DEBUG_ERROR("PICO_ERR_EPROTONOSUPPORT");
			}
			if(ret == PICO_ERR_EINVAL) {
				DEBUG_ERROR("PICO_ERR_EINVAL");
			}
			if(ret == PICO_ERR_EHOSTUNREACH) {
				DEBUG_ERROR("PICO_ERR_EHOSTUNREACH");
			}
	        picotap->sendReturnValue(rpcSock, 0, ERR_OK);
		}
    }

    // Instructs the stack to bind to a given address
    static void pico_handleBind(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct bind_st *bind_rpc)
    {
    	DEBUG_INFO();
    	Connection *conn = picotap->getConnection(sock);
    	if(!sock) {
    		DEBUG_ERROR("invalid connection");
    		return;
    	}
		struct sockaddr_in *addr = (struct sockaddr_in *) &bind_rpc->addr;
    	int ret;
    	// TODO: Rewrite this
		#if defined(SDK_IPV4)
			struct pico_ip4 zaddr;
			struct sockaddr_in *in4 = (struct sockaddr_in*)&bind_rpc->addr;
			char ipv4_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(in4->sin_addr), ipv4_str, INET_ADDRSTRLEN);
			picotap->picostack->__pico_string_to_ipv4(ipv4_str, &(zaddr.addr));
			DEBUG_ATTN("addr=%s", ipv4_str/*, ntohs((uint16_t*)&(addr->sin_port))*/);
			ret = picotap->picostack->__pico_socket_bind(conn->picosock, &zaddr, (uint16_t*)&(addr->sin_port));
		#elif defined(SDK_IPV6)
			struct pico_ip6 zaddr;
			struct sockaddr_in6 *in6 = (struct sockaddr_in6*)&bind_rpc->addr;
			char ipv6_str[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &(in6->sin6_addr), ipv6_str, INET6_ADDRSTRLEN);
	    	picotap->picostack->__pico_string_to_ipv6(ipv6_str, zaddr.addr);
	    	DEBUG_ATTN("addr=%s", ipv6_str/*, ntohs((uint16_t*)&(addr->sin_port))*/);
			ret = picotap->picostack->__pico_socket_bind(conn->picosock, &zaddr, (uint16_t*)&(addr->sin_port));
		#endif
		if(ret < 0) {
			DEBUG_ERROR("unable to bind pico_socket(%p)", (void*)(conn->picosock));
			if(ret == PICO_ERR_EINVAL) {
				DEBUG_ERROR("PICO_ERR_EINVAL - invalid argument");
				picotap->sendReturnValue(rpcSock, -1, EINVAL);
			} 
			if(ret == PICO_ERR_ENOMEM) {
				DEBUG_ERROR("PICO_ERR_ENOMEM - not enough space");
				picotap->sendReturnValue(rpcSock, -1, ENOMEM);
			} 
			if(ret == PICO_ERR_ENXIO) {
				DEBUG_ERROR("PICO_ERR_ENXIO - no such device or address");
				picotap->sendReturnValue(rpcSock, -1, ENXIO);
			}
		}
		picotap->sendReturnValue(rpcSock, ERR_OK, ERR_OK); // success
    }

    // Puts a pico_socket into a listening state to receive incoming connection requests
    static void pico_handleListen(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct listen_st *listen_rpc)
    {
    	Connection *conn = picotap->getConnection(sock);
    	DEBUG_ATTN("conn = %p", (void*)conn);
    	if(!sock || !conn) {
    		DEBUG_ERROR("invalid connection");
    		return;
    	}
    	int ret, backlog = 1;
    	if((ret = picotap->picostack->__pico_socket_listen(conn->picosock, backlog)) < 0)
    	{
    		if(ret == PICO_ERR_EINVAL) {
    			DEBUG_ERROR("PICO_ERR_EINVAL - invalid argument");
    			picotap->sendReturnValue(rpcSock, -1, EINVAL);
    		}
			if(ret == PICO_ERR_EISCONN) {
				DEBUG_ERROR("PICO_ERR_EISCONN - socket is connected");
				picotap->sendReturnValue(rpcSock, -1, EISCONN);
			}
    	}
    	picotap->sendReturnValue(rpcSock, ERR_OK, ERR_OK); // success
    }

    // Feeds data into the client socket from the I/O buffer associated with the connection
    static void pico_handleRead(PhySocket *sock,void **uptr,bool lwip_invoked)
    {
    	// DEBUG_INFO();
		Connection *conn = picotap->getConnection(sock);
		if(conn && conn->rxsz) {
			float max = conn->type == SOCK_STREAM ? (float)DEFAULT_TCP_RX_BUF_SZ : (float)DEFAULT_UDP_RX_BUF_SZ;
			long n = picotap->_phy.streamSend(conn->sock, conn->rxbuf, /* ZT_MAX_MTU */ conn->rxsz);
			// DEBUG_INFO(" n=%d", n);
			// extract address and payload size info
			if(conn->type==SOCK_DGRAM) {
				int payload_sz, addr_sz_offset = sizeof(struct sockaddr_storage);
				memcpy(&payload_sz, conn->rxbuf + addr_sz_offset, sizeof(int));
				struct sockaddr_storage addr;
				memcpy(&addr, conn->rxbuf, addr_sz_offset);
				// adjust buffer
				if(conn->rxsz-n > 0) // If more remains on buffer
					memcpy(conn->rxbuf, conn->rxbuf+ZT_MAX_MTU, conn->rxsz - ZT_MAX_MTU);
			  	conn->rxsz -= ZT_MAX_MTU;
			}
			if(conn->type==SOCK_STREAM) {
				//int payload_sz, addr_sz_offset = sizeof(struct sockaddr_storage);
				//memcpy(&payload_sz, conn->rxbuf + addr_sz_offset, sizeof(int));
				//struct sockaddr_storage addr;
				//memcpy(&addr, conn->rxbuf, addr_sz_offset);
				// adjust buffer
				if(conn->rxsz-n > 0) // If more remains on buffer
					memcpy(conn->rxbuf, conn->rxbuf+n, conn->rxsz - n);
			  	conn->rxsz -= n;
			  	DEBUG_INFO("rxsz=%d", conn->rxsz);
			}
			if(n) {
				//DEBUG_INFO("wrote %d bytes to client application", n);
				if(conn->type==SOCK_STREAM) { // Only acknolwedge receipt of TCP packets
	            	DEBUG_TRANS("[TCP RX] <---    :: {TX: %.3f%%, RX: %.3f%%, sock=%p} :: %ld bytes",
	                	(float)conn->txsz / max, (float)conn->rxsz / max, (void*)conn->sock, n);
	        	}
	        	picotap->_phy.setNotifyWritable(conn->sock, true);
			}
			if(!n || !(conn->rxsz)) {
				//DEBUG_ERROR("error writing %d-byte-sized chunk to client socket", ZT_MAX_MTU);
				picotap->_phy.setNotifyWritable(conn->sock, false);
			}
		}
    }

    // Closes a pico_socket
    static void pico_handleClose(Connection *conn)
    {
    	DEBUG_INFO();
    	int ret;
    	if(conn && conn->picosock) {
	    	if((ret = picotap->picostack->__pico_socket_close(conn->picosock)) < 0) {
	    		DEBUG_ERROR("error closing pico_socket(%p)", (void*)(conn->picosock));
	    		// sendReturnValue()
	    	}
	    	return;
    	}
    	DEBUG_ERROR("invalid connection or pico_socket");
    }


/*------------------------------------------------------------------------------
-------------------------------- Tap Service  ----------------------------------
------------------------------------------------------------------------------*/

static err_t tapif_init(struct netif *netif)
{
  // Actual init functionality is in addIp() of tap
  return ERR_OK;
}

/*
 * Outputs data from the pbuf queue to the interface
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	struct pbuf *q;
	char buf[ZT_MAX_MTU+32];
	char *bufptr;
	int totalLength = 0;

	ZeroTier::NetconEthernetTap *tap = (ZeroTier::NetconEthernetTap*)netif->state;
	bufptr = buf;
	// Copy data from each pbuf, one at a time
	for(q = p; q != NULL; q = q->next) {
		memcpy(bufptr, q->payload, q->len);
		bufptr += q->len;
		totalLength += q->len;
	}
	// [Send packet to network]
	// Split ethernet header and feed into handler
	struct eth_hdr *ethhdr;
	ethhdr = (struct eth_hdr *)buf;

	ZeroTier::MAC src_mac;
	ZeroTier::MAC dest_mac;
	src_mac.setTo(ethhdr->src.addr, 6);
	dest_mac.setTo(ethhdr->dest.addr, 6);

	tap->_handler(tap->_arg,tap->_nwid,src_mac,dest_mac,
		Utils::ntoh((uint16_t)ethhdr->type),0,buf + sizeof(struct eth_hdr),totalLength - sizeof(struct eth_hdr));
	return ERR_OK;
}

// ---------------------------------------------------------------------------

NetconEthernetTap::NetconEthernetTap(
	const char *homePath,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
	void *arg) :
		_homePath(homePath),
		_mac(mac),
		_mtu(mtu),
		_nwid(nwid),
		_handler(handler),
		_arg(arg),
		_phy(this,false,true),
		_unixListenSocket((PhySocket *)0),
		_enabled(true),
		_run(true)
{
	sockstate = -1;
    char sockPath[4096],stackPath[4096];
    Utils::snprintf(sockPath,sizeof(sockPath),"%s%snc_%.16llx",homePath,ZT_PATH_SEPARATOR_S,_nwid,ZT_PATH_SEPARATOR_S,(unsigned long long)nwid);
    _dev = sockPath; // in SDK mode, set device to be just the network ID

	// SIP-0
	// Load and initialize network stack library

    #if defined(SDK_LWIP)
		Utils::snprintf(stackPath,sizeof(stackPath),"%s%sliblwip.so",homePath,ZT_PATH_SEPARATOR_S);
		lwipstack = new lwIP_stack(stackPath);
		if(!lwipstack) {
			DEBUG_ERROR("unable to dynamically load a new instance of (%s) (searched ZeroTier home path)", stackPath);
			throw std::runtime_error("");
		}
		lwipstack->__lwip_init();
	#elif defined(SDK_PICOTCP)
		Utils::snprintf(stackPath,sizeof(stackPath),"%s%slibpicotcp.so",homePath,ZT_PATH_SEPARATOR_S);
		picostack = new picoTCP_stack(stackPath);
		if(!picostack) {
			DEBUG_ERROR("unable to dynamically load a new instance of (%s) (searched ZeroTier home path)", stackPath);
			throw std::runtime_error("");
		}
		picostack->__pico_stack_init();
	#elif defined(SDK_JIP)
		Utils::snprintf(stackPath,sizeof(stackPath),"%s%slibjip.so",homePath,ZT_PATH_SEPARATOR_S);
		jipstack = new jip_stack(stackPath);
	#endif

	_unixListenSocket = _phy.unixListen(sockPath,(void *)this);
	DEBUG_INFO("tap initialized on: path=%s", sockPath);
	if (!_unixListenSocket)
		DEBUG_ERROR("unable to bind to: path=%s", sockPath);
     _thread = Thread::start(this);
}

NetconEthernetTap::~NetconEthernetTap()
{
	_run = false;
	_phy.whack();
	_phy.whack(); // TODO: Rationale?
	Thread::join(_thread);
	_phy.close(_unixListenSocket,false);
	#if defined(SDK_LWIP)
		delete lwipstack;
	#endif
	#if defined(SDK_PICOTCP)
		delete picostack;
	#endif
	#if defined(SDK_JIP)
		delete jipstack;
	#endif
}

void NetconEthernetTap::setEnabled(bool en)
{
	_enabled = en;
}

bool NetconEthernetTap::enabled() const
{
	return _enabled;
}

void NetconEthernetTap::lwIP_init_interface(const InetAddress &ip)
{
	#if defined(SDK_LWIP)
	DEBUG_INFO("local_addr=%s", ip.toString().c_str());
	Mutex::Lock _l(_ips_m);

	if (std::find(_ips.begin(),_ips.end(),ip) == _ips.end()) {
		_ips.push_back(ip);
		std::sort(_ips.begin(),_ips.end());

	#if defined(SDK_IPV4)
		if (ip.isV4()) {			
			DEBUG_INFO("IPV4");
			// convert address
			static ip_addr_t ipaddr, netmask, gw;
			IP4_ADDR((ip4_addr_t *)&gw,127,0,0,1);
			((ip4_addr_t *)&ipaddr)->addr = *((u32_t *)ip.rawIpData());
			((ip4_addr_t *)&netmask)->addr = *((u32_t *)ip.netmask().rawIpData());
			// initialize netif
			lwipstack->__netif_add(&interface,&ipaddr, &netmask, &gw, NULL, tapif_init, lwipstack->_ethernet_input);
			interface.state = this;
			interface.output = lwipstack->_etharp_output;
			_mac.copyTo(interface.hwaddr, 6);
			interface.mtu = _mtu;
			interface.name[0] = 'l';
			interface.name[1] = 'w';
			interface.name[2] = '4';
			interface.linkoutput = low_level_output;
			interface.hwaddr_len = 6;
			interface.flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
			lwipstack->__netif_set_default(&interface);
			lwipstack->__netif_set_up(&interface);			
		}
	#endif

	#if defined(SDK_IPV6)
		if(ip.isV6()) {
			DEBUG_INFO("IPV6");
			// convert address
			static ip6_addr_t addr6;
		    struct sockaddr_in6 in6;
		    memcpy(in6.sin6_addr.s6_addr,ip.rawIpData(),16);
		    in6_to_ip6((ip6_addr *)&addr6, &in6);
		    // initialize netif
			interface6.mtu = _mtu;
			interface6.name[0] = 'l';
			interface6.name[1] = 'w';
			interface6.name[2] = '6';
			interface6.hwaddr_len = 6;
			interface6.linkoutput = low_level_output;
			interface6.ip6_autoconfig_enabled = 1;
			_mac.copyTo(interface6.hwaddr, interface6.hwaddr_len);
			lwipstack->__netif_create_ip6_linklocal_address(&interface6, 1);
			lwipstack->__netif_add(&interface6, NULL, tapif_init, lwipstack->_ethernet_input);
			lwipstack->__netif_set_default(&interface6);
			lwipstack->__netif_set_up(&interface6);	
			netif_ip6_addr_set_state(&interface6, 1, IP6_ADDR_TENTATIVE); 
			ip6_addr_copy(ip_2_ip6(interface6.ip6_addr[1]), addr6);
			interface6.output_ip6 = lwipstack->_ethip6_output;
			interface6.state = this;
			interface6.flags = NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
		}	
	#endif	
	}
	#endif
}

void NetconEthernetTap::jip_init_interface(const InetAddress &ip)
{
	// will be similar to lwIP initialization process
}

bool NetconEthernetTap::addIp(const InetAddress &ip)
{
	picotap = this;
	// SIP-1
	// Initialize network stack's interface, assign addresses
    #if defined(SDK_LWIP)
		lwIP_init_interface(ip);
	#elif defined(SDK_PICOTCP)
		pico_init_interface(this, ip);
	#elif defined(SDK_JIP)
		jip_init_interface(ip);
	#endif
	return true;
}

bool NetconEthernetTap::removeIp(const InetAddress &ip)
{
	Mutex::Lock _l(_ips_m);
	std::vector<InetAddress>::iterator i(std::find(_ips.begin(),_ips.end(),ip));
	if (i == _ips.end())
		return false;
	_ips.erase(i);
	if (ip.isV4()) {
		// TODO: dealloc from LWIP
	}
	return true;
}

std::vector<InetAddress> NetconEthernetTap::ips() const
{
	Mutex::Lock _l(_ips_m);
	return _ips;
}


void NetconEthernetTap::lwIP_rx(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	#if defined(SDK_LWIP)
		DEBUG_INFO();
		struct pbuf *p,*q;
		if (!_enabled)
			return;
		struct eth_hdr ethhdr;
		from.copyTo(ethhdr.src.addr, 6);
		to.copyTo(ethhdr.dest.addr, 6);
		ethhdr.type = Utils::hton((uint16_t)etherType);
		
		p = lwipstack->__pbuf_alloc(PBUF_RAW, len+sizeof(struct eth_hdr), PBUF_POOL);
		if (p != NULL) {
			const char *dataptr = reinterpret_cast<const char *>(data);
			// First pbuf gets ethernet header at start
			q = p;
			if (q->len < sizeof(ethhdr)) {
				DEBUG_ERROR("dropped packet: first pbuf smaller than ethernet header");
				return;
			}
			memcpy(q->payload,&ethhdr,sizeof(ethhdr));
			memcpy((char*)q->payload + sizeof(ethhdr),dataptr,q->len - sizeof(ethhdr));
			dataptr += q->len - sizeof(ethhdr);

			// Remaining pbufs (if any) get rest of data
			while ((q = q->next)) {
				memcpy(q->payload,dataptr,q->len);
				dataptr += q->len;
			}
		} 
		else {
			DEBUG_ERROR("dropped packet: no pbufs available");
			return;
		}
		{
			#if defined(SDK_IPV6)
				if(interface6.input(p, &interface6) != ERR_OK) {
					DEBUG_ERROR("error while feeding frame into stack interface6");
				}
			#endif
			#if defined(SDK_IPV4)
				if(interface.input(p, &interface) != ERR_OK) {
					DEBUG_ERROR("error while feeding frame into stack interface");
				}
			#endif
		}
	#endif
}


void NetconEthernetTap::jip_rx(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	DEBUG_INFO();
}

void NetconEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
    // DEBUG_EXTRA("RX packet: len=%d, etherType=%d", len, etherType);
    // SIP-
    // RX packet
    #if defined(SDK_LWIP)
		lwIP_rx(from,to,etherType,data,len);
	#elif defined(SDK_PICOTCP)
		pico_rx(this, from,to,etherType,data,len);
	#elif defined(SDK_JIP)
		jip_rx(from,to,etherType,data,len);
	#endif
}

std::string NetconEthernetTap::deviceName() const
{
	return _dev;
}

void NetconEthernetTap::setFriendlyName(const char *friendlyName) {
}

void NetconEthernetTap::scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed)
{
	std::vector<MulticastGroup> newGroups;
	Mutex::Lock _l(_multicastGroups_m);
	// TODO: get multicast subscriptions from LWIP
	std::vector<InetAddress> allIps(ips());
	for(std::vector<InetAddress>::iterator ip(allIps.begin());ip!=allIps.end();++ip)
		newGroups.push_back(MulticastGroup::deriveMulticastGroupForAddressResolution(*ip));

	std::sort(newGroups.begin(),newGroups.end());
	std::unique(newGroups.begin(),newGroups.end());

	for(std::vector<MulticastGroup>::iterator m(newGroups.begin());m!=newGroups.end();++m) {
		if (!std::binary_search(_multicastGroups.begin(),_multicastGroups.end(),*m))
			added.push_back(*m);
	}
	for(std::vector<MulticastGroup>::iterator m(_multicastGroups.begin());m!=_multicastGroups.end();++m) {
		if (!std::binary_search(newGroups.begin(),newGroups.end(),*m))
			removed.push_back(*m);
	}
	_multicastGroups.swap(newGroups);
}
    
void NetconEthernetTap::lwIP_loop()
{
	#if defined(SDK_LWIP)
		DEBUG_INFO();
		uint64_t prev_tcp_time = 0, prev_status_time = 0, prev_discovery_time = 0;
		// Main timer loop
		while (_run) {
			uint64_t now = OSUtils::now();
			uint64_t since_tcp = now - prev_tcp_time;
			uint64_t since_discovery = now - prev_discovery_time;
			uint64_t since_status = now - prev_status_time;
			uint64_t tcp_remaining = ZT_LWIP_TCP_TIMER_INTERVAL;
			uint64_t discovery_remaining = 5000;

			#if defined(LWIP_IPV6)
				#define DISCOVERY_INTERVAL 	1000 // fuck you
			#elif
				#define DISCOVERY_INTERVAL ARP_TMR_INTERVAL
			#endif

			// Connection prunning
			if (since_status >= STATUS_TMR_INTERVAL) {
				prev_status_time = now;
				for(size_t i=0;i<_Connections.size();++i) {
					if(!_Connections[i]->sock || _Connections[i]->type != SOCK_STREAM)
						continue;
					int fd = _phy.getDescriptor(_Connections[i]->sock);
					// DEBUG_INFO(" tap_thread(): tcp\\jobs = {%d, %d}\n", _Connection.size(), jobmap.size());
					// If there's anything on the RX buf, set to notify in case we stalled
					if(_Connections[i]->rxsz > 0)
						_phy.setNotifyWritable(_Connections[i]->sock, true);
					fcntl(fd, F_SETFL, O_NONBLOCK);
					unsigned char tmpbuf[BUF_SZ];
					
					ssize_t n = read(fd,&tmpbuf,BUF_SZ);
					if(_Connections[i]->TCP_pcb->state == SYN_SENT) {
						DEBUG_EXTRA("  should finish or be removed soon, sock=%p, state=SYN_SENT", 
							(void*)&(_Connections[i]->sock));
					}
					if((n < 0 && errno != EAGAIN) || (n == 0 && errno == EAGAIN)) {
						//DEBUG_INFO(" closing sock (%x)", (void*)_Connections[i]->sock);
						closeConnection(_Connections[i]->sock);
					} else if (n > 0) {
						DEBUG_INFO(" data read during connection check (%ld bytes)", n);
						phyOnUnixData(_Connections[i]->sock,_phy.getuptr(_Connections[i]->sock),&tmpbuf,n);
					}		
				}
			}
			// Main TCP/ETHARP timer section
			if (since_tcp >= ZT_LWIP_TCP_TIMER_INTERVAL) {
				prev_tcp_time = now;
	            lwipstack->__tcp_tmr();
	            // FIXME: could be removed or refactored?
	            // Makeshift poll
				for(size_t i=0;i<_Connections.size();++i) {
					if(_Connections[i]->txsz > 0){
						handleWrite(_Connections[i]);
					}
				}
			} else {
				tcp_remaining = ZT_LWIP_TCP_TIMER_INTERVAL - since_tcp;
			}
			if (since_discovery >= DISCOVERY_INTERVAL) {
				prev_discovery_time = now;
				#if defined(SDK_IPV4)
					lwipstack->__etharp_tmr();
				#endif
				#if defined(SDK_IPV6)
					lwipstack->__nd6_tmr();
				#endif
	        } else {
				discovery_remaining = DISCOVERY_INTERVAL - since_discovery;
			}
			_phy.poll((unsigned long)std::min(tcp_remaining,discovery_remaining));
		}
	    lwipstack->close();
    #endif
}

void NetconEthernetTap::jip_loop()
{
	DEBUG_INFO();
	while(_run)
	{

	}
}

void NetconEthernetTap::threadMain()
	throw()
{
	// SIP-2
	// Enter main thread loop for network stack
    #if defined(SDK_LWIP)
		lwIP_loop();
	#elif defined(SDK_PICOTCP)
		pico_loop(this);
	#elif defined(SDK_JIP)
		jip_loop();
	#endif
}

Connection *NetconEthernetTap::getConnection(PhySocket *sock)
{
	for(size_t i=0;i<_Connections.size();++i) {
		if(_Connections[i]->sock == sock)
			return _Connections[i];
	}
	return NULL;
}

Connection *NetconEthernetTap::getConnection(struct pico_socket *sock)
{
	for(size_t i=0;i<_Connections.size();++i) {
		if(_Connections[i]->picosock == sock)
			return _Connections[i];
	}
	return NULL;
}

void NetconEthernetTap::closeConnection(PhySocket *sock)
{
    DEBUG_EXTRA("sock=%p", (void*)sock);
    //return;

	Mutex::Lock _l(_close_m);
	// Here we assume _tcpconns_m is already locked by caller
	if(!sock) {
		DEBUG_EXTRA("invalid PhySocket");
		return;
	}
	Connection *conn = getConnection(sock);
	if(!conn)
		return;
    
	// picoTCP
	#if defined(SDK_PICOTCP)
		//pico_handleClose(conn);
	#endif

    // lwIP
	#if defined(SDK_LWIP)
	    if(conn->type==SOCK_DGRAM) {
	        lwipstack->__udp_remove(conn->UDP_pcb);
	    }
		if(conn->TCP_pcb && conn->TCP_pcb->state != CLOSED) {
			DEBUG_EXTRA("conn=%p, sock=%p, PCB->state = %d", 
				(void*)&conn, (void*)&sock, conn->TCP_pcb->state);
			if(conn->TCP_pcb->state == SYN_SENT /*|| conn->TCP_pcb->state == CLOSE_WAIT*/) {
				DEBUG_EXTRA("ignoring close request. invalid PCB state for this operation. sock=%p", (void*)&sock);
				return;
			}	
			DEBUG_BLANK("__tcp_close(...)");
			if(lwipstack->__tcp_close(conn->TCP_pcb) == ERR_OK) {
				// Unregister callbacks for this PCB
				lwipstack->__tcp_arg(conn->TCP_pcb, NULL);
			    lwipstack->__tcp_recv(conn->TCP_pcb, NULL);
			    lwipstack->__tcp_err(conn->TCP_pcb, NULL);
			    lwipstack->__tcp_sent(conn->TCP_pcb, NULL);
			    lwipstack->__tcp_poll(conn->TCP_pcb, NULL, 1);
			}
			else {
				DEBUG_EXTRA("error while calling tcp_close() sock=%p", (void*)&sock);
			}
		}
	#endif
	for(size_t i=0;i<_Connections.size();++i) {
		if(_Connections[i] == conn){
			_Connections.erase(_Connections.begin() + i);
			delete conn;
			break;
		}
	}
	if(!sock)
		return;
	close(_phy.getDescriptor(sock));
	_phy.close(sock, false);
}

void NetconEthernetTap::phyOnUnixClose(PhySocket *sock,void **uptr) {
    DEBUG_EXTRA("sock=%p", (void*)&sock);
	Mutex::Lock _l(_tcpconns_m);
    //closeConnection(sock);
}


void NetconEthernetTap::handleRead(PhySocket *sock,void **uptr,bool lwip_invoked)
{
	//DEBUG_EXTRA("handleRead(sock=%p): lwip_invoked = %d\n", (void*)&sock, lwip_invoked);

	// picoTCP
	#if defined(SDK_PICOTCP)
		pico_handleRead(sock, uptr, lwip_invoked);
	#endif

	// lwIP
	#if defined(SDK_LWIP)
		if(!lwip_invoked) {
			_tcpconns_m.lock();
			_rx_buf_m.lock();
		}
		Connection *conn = getConnection(sock);	
		if(conn && conn->rxsz) {
			float max = conn->type == SOCK_STREAM ? (float)DEFAULT_TCP_RX_BUF_SZ : (float)DEFAULT_UDP_RX_BUF_SZ;
			long n = _phy.streamSend(conn->sock, conn->rxbuf, ZT_MAX_MTU);
			int payload_sz, addr_sz_offset = sizeof(struct sockaddr_storage);
			memcpy(&payload_sz, conn->rxbuf + addr_sz_offset, sizeof(int)); // OPT:
			// extract address
			struct sockaddr_storage addr;
			memcpy(&addr, conn->rxbuf, addr_sz_offset);
			
			if(n == ZT_MAX_MTU) {
				if(conn->rxsz-n > 0) // If more remains on buffer
					memcpy(conn->rxbuf, conn->rxbuf+ZT_MAX_MTU, conn->rxsz - ZT_MAX_MTU);
			  	conn->rxsz -= ZT_MAX_MTU;
				// DGRAM
	            if(conn->type==SOCK_DGRAM){
	                _phy.setNotifyWritable(conn->sock, false);

				#if DEBUG_LEVEL >= MSG_TRANSFER
					struct sockaddr_in * addr_in2 = (struct sockaddr_in *)&addr;
					int port = lwipstack->__lwip_ntohs(addr_in2->sin_port);
					int ip = addr_in2->sin_addr.s_addr;
					unsigned char d[4];
					d[0] = ip & 0xFF;
					d[1] = (ip >>  8) & 0xFF;
					d[2] = (ip >> 16) & 0xFF;
					d[3] = (ip >> 24) & 0xFF;
	            	DEBUG_TRANS("UDP RX <---    :: {TX: %.3f%%, RX: %d, sock=%p} :: payload = %d bytes (src_addr=%d.%d.%d.%d:%d)", 
						(float)conn->txsz / max, conn->rxsz/* / max*/, (void*)conn->sock, payload_sz, d[0],d[1],d[2],d[3], port);
				#endif
	            }
				// STREAM
	            //DEBUG_INFO("phyOnUnixWritable(): tid = %d\n", pthread_mach_thread_np(pthread_self()));
	            if(conn->type==SOCK_STREAM) { // Only acknolwedge receipt of TCP packets
	                lwipstack->__tcp_recved(conn->TCP_pcb, n);
	            	DEBUG_TRANS("TCP RX <---    :: {TX: %.3f%%, RX: %.3f%%, sock=%p} :: %ld bytes",
	                	(float)conn->txsz / max, (float)conn->rxsz / max, (void*)conn->sock, n);
	        	}
			} else {
				DEBUG_EXTRA(" errno = %d, rxsz = %d", errno, conn->rxsz);
				_phy.setNotifyWritable(conn->sock, false);
			}
		}
	    // If everything on the buffer has been written
	    if(conn->rxsz == 0) {
	        _phy.setNotifyWritable(conn->sock, false);
	    }
		if(!lwip_invoked) {
			_tcpconns_m.unlock();
			_rx_buf_m.unlock();
		}
	#endif
}

void NetconEthernetTap::phyOnUnixWritable(PhySocket *sock,void **uptr,bool lwip_invoked)
{
	handleRead(sock,uptr,lwip_invoked);
}

void NetconEthernetTap::phyOnUnixData(PhySocket *sock, void **uptr, void *data, ssize_t len)
{
    DEBUG_EXTRA("sock=%p, len=%d", (void*)&sock, (int)len);
	uint64_t CANARY_num;
	pid_t pid, tid;
	ssize_t wlen = len;
	char cmd, timestamp[20], CANARY[CANARY_SZ], padding[] = {PADDING};
	void *payload;
	unsigned char *buf = (unsigned char*)data;
	std::pair<PhySocket*, void*> sockdata;
	PhySocket *rpcSock;
	bool foundJob = false, detected_rpc = false;
	Connection *conn;
	// RPC
	char phrase[RPC_PHRASE_SZ];
	memset(phrase, 0, RPC_PHRASE_SZ);
	if(len == BUF_SZ) {
		memcpy(phrase, buf, RPC_PHRASE_SZ);
		if(strcmp(phrase, RPC_PHRASE) == 0)
			detected_rpc = true;
	}
	if(detected_rpc) {
		unloadRPC(data, pid, tid, timestamp, CANARY, cmd, payload);
		memcpy(&CANARY_num, CANARY, CANARY_SZ);
		// DEBUG_EXTRA(" RPC: sock=%p, (pid=%d, tid=%d, timestamp=%s, cmd=%d)", (void*)&sock, pid, tid, timestamp, cmd);

		if(cmd == RPC_SOCKET) {				
			DEBUG_INFO("  RPC_SOCKET, sock=%p", (void*)&sock);
			// Create new lwip socket and associate it with this sock
			struct socket_st socket_rpc;
			memcpy(&socket_rpc, &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct socket_st));
			Connection * new_conn;
			if((new_conn = handleSocket(sock, uptr, &socket_rpc))) {
				new_conn->pid = pid; // Merely kept to look up application path/names later, not strictly necessary
			}
		} else {
			jobmap[CANARY_num] = std::pair<PhySocket*, void*>(sock, data);
		}
		write(_phy.getDescriptor(sock), "z", 1); // RPC ACK byte to maintain order
	}
	// STREAM
	else {
		int data_start = -1, data_end = -1, canary_pos = -1, padding_pos = -1;
		// Look for padding
		std::string padding_pattern(padding, padding+PADDING_SZ);
		std::string buffer(buf, buf + len);
		padding_pos = buffer.find(padding_pattern);
		canary_pos = padding_pos-CANARY_SZ;
		// Grab token, next we'll use it to look up an RPC job
		if(canary_pos > -1) {
			memcpy(&CANARY_num, buf+canary_pos, CANARY_SZ);
			if(CANARY_num != 0) {
				// Find job
				sockdata = jobmap[CANARY_num];
				if(!sockdata.first) {
					DEBUG_ERROR(" unable to locate job entry for %lu, sock=%p", CANARY_num, (void*)&sock);
					return;
				}  else
					foundJob = true;
			}
		}

		conn = getConnection(sock);
		if(!conn)
			return;

		if(padding_pos == -1) { // [DATA]
			memcpy(&conn->txbuf[conn->txsz], buf, wlen);
		} else { // Padding found, implies a canary is present
			// [CANARY]
			if(len == CANARY_SZ+PADDING_SZ && canary_pos == 0) {
				wlen = 0; // Nothing to write
			} else {
				// [CANARY] + [DATA]
				if(len > CANARY_SZ+PADDING_SZ && canary_pos == 0) {
					wlen = len - CANARY_SZ+PADDING_SZ;
					data_start = padding_pos+PADDING_SZ;
					memcpy((&conn->txbuf)+conn->txsz, buf+data_start, wlen);
				}
				// [DATA] + [CANARY]
				if(len > CANARY_SZ+PADDING_SZ && canary_pos > 0 && canary_pos == len - CANARY_SZ+PADDING_SZ) {
					wlen = len - CANARY_SZ+PADDING_SZ;
					data_start = 0;
					memcpy((&conn->txbuf)+conn->txsz, buf+data_start, wlen);												
				}
				// [DATA] + [CANARY] + [DATA]
				if(len > CANARY_SZ+PADDING_SZ && canary_pos > 0 && len > (canary_pos + CANARY_SZ+PADDING_SZ)) {
					wlen = len - CANARY_SZ+PADDING_SZ;
					data_start = 0;
					data_end = padding_pos-CANARY_SZ;
					memcpy((&conn->txbuf)+conn->txsz, buf+data_start, (data_end-data_start)+1);
					memcpy((&conn->txbuf)+conn->txsz, buf+(padding_pos+PADDING_SZ), len-(canary_pos+CANARY_SZ+PADDING_SZ));
				}
			}
		}
		// Write data from stream
        if(wlen) {
            if(conn->type == SOCK_STREAM) { // We only disable TCP "connections"
                int softmax = conn->type == SOCK_STREAM ? DEFAULT_TCP_RX_BUF_SZ : DEFAULT_UDP_RX_BUF_SZ;
                if(conn->txsz > softmax) {
                    _phy.setNotifyReadable(sock, false);
                    conn->disabled = true;
                }
                else if (conn->disabled) {
                    conn->disabled = false;
                    _phy.setNotifyReadable(sock, true);
                }
            }
            conn->txsz += wlen;
            handleWrite(conn);
        }
	}
	// Process RPC if we have a corresponding jobmap entry
    if(foundJob) {
        rpcSock = sockdata.first;
        buf = (unsigned char*)sockdata.second;
		unloadRPC(buf, pid, tid, timestamp, CANARY, cmd, payload);
		//DEBUG_EXTRA(" RPC: sock=%p, (pid=%d, tid=%d, timestamp=%s, cmd=%d)", (void*)&sock, pid, tid, timestamp, cmd);

		switch(cmd) {
			case RPC_BIND:
				DEBUG_INFO("  RPC_BIND, sock=%p", (void*)&sock);
			    struct bind_st bind_rpc;
			    memcpy(&bind_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct bind_st));
			    handleBind(sock, rpcSock, uptr, &bind_rpc);
				break;
		  	case RPC_LISTEN:
		  		DEBUG_INFO("  RPC_LISTEN, sock=%p", (void*)&sock);
			    struct listen_st listen_rpc;
			    memcpy(&listen_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct listen_st));
			    handleListen(sock, rpcSock, uptr, &listen_rpc);
				break;
		  	case RPC_GETSOCKNAME:
		  		DEBUG_INFO("  RPC_GETSOCKNAME, sock=%p", (void*)&sock);
		  		struct getsockname_st getsockname_rpc;
		    	memcpy(&getsockname_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct getsockname_st));
		  		handleGetsockname(sock, rpcSock, uptr, &getsockname_rpc);
		  		break;
			case RPC_GETPEERNAME:
		  		DEBUG_INFO("  RPC_GETPEERNAME, sock=%p", (void*)&sock);
		  		struct getsockname_st getpeername_rpc;
		    	memcpy(&getpeername_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct getsockname_st));
		  		handleGetpeername(sock, rpcSock, uptr, &getpeername_rpc);
		  		break;
			case RPC_CONNECT:
				DEBUG_INFO("  RPC_CONNECT, sock=%p", (void*)&sock);
			    struct connect_st connect_rpc;
			    memcpy(&connect_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct connect_st));
			    handleConnect(sock, rpcSock, conn, &connect_rpc);
			    jobmap.erase(CANARY_num);
				return; // Keep open RPC, we'll use it once in nc_connected to send retval
		  	default:
				break;
		}
		Mutex::Lock _l(_tcpconns_m);
		closeConnection(sockdata.first); // close RPC after sending retval, no longer needed
		jobmap.erase(CANARY_num);
		return;
	}
}

int NetconEthernetTap::sendReturnValue(PhySocket *sock, int retval, int _errno = 0){
    DEBUG_EXTRA("sock=%p", (void*)&sock);
	return sendReturnValue(_phy.getDescriptor(sock), retval, _errno);
}
int NetconEthernetTap::sendReturnValue(int fd, int retval, int _errno = 0)
{
//#if !defined(USE_SOCKS_PROXY)
	DEBUG_EXTRA("fd=%d, retval=%d, errno=%d", fd, retval, _errno);
	int sz = sizeof(char) + sizeof(retval) + sizeof(errno);
	char retmsg[sz];
	memset(&retmsg, 0, sizeof(retmsg));
	retmsg[0]=RPC_RETVAL;
	memcpy(&retmsg[1], &retval, sizeof(retval));
	memcpy(&retmsg[1]+sizeof(retval), &_errno, sizeof(_errno));
	return write(fd, &retmsg, sz);
//#else
//    return 1;
//#endif
}

void NetconEthernetTap::unloadRPC(void *data, pid_t &pid, pid_t &tid, 
	char (timestamp[RPC_TIMESTAMP_SZ]), char (CANARY[sizeof(uint64_t)]), char &cmd, void* &payload)
{
	unsigned char *buf = (unsigned char*)data;
	memcpy(&pid, &buf[IDX_PID], sizeof(pid_t));
	memcpy(&tid, &buf[IDX_TID], sizeof(pid_t));
	memcpy(timestamp, &buf[IDX_TIME], RPC_TIMESTAMP_SZ);
	memcpy(&cmd, &buf[IDX_PAYLOAD], sizeof(char));
	memcpy(CANARY, &buf[IDX_PAYLOAD+1], CANARY_SZ);
}

/*------------------------------------------------------------------------------
--------------------------------- LWIP callbacks -------------------------------
------------------------------------------------------------------------------*/

#if defined(SDK_LWIP)

err_t NetconEthernetTap::nc_accept(void *arg, struct tcp_pcb *newPCB, err_t err)
{
	DEBUG_ATTN("pcb=%p", (void*)&newPCB);
	Larg *l = (Larg*)arg;
    Mutex::Lock _l(l->tap->_tcpconns_m);
	Connection *conn = l->conn;
	NetconEthernetTap *tap = l->tap;

    if(!conn)
        return -1;
    if(conn->type==SOCK_DGRAM)
        return -1;
	if(!conn->sock)
		return -1;
	int fd = tap->_phy.getDescriptor(conn->sock);

  	if(conn) {
	  	// create new socketpair
	  	ZT_PHY_SOCKFD_TYPE fds[2];
		if(socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
			if(errno < 0) {
				l->tap->sendReturnValue(conn, -1, errno);
				DEBUG_ERROR("unable to create socketpair");
				return ERR_MEM;
			}
		}
		// create and populate new Connection
		Connection *newTcpConn = new Connection();
		l->tap->_Connections.push_back(newTcpConn);
		newTcpConn->TCP_pcb = newPCB;
		newTcpConn->type = SOCK_STREAM;
		newTcpConn->sock = tap->_phy.wrapSocket(fds[0], newTcpConn);

		if(sock_fd_write(fd, fds[1]) < 0)
	  		return -1;
	    tap->lwipstack->__tcp_arg(newPCB, new Larg(tap, newTcpConn));
	    tap->lwipstack->__tcp_recv(newPCB, nc_recved);
	    tap->lwipstack->__tcp_err(newPCB, nc_err);
	    tap->lwipstack->__tcp_sent(newPCB, nc_sent);
	    tap->lwipstack->__tcp_poll(newPCB, nc_poll, 1);
	    if(conn->TCP_pcb->state == LISTEN)
	    	return ERR_OK;
	    tcp_accepted(conn->TCP_pcb); // Let lwIP know that it can queue additional incoming connections
		return ERR_OK;
  	} else
  		DEBUG_ERROR("can't locate Connection object for PCB");
  	return -1;
}
    
void NetconEthernetTap::nc_udp_recved(void * arg, struct udp_pcb * upcb, struct pbuf * p, ip_addr_t * addr, u16_t port)
{
    Larg *l = (Larg*)arg;
    DEBUG_EXTRA("nc_udp_recved(conn=%p,pcb=%p,port=%d)\n", (void*)&(l->conn), (void*)&upcb, port);
    /*
    int tot = 0;
	unsigned char *addr_pos, *sz_pos, *payload_pos;
    struct pbuf* q = p;
    struct sockaddr_storage sockaddr_big;

#if defined(LWIP_IPV6)
	struct sockaddr_in6 addr_in;
	addr_in.sin6_addr.s6_addr = addr->u_addr.ip6.addr;
	addr_in.sin6_port = port;
#else // ipv4
	struct sockaddr_in *addr_in = (struct sockaddr_in *)&sockaddr_big;
	addr_in->sin_addr.s_addr = addr->addr;
	addr_in->sin_port = port;
#endif

	// TODO: Finish address treatment

    Mutex::Lock _l2(l->tap->_rx_buf_m);
    // Cycle through pbufs and write them to the RX buffer
    // The RX "buffer" will be emptied via phyOnUnixWritable()
	if(p) {
		// Intra-API "packetization" scheme: [addr_len|addr|payload_len|payload]
		if(l->conn->rxsz == DEFAULT_UDP_RX_BUF_SZ) { // if UDP buffer full
			DEBUG_INFO("UDP RX buffer full. Discarding oldest payload segment");
			memmove(l->conn->rxbuf, l->conn->rxbuf + ZT_MAX_MTU, DEFAULT_UDP_RX_BUF_SZ - ZT_MAX_MTU);
			addr_pos = l->conn->rxbuf + (DEFAULT_UDP_RX_BUF_SZ - ZT_MAX_MTU); // TODO:
			sz_pos = addr_pos + sizeof(struct sockaddr_storage);
			l->conn->rxsz -= ZT_MAX_MTU;
		}
		else {
			addr_pos = l->conn->rxbuf + l->conn->rxsz; // where we'll prepend the size of the address
			sz_pos = addr_pos + sizeof(struct sockaddr_storage);
		}
		payload_pos = addr_pos + sizeof(struct sockaddr_storage) + sizeof(tot); // where we'll write the payload
		// write remote host address
		memcpy(addr_pos, &addr_in, sizeof(struct sockaddr_storage));
    }
    while(p != NULL) {
        if(p->len <= 0)
            break;
        int len = p->len;
        memcpy(payload_pos, p->payload, len);
		payload_pos = payload_pos + len;
        p = p->next;
        tot += len;
    }
    if(tot) {
		l->conn->rxsz += ZT_MAX_MTU;
		memcpy(sz_pos, &tot, sizeof(tot));
        //DEBUG_EXTRA(" nc_udp_recved(): data_len = %d, rxsz = %d, addr_info_len = %d\n", 
		//	tot, l->conn->rxsz, sizeof(u32_t) + sizeof(u16_t));
        l->tap->phyOnUnixWritable(l->conn->sock, NULL, true);
        l->tap->_phy.setNotifyWritable(l->conn->sock, true);
    }
    l->tap->lwipstack->__pbuf_free(q);
    */
}
   

err_t NetconEthernetTap::nc_recved(void *arg, struct tcp_pcb *PCB, struct pbuf *p, err_t err)
{
    Larg *l = (Larg*)arg;
    DEBUG_EXTRA("conn=%p, pcb=%p", (void*)&(l->conn), (void*)&PCB);
	int tot = 0;
  	struct pbuf* q = p;
	Mutex::Lock _l(l->tap->_tcpconns_m);

	if(!l->conn) {
		DEBUG_ERROR("no connection");
		return ERR_OK; 
	}
	if(p == NULL) {
		if(l->conn->TCP_pcb->state == CLOSE_WAIT){
			l->tap->closeConnection(l->conn->sock);
			return ERR_ABRT;
		}
		return err;
	}
	Mutex::Lock _l2(l->tap->_rx_buf_m);
	// Cycle through pbufs and write them to the RX buffer
	// The RX buffer will be emptied via phyOnUnixWritable()
	while(p != NULL) {
		if(p->len <= 0)
			break;
		int avail = DEFAULT_TCP_RX_BUF_SZ - l->conn->rxsz;
		int len = p->len;
		if(avail < len)
			DEBUG_ERROR("not enough room (%d bytes) on RX buffer", avail);
		memcpy(l->conn->rxbuf + (l->conn->rxsz), p->payload, len);
		l->conn->rxsz += len;
		p = p->next;
		tot += len;
	}
	if(tot) {
		//#if defined(USE_SOCKS_PROXY)
		//	l->tap->phyOnTcpWritable(l->conn->sock, NULL, true);
		//#else
			l->tap->phyOnUnixWritable(l->conn->sock, NULL, true);
		//#endif
	}
	l->tap->lwipstack->__pbuf_free(q);
	return ERR_OK;
}

err_t NetconEthernetTap::nc_sent(void* arg, struct tcp_pcb *PCB, u16_t len)
{
    DEBUG_EXTRA("pcb=%p", (void*)&PCB);
	Larg *l = (Larg*)arg;
	Mutex::Lock _l(l->tap->_tcpconns_m);
	if(l->conn->probation && l->conn->txsz == 0){
		l->conn->probation = false; // TX buffer now empty, removing from probation
	}
	if(l && l->conn && len && !l->conn->probation) {
        int softmax = l->conn->type == SOCK_STREAM ? DEFAULT_TCP_TX_BUF_SZ : DEFAULT_UDP_TX_BUF_SZ;
		if(l->conn->txsz < softmax) {
			l->tap->_phy.setNotifyReadable(l->conn->sock, true);
            l->tap->_phy.whack();
		}
	}
	return ERR_OK;
}

err_t NetconEthernetTap::nc_connected_proxy(void *arg, struct tcp_pcb *PCB, err_t err)
{
    DEBUG_INFO("pcb=%p", (void*)&PCB);
    return ERR_OK;
}
    
err_t NetconEthernetTap::nc_connected(void *arg, struct tcp_pcb *PCB, err_t err)
{
    DEBUG_ATTN("pcb=%p", (void*)&PCB);
	Larg *l = (Larg*)arg;
	if(l && l->conn)
		l->tap->sendReturnValue(l->tap->_phy.getDescriptor(l->conn->rpcSock), ERR_OK);
	return ERR_OK;
}

err_t NetconEthernetTap::nc_poll(void* arg, struct tcp_pcb *PCB)
{
	return ERR_OK;
}

void NetconEthernetTap::nc_err(void *arg, err_t err)
{
	DEBUG_ERROR("err=%d", err);
	Larg *l = (Larg*)arg;
	Mutex::Lock _l(l->tap->_tcpconns_m);

	if(!l->conn)
		DEBUG_ERROR("conn==NULL");
	int fd = l->tap->_phy.getDescriptor(l->conn->sock);

	switch(err)
	{
		case ERR_MEM:
		  DEBUG_ERROR("ERR_MEM->ENOMEM");
			l->tap->sendReturnValue(fd, -1, ENOMEM);
			break;
		case ERR_BUF:
			DEBUG_ERROR("ERR_BUF->ENOBUFS");
			l->tap->sendReturnValue(fd, -1, ENOBUFS);
			break;
		case ERR_TIMEOUT:
			DEBUG_ERROR("ERR_TIMEOUT->ETIMEDOUT");
			l->tap->sendReturnValue(fd, -1, ETIMEDOUT);
			break;
		case ERR_RTE:
			DEBUG_ERROR("ERR_RTE->ENETUNREACH");
			l->tap->sendReturnValue(fd, -1, ENETUNREACH);
			break;
		case ERR_INPROGRESS:
			DEBUG_ERROR("ERR_INPROGRESS->EINPROGRESS");
			l->tap->sendReturnValue(fd, -1, EINPROGRESS);
			break;
		case ERR_VAL:
			DEBUG_ERROR("ERR_VAL->EINVAL");
			l->tap->sendReturnValue(fd, -1, EINVAL);
			break;
		case ERR_WOULDBLOCK:
			DEBUG_ERROR("ERR_WOULDBLOCK->EWOULDBLOCK");
			l->tap->sendReturnValue(fd, -1, EWOULDBLOCK);
			break;
		case ERR_USE:
			DEBUG_ERROR("ERR_USE->EADDRINUSE");
			l->tap->sendReturnValue(fd, -1, EADDRINUSE);
			break;
		case ERR_ISCONN:
			DEBUG_ERROR("ERR_ISCONN->EISCONN");
			l->tap->sendReturnValue(fd, -1, EISCONN);
			break;
		case ERR_ABRT:
			DEBUG_ERROR("ERR_ABRT->ECONNREFUSED");
			l->tap->sendReturnValue(fd, -1, ECONNREFUSED);
			break;

			// TODO: Below are errors which don't have a standard errno correlate

		case ERR_RST:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_CLSD:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_CONN:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_ARG:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		case ERR_IF:
			l->tap->sendReturnValue(fd, -1, -1);
			break;
		default:
			break;
	}
	DEBUG_ERROR(" closing connection");
	l->tap->closeConnection(l->conn);
}

#endif // SDK_LWIP

/*------------------------------------------------------------------------------
----------------------------- RPC Handler functions ----------------------------
------------------------------------------------------------------------------*/

void NetconEthernetTap::handleGetsockname(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct getsockname_st *getsockname_rpc)
{
	Mutex::Lock _l(_tcpconns_m);
	Connection *conn = getConnection(sock);
	if(conn->local_addr == NULL){
		DEBUG_EXTRA("no address info available. is it bound?");
		struct sockaddr_storage storage;
		memset(&storage, 0, sizeof(struct sockaddr_storage));
		write(_phy.getDescriptor(rpcSock), NULL, sizeof(struct sockaddr_storage));
		return;
	}
	write(_phy.getDescriptor(rpcSock), conn->local_addr, sizeof(struct sockaddr_storage));
}

void NetconEthernetTap::handleGetpeername(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct getsockname_st *getsockname_rpc)
{
	Mutex::Lock _l(_tcpconns_m);
	Connection *conn = getConnection(sock);
	if(conn->peer_addr == NULL){
		DEBUG_EXTRA("no peer address info available. is it connected?");
		struct sockaddr_storage storage;
		memset(&storage, 0, sizeof(struct sockaddr_storage));
		write(_phy.getDescriptor(rpcSock), NULL, sizeof(struct sockaddr_storage));
		return;
	}
	write(_phy.getDescriptor(rpcSock), conn->peer_addr, sizeof(struct sockaddr_storage));
}

void NetconEthernetTap::handleBind(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct bind_st *bind_rpc)
{
	Mutex::Lock _l(_tcpconns_m);

	// picoTCP
	#if defined(SDK_PICOTCP)
		pico_handleBind(sock,rpcSock,uptr,bind_rpc);
	#endif

	// lwIP
	#if defined(SDK_LWIP)
		if(!_ips.size()) {
			// We haven't been given an address yet. Binding at this stage is premature
			DEBUG_ERROR("cannot bind yet. ZT address hasn't been provided");
			sendReturnValue(rpcSock, -1, ENOMEM);
			return;
		}

		ip_addr_t ba;
	    char addrstr[INET6_ADDRSTRLEN];
	    struct sockaddr_in6 *rawAddr = (struct sockaddr_in6 *) &bind_rpc->addr;
	    struct sockaddr *addr = (struct sockaddr*)rawAddr;
		int err, port = lwipstack->__lwip_ntohs(rawAddr->sin6_port);

		// ipv4
		#if defined(SDK_IPV4)
			//ip4_addr_t ba;
		    if(addr->sa_family == AF_INET) {
		        struct sockaddr_in *connaddr = (struct sockaddr_in *)addr;
		        inet_ntop(AF_INET, &(connaddr->sin_addr), addrstr, INET_ADDRSTRLEN);    
		        sprintf(addrstr, "%s:%d", addrstr, lwipstack->__lwip_ntohs(connaddr->sin_port));
		    }
		#endif

		// ipv6
		#if defined(SDK_IPV6)
		    //ip6_addr_t ba;
			struct sockaddr_in6 *in6 = (struct sockaddr_in6*)&bind_rpc->addr;
			in6_to_ip6((ip6_addr *)&ba, in6);

		    if(addr->sa_family == AF_INET6) {        
		        struct sockaddr_in6 *connaddr6 = (struct sockaddr_in6 *)addr;
		        inet_ntop(AF_INET6, &(connaddr6->sin6_addr), addrstr, INET6_ADDRSTRLEN);
		        sprintf(addrstr, "%s:%d", addrstr, lwipstack->__lwip_ntohs(connaddr6->sin6_port));
		    }
		#endif
		
		Connection *conn = getConnection(sock);
	    DEBUG_ATTN(" sock=%p, fd=%d, port=%d", (void*)&sock, bind_rpc->fd, port);
	    if(conn) {
	        if(conn->type == SOCK_DGRAM) {
	       		#if defined(__ANDROID__)
	            	err = lwipstack->__udp_bind(conn->UDP_pcb, NULL, port);
	            #else
					err = lwipstack->__udp_bind(conn->UDP_pcb, (const ip_addr_t *)&ba, port);
	            #endif
	            if(err == ERR_USE) // port in use
	                sendReturnValue(rpcSock, -1, EADDRINUSE);
	            else {
	            	lwipstack->__udp_recv(conn->UDP_pcb, nc_udp_recved, new Larg(this, conn));
	            	struct sockaddr_in addr_in;
	                memcpy(&addr_in, &bind_rpc->addr, sizeof(addr_in));
	                addr_in.sin_port = Utils::ntoh(conn->UDP_pcb->local_port); // Newly assigned port
	                memcpy(&conn->local_addr, &addr_in, sizeof(addr_in));
	  				sendReturnValue(rpcSock, ERR_OK, ERR_OK); // Success
	            }
	            return;
	        }
	        else if (conn->type == SOCK_STREAM) {
	            if(conn->TCP_pcb->state == CLOSED){
	                err = lwipstack->__tcp_bind(conn->TCP_pcb, (const ip_addr_t *)&ba, port);
	                if(err != ERR_OK) {
	                    DEBUG_ERROR("err=%d", err);
	                    if(err == ERR_USE)
	                        sendReturnValue(rpcSock, -1, EADDRINUSE);
	                    if(err == ERR_MEM)
	                        sendReturnValue(rpcSock, -1, ENOMEM);
	                    if(err == ERR_BUF)
	                        sendReturnValue(rpcSock, -1, ENOMEM);
	                } else {
	                    conn->local_addr = (struct sockaddr_storage *) &bind_rpc->addr;
	                    sendReturnValue(rpcSock, ERR_OK, ERR_OK); // Success
	                }
	            } else {
	                DEBUG_ERROR(" ignoring BIND request, PCB (conn=%p, pcb=%p) not in CLOSED state. ", 
	                	(void*)&conn, (void*)&conn->TCP_pcb);
	                sendReturnValue(rpcSock, -1, EINVAL);
	            }
	        }
		} else {
			DEBUG_ERROR(" unable to locate Connection");
			sendReturnValue(rpcSock, -1, EBADF);
		}
	#endif
}

void NetconEthernetTap::handleListen(PhySocket *sock, PhySocket *rpcSock, void **uptr, struct listen_st *listen_rpc)
{
	DEBUG_ATTN("sock=%p", (void*)&sock);
	Mutex::Lock _l(_tcpconns_m);
	Connection *conn = getConnection(sock);
    
    // picoTCP
  	#if defined(SDK_PICOTCP)
  		pico_handleListen(sock, rpcSock, uptr, listen_rpc);
  	#endif

    // lwIP
	#if defined(SDK_LWIP)
	    if(conn->type==SOCK_DGRAM) {
			// FIX: Added sendReturnValue() call to fix listen() return bug on Android
			sendReturnValue(rpcSock, ERR_OK, ERR_OK);
	        return;
		}
		if(!conn) {
			DEBUG_ERROR(" unable to locate Connection");
			sendReturnValue(rpcSock, -1, EBADF);
			return;
		}
		if(conn->TCP_pcb->state == LISTEN) {
			DEBUG_ERROR(" PCB is already in listening state");
			sendReturnValue(rpcSock, ERR_OK, ERR_OK);
			return;
		}
		struct tcp_pcb* listeningPCB;

		#ifdef TCP_LISTEN_BACKLOG
			listeningPCB = lwipstack->__tcp_listen_with_backlog(conn->TCP_pcb, listen_rpc->backlog);
		#else
			listeningPCB = lwipstack->__tcp_listen(conn->pcb);
		#endif
		if(listeningPCB != NULL) {
	    	conn->TCP_pcb = listeningPCB;
	    	lwipstack->__tcp_accept(listeningPCB, nc_accept);
			lwipstack->__tcp_arg(listeningPCB, new Larg(this, conn));
			fcntl(_phy.getDescriptor(conn->sock), F_SETFL, O_NONBLOCK);
			conn->listening = true;
			sendReturnValue(rpcSock, ERR_OK, ERR_OK);
			return;
	  	}
	  sendReturnValue(rpcSock, -1, -1);
	#endif
}
    
Connection * NetconEthernetTap::handleSocketProxy(PhySocket *sock, int socket_type)
{
	/*
    Connection *conn = getConnection(sock);
    if(!conn){
        DEBUG_ERROR("unable to locate Connection object for this PhySocket sock=%p", (void*)&sock);
        return NULL;
    }
	DEBUG_ATTN("sock=%p", (void*)&sock);
    struct udp_pcb *new_udp_PCB = NULL;
    struct tcp_pcb *new_tcp_PCB = NULL;
    if(socket_type == SOCK_DGRAM) {
        DEBUG_EXTRA("SOCK_DGRAM");
        Mutex::Lock _l(_tcpconns_m);
        new_udp_PCB = lwipstack->__udp_new();
    }
    else if(socket_type == SOCK_STREAM) {
        DEBUG_EXTRA("SOCK_STREAM");
        Mutex::Lock _l(_tcpconns_m);
        new_tcp_PCB = lwipstack->__tcp_new();
    }
    if(new_udp_PCB || new_tcp_PCB) {
        conn->sock = sock;
        conn->type = socket_type;
		conn->local_addr = NULL;
		conn->peer_addr = NULL;
        if(conn->type == SOCK_DGRAM) conn->UDP_pcb = new_udp_PCB;
        if(conn->type == SOCK_STREAM) conn->TCP_pcb = new_tcp_PCB;
        DEBUG_INFO(" updated sock=%p", (void*)&sock);
        return conn;
    }
	DEBUG_ERROR(" memory not available for new PCB");
	*/
	return NULL;
}

Connection * NetconEthernetTap::handleSocket(PhySocket *sock, void **uptr, struct socket_st* socket_rpc)
{
    DEBUG_ATTN("sock=%p, sock_type=%d", (void*)&sock, socket_rpc->socket_type);

	// picoTCP
	#if defined(SDK_PICOTCP)
		return pico_handleSocket(sock, uptr, socket_rpc);
	#endif

    // lwIP
	#if defined(SDK_LWIP) 
	    struct udp_pcb *new_udp_PCB = NULL;
	    struct tcp_pcb *new_tcp_PCB = NULL;

	    if(socket_rpc->socket_type == SOCK_DGRAM) {
	        DEBUG_EXTRA("SOCK_DGRAM");
	        Mutex::Lock _l(_tcpconns_m);
	        new_udp_PCB = lwipstack->__udp_new();
	    }
	    else if(socket_rpc->socket_type == SOCK_STREAM) {
	        DEBUG_EXTRA("SOCK_STREAM");
	        Mutex::Lock _l(_tcpconns_m);
	        new_tcp_PCB = lwipstack->__tcp_new();
	    }
	    else if(socket_rpc->socket_type == SOCK_RAW) {
	    	DEBUG_ERROR("SOCK_RAW, not currently supported.");
	    }
	    if(new_udp_PCB || new_tcp_PCB) {
	        Connection * newConn = new Connection();
	        *uptr = newConn;
	        newConn->type = socket_rpc->socket_type;
	        newConn->sock = sock;
			newConn->local_addr = NULL;
			newConn->peer_addr = NULL;
	        if(newConn->type == SOCK_DGRAM) newConn->UDP_pcb = new_udp_PCB;
	        if(newConn->type == SOCK_STREAM) newConn->TCP_pcb = new_tcp_PCB;
	        _Connections.push_back(newConn);
	        return newConn;
	    }
		DEBUG_ERROR(" memory not available for new PCB");
		sendReturnValue(_phy.getDescriptor(sock), -1, ENOMEM);
		return NULL;
	#endif
}

int NetconEthernetTap::handleConnectProxy(PhySocket *sock, struct sockaddr_in *rawAddr)
{
	/*
    DEBUG_ATTN("sock=%p", (void*)&sock);
    Mutex::Lock _l(_tcpconns_m);
    int port = rawAddr->sin_port;
	ip_addr_t connAddr = convert_ip(rawAddr);
    int err = 0;

    Connection *conn = getConnection(sock);
    if(!conn) {
    	DEBUG_INFO(" unable to locate Connection object for sock=%p", (void*)&sock);
    	return -1;
    }
    if(conn->type == SOCK_DGRAM) {
        // Generates no network traffic
        if((err = lwipstack->__udp_connect(conn->UDP_pcb,&connAddr,port)) < 0)
            DEBUG_INFO("error while connecting to with UDP (sock=%p)", (void*)&sock);
        lwipstack->__udp_recv(conn->UDP_pcb, nc_udp_recved, new Larg(this, conn));
        errno = ERR_OK;
        return 0;
    }
	if(conn != NULL) {
		lwipstack->__tcp_sent(conn->TCP_pcb, nc_sent);
		lwipstack->__tcp_recv(conn->TCP_pcb, nc_recved);
		lwipstack->__tcp_err(conn->TCP_pcb, nc_err);
		lwipstack->__tcp_poll(conn->TCP_pcb, nc_poll, APPLICATION_POLL_FREQ);
		lwipstack->__tcp_arg(conn->TCP_pcb, new Larg(this, conn));
        
		int ip = rawAddr->sin_addr.s_addr;
		unsigned char d[4];
		d[0] = ip & 0xFF;
		d[1] = (ip >>  8) & 0xFF;
		d[2] = (ip >> 16) & 0xFF;
		d[3] = (ip >> 24) & 0xFF;
		DEBUG_INFO(" addr=%d.%d.%d.%d:%d", d[0],d[1],d[2],d[3], port);	
		DEBUG_INFO(" pcb->state=%x", conn->TCP_pcb->state);
		if(conn->TCP_pcb->state != CLOSED) {
			DEBUG_INFO(" cannot connect using this PCB, PCB!=CLOSED");
			errno = EAGAIN;
			return -1;
		}
		if((err = lwipstack->__tcp_connect(conn->TCP_pcb,&connAddr,port,nc_connected_proxy)) < 0)
		{
			if(err == ERR_ISCONN) {
				errno = EISCONN; // Already in connected state
				return -1;
			} if(err == ERR_USE) {
				errno = EADDRINUSE; // Already in use
				return -1;
			} if(err == ERR_VAL) {
				errno = EINVAL; // Invalid ipaddress parameter
				return -1;
			} if(err == ERR_RTE) {
				errno = ENETUNREACH; // No route to host
				return -1;
			} if(err == ERR_BUF) {
				errno = EAGAIN; // No more ports available
				return -1;
			}
			if(err == ERR_MEM) {
				// Can occur for the following reasons: tcp_enqueue_flags()

				// 1) tcp_enqueue_flags is always called with either SYN or FIN in flags.
				// We need one available snd_buf byte to do that.
				// This means we can't send FIN while snd_buf==0. A better fix would be to
				// not include SYN and FIN sequence numbers in the snd_buf count.

				// 2) Cannot allocate new pbuf
				// 3) Cannot allocate new TCP segment

				errno = EAGAIN; // TODO: Doesn't describe the problem well, but closest match
				return -1;
			}
			// We should only return a value if failure happens immediately
			// Otherwise, we still need to wait for a callback from lwIP.
			// - This is because an ERR_OK from tcp_connect() only verifies
			//   that the SYN packet was enqueued onto the stack properly,
			//   that's it!
			// - Most instances of a retval for a connect() should happen
			//   in the nc_connect() and nc_err() callbacks!
			DEBUG_ERROR(" unable to connect");
			errno = EAGAIN;
			return -1;
		}
		// Everything seems to be ok, but we don't have enough info to retval
		conn->listening=true;
        return 0;
	} else {
		DEBUG_ERROR(" could not locate PCB based on application-provided fd");
		errno = EBADF;
		return -1;
	}
	*/
    return -1;
}

void NetconEthernetTap::handleConnect(PhySocket *sock, PhySocket *rpcSock, Connection *conn, struct connect_st* connect_rpc)
{
    DEBUG_ATTN("sock=%p", (void*)&sock);
	Mutex::Lock _l(_tcpconns_m);

	// picoTCP
	#if defined(SDK_PICOTCP)
		pico_handleConnect(sock, rpcSock, conn, connect_rpc);		
	#endif

	// lwIP
    #if defined(SDK_LWIP)
    	struct sockaddr_in *rawAddr = (struct sockaddr_in *) &connect_rpc->addr;
		int port = lwipstack->__lwip_ntohs(rawAddr->sin_port);
		ip_addr_t connAddr = convert_ip(rawAddr);    
		int err = 0, ip = rawAddr->sin_addr.s_addr;

	    char addrstr[INET6_ADDRSTRLEN];
	    struct sockaddr *addr = (struct sockaddr*)rawAddr;
	    if(addr->sa_family == AF_INET) {
	        struct sockaddr_in *connaddr = (struct sockaddr_in *)addr;
	        inet_ntop(AF_INET, &(connaddr->sin_addr), addrstr, INET_ADDRSTRLEN);    
	        sprintf(addrstr, "%s:%d", addrstr, lwipstack->__lwip_ntohs(connaddr->sin_port));
	    }
	    if(addr->sa_family == AF_INET6) {        
	        struct sockaddr_in6 *connaddr6 = (struct sockaddr_in6 *)addr;
	        inet_ntop(AF_INET6, &(connaddr6->sin6_addr), addrstr, INET6_ADDRSTRLEN);
	        sprintf(addrstr, "%s:%d", addrstr, lwipstack->__lwip_ntohs(connaddr6->sin6_port));
	    }
	    DEBUG_INFO("addr=%s", addrstr);

	    if(conn->type == SOCK_DGRAM) {
	        // Generates no network traffic
	        if((err = lwipstack->__udp_connect(conn->UDP_pcb,&connAddr,port)) < 0)
	            DEBUG_ERROR("error while connecting to with UDP");
	        lwipstack->__udp_recv(conn->UDP_pcb, nc_udp_recved, new Larg(this, conn));
	        sendReturnValue(rpcSock, 0, ERR_OK);
	        return;
	    }
		if(conn != NULL) {
			lwipstack->__tcp_sent(conn->TCP_pcb, nc_sent);
			lwipstack->__tcp_recv(conn->TCP_pcb, nc_recved);
			lwipstack->__tcp_err(conn->TCP_pcb, nc_err);
			lwipstack->__tcp_poll(conn->TCP_pcb, nc_poll, APPLICATION_POLL_FREQ);
			lwipstack->__tcp_arg(conn->TCP_pcb, new Larg(this, conn));
	        	
			DEBUG_EXTRA(" pcb->state=%x", conn->TCP_pcb->state);
			if(conn->TCP_pcb->state != CLOSED) {
				DEBUG_INFO(" cannot connect using this PCB, PCB!=CLOSED");
				sendReturnValue(rpcSock, -1, EAGAIN);
				return;
			}

			static ip_addr_t ba;

			IP6_ADDR2(&ba, 0xfd56,0x5799,0xd8f6,0x1238,0x8c99,0x9322,0x30ce,0x418a);


			if((err = lwipstack->__tcp_connect(conn->TCP_pcb,&ba,port,nc_connected)) < 0)
			{
				if(err == ERR_ISCONN) {
					sendReturnValue(rpcSock, -1, EISCONN); // Already in connected state
					return;
				} if(err == ERR_USE) {
					sendReturnValue(rpcSock, -1, EADDRINUSE); // Already in use
					return;
				} if(err == ERR_VAL) {
					sendReturnValue(rpcSock, -1, EINVAL); // Invalid ipaddress parameter
					return;
				} if(err == ERR_RTE) {
					sendReturnValue(rpcSock, -1, ENETUNREACH); // No route to host
					return;
				} if(err == ERR_BUF) {
					sendReturnValue(rpcSock, -1, EAGAIN); // No more ports available
					return;
				}
				if(err == ERR_MEM) {
					sendReturnValue(rpcSock, -1, EAGAIN); // TODO: Doesn't describe the problem well, but closest match
					return;
				}

				// We should only return a value if failure happens immediately
				// Otherwise, we still need to wait for a callback from lwIP.
				// - This is because an ERR_OK from tcp_connect() only verifies
				//   that the SYN packet was enqueued onto the stack properly,
				//   that's it!
				// - Most instances of a retval for a connect() should happen
				//   in the nc_connect() and nc_err() callbacks!
				DEBUG_ERROR(" unable to connect");
				sendReturnValue(rpcSock, -1, EAGAIN);
			}
			// Everything seems to be ok, but we don't have enough info to retval
			conn->listening=true;
			conn->rpcSock=rpcSock; // used for return value from lwip CB
		} 
		else {
			DEBUG_ERROR(" could not locate PCB based on application-provided fd");
			sendReturnValue(rpcSock, -1, EBADF);
		}
	#endif
}

void NetconEthernetTap::handleWrite(Connection *conn)
{
    DEBUG_EXTRA("conn=%p", (void*)&conn);
	
	// picoTCP
    #if defined(SDK_PICOTCP)
	    pico_handleWrite(conn);
    #endif

	// lwIP
    #if defined(SDK_LWIP)
		if(!conn || (!conn->TCP_pcb && !conn->UDP_pcb)) {
			DEBUG_ERROR(" invalid connection");
			return;
		}
	    if(conn->type == SOCK_DGRAM) {
	        if(!conn->UDP_pcb) {
	            DEBUG_ERROR(" invalid UDP_pcb, type=SOCK_DGRAM");
	            return;
	        }
	        // TODO: Packet re-assembly hasn't yet been tested with lwIP so UDP packets are limited to MTU-sized chunks
	        int udp_trans_len = conn->txsz < ZT_UDP_DEFAULT_PAYLOAD_MTU ? conn->txsz : ZT_UDP_DEFAULT_PAYLOAD_MTU;
	        
	        DEBUG_EXTRA(" allocating pbuf chain of size=%d for UDP packet, txsz=%d", udp_trans_len, conn->txsz);
	        struct pbuf * pb = lwipstack->__pbuf_alloc(PBUF_TRANSPORT, udp_trans_len, PBUF_POOL);
	        if(!pb){
	            DEBUG_ERROR(" unable to allocate new pbuf of size=%d", conn->txsz);
	            return;
	        }
	        memcpy(pb->payload, conn->txbuf, udp_trans_len);
	        int err = lwipstack->__udp_send(conn->UDP_pcb, pb);
	        
	        if(err == ERR_MEM) {
	            DEBUG_ERROR(" error sending packet. out of memory");
	        } else if(err == ERR_RTE) {
	            DEBUG_ERROR(" could not find route to destinations address");
	        } else if(err != ERR_OK) {
	            DEBUG_ERROR(" error sending packet - %d", err);
	        } else {
				// Success
	            int buf_remaining = (conn->txsz)-udp_trans_len;
	            if(buf_remaining)
	                memmove(&conn->txbuf, (conn->txbuf+udp_trans_len), buf_remaining);
	            conn->txsz -= udp_trans_len;

				#if DEBUG_LEVEL >= MSG_TRANSFER
					struct sockaddr_in * addr_in2 = (struct sockaddr_in *)conn->peer_addr;
					int port = lwipstack->__lwip_ntohs(addr_in2->sin_port);
					int ip = addr_in2->sin_addr.s_addr;
					unsigned char d[4];
					d[0] = ip & 0xFF;
					d[1] = (ip >>  8) & 0xFF;
					d[2] = (ip >> 16) & 0xFF;
					d[3] = (ip >> 24) & 0xFF;
					DEBUG_TRANS("[UDP TX] --->    :: {TX: ------, RX: ------, sock=%p} :: %d bytes (dest_addr=%d.%d.%d.%d:%d)", 
						(void*)conn->sock, udp_trans_len, d[0], d[1], d[2], d[3], port);
				#endif
	        }
	        lwipstack->__pbuf_free(pb);
	        return;
	    }
	    else if(conn->type == SOCK_STREAM) {
	        if(!conn->TCP_pcb) {
	            DEBUG_ERROR(" invalid TCP_pcb, type=SOCK_STREAM");
	            return;
	        }
	        // How much we are currently allowed to write to the connection
	        int sndbuf = conn->TCP_pcb->snd_buf;
	        int err, sz, r;
	    
	        if(!sndbuf) {
	            // PCB send buffer is full, turn off readability notifications for the
	            // corresponding PhySocket until nc_sent() is called and confirms that there is
	            // now space on the buffer
	            if(!conn->probation) {
	                DEBUG_ERROR(" LWIP stack is full, sndbuf == 0");
	                _phy.setNotifyReadable(conn->sock, false);
	                conn->probation = true;
	            }
	            return;
	        }
	        if(conn->txsz <= 0)
	            return; // Nothing to write
	        if(!conn->listening)
	            lwipstack->__tcp_output(conn->TCP_pcb);

	        if(conn->sock) {
	            r = conn->txsz < sndbuf ? conn->txsz : sndbuf;
	            // Writes data pulled from the client's socket buffer to LWIP. This merely sends the
	            // data to LWIP to be enqueued and eventually sent to the network.
	            if(r > 0) {
	                err = lwipstack->__tcp_write(conn->TCP_pcb, &conn->txbuf, r, TCP_WRITE_FLAG_COPY);
	                lwipstack->__tcp_output(conn->TCP_pcb);
	                if(err != ERR_OK) {
	                    DEBUG_ERROR(" error while writing to PCB, err=%d", err);
	                    if(err == -1)
	                        DEBUG_ERROR("out of memory");
	                    return;
	                } else {
	                	// adjust buffer
	                    sz = (conn->txsz)-r;
	                    if(sz)
	                        memmove(&conn->txbuf, (conn->txbuf+r), sz);
	                    conn->txsz -= r;
	                    int max = conn->type == SOCK_STREAM ? DEFAULT_TCP_TX_BUF_SZ : DEFAULT_UDP_TX_BUF_SZ;
	                    DEBUG_TRANS("[TCP TX] --->    :: {TX: %.3f%%, RX: %.3f%%, sock=%p} :: %d bytes",
	                        (float)conn->txsz / (float)max, (float)conn->rxsz / max, (void*)&conn->sock, r);
	                    return;
	                }
	            }
	        }
	    }
	#endif
}

} // namespace ZeroTier

