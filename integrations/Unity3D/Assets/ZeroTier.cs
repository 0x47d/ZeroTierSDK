﻿/*
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

using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Networking;

using System;
using System.Collections;
using System.Runtime.InteropServices;
using System.Threading;
using System.Net.Sockets;
using System.Net;
using System.IO;
using C5;

public class ZeroTierNetworkInterface {

	// Apple
	#if UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
	const string DLL_PATH = "ZeroTierSDK_Unity3D_OSX";
	#endif

	#if UNITY_IOS || UNITY_IPHONE
	const string DLL_PATH = "ZeroTierSDK_Unity3D_iOS";
	#endif

	// Windows
	#if UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
	const string DLL_PATH = "ZeroTierSDK_Unity3D_WIN";
	#endif

	// Linux
	#if UNITY_STANDALONE_LINUX
	const string DLL_PATH = "ZeroTierSDK_Unity3D_LINUX";
	#endif

	// Android
	#if UNITY_ANDROID
	const string DLL_PATH = "ZeroTierSDK_Unity3D_ANDROID";
	#endif 

	// Interop structures
	[System.Runtime.InteropServices.StructLayoutAttribute(System.Runtime.InteropServices.LayoutKind.Sequential, CharSet=System.Runtime.InteropServices.CharSet.Ansi)]
	public struct sockaddr {
		/// u_short->unsigned short
		public ushort sa_family;
		/// char[14]
		[System.Runtime.InteropServices.MarshalAsAttribute(System.Runtime.InteropServices.UnmanagedType.ByValTStr, SizeConst=14)]
		public string sa_data;
	}

	// ZeroTier background thread
	private Thread ztThread;

	private ArrayList<int> connections = new ArrayList<int> ();

	// Virtual network interace config
	private int MaxPacketSize;
	private string rpc_path = "/does/this/work";

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void MyDelegate(string str);

	static void CallBackFunction(string str) {
		Debug.Log("Native ZT Plugin: " + str);
	}

#region
	// ZeroTier service / debug initialization
	[DllImport (DLL_PATH)]
	public static extern void SetDebugFunction( IntPtr fp );
	[DllImport (DLL_PATH)]
	private static extern int unity_start_service(string path);

	// Connection calls
	[DllImport (DLL_PATH)]
	private static extern int zt_socket(int family, int type, int protocol);

	[DllImport (DLL_PATH)]
	unsafe private static extern int zt_bind(int sockfd, System.IntPtr addr, int addrlen);
	[DllImport (DLL_PATH)]
	unsafe private static extern int zt_connect(int sockfd, System.IntPtr addr, int addrlen);

	[DllImport (DLL_PATH)]
	private static extern int zt_accept(int sockfd);
	[DllImport (DLL_PATH)]
	private static extern int zt_listen(int sockfd, int backlog);
	[DllImport (DLL_PATH)]
	private static extern int zt_close(int sockfd);

	// RX / TX
	[DllImport (DLL_PATH)]
	unsafe private static extern int zt_recv(int sockfd, string buf, int len);
	[DllImport (DLL_PATH)]
	unsafe private static extern int zt_send(int sockfd, IntPtr buf, int len);
	[DllImport (DLL_PATH)]
	unsafe private static extern int zt_set_nonblock(int sockfd);

	// ZT Thread controls
	[DllImport (DLL_PATH)]
	private static extern bool zt_is_running();
	[DllImport (DLL_PATH)]
	private static extern void zt_terminate();

	// ZT Network controls
	[DllImport (DLL_PATH)]
	private static extern bool zt_join_network(string nwid);
	[DllImport (DLL_PATH)]
	private static extern void zt_leave_network(string nwid);
#endregion

	// Thread which starts the ZeroTier service
	// The ZeroTier service may spin off a SOCKS5 proxy server 
	// if -DUSE_SOCKS_PROXY is set when building the bundle
	private void zt_service_thread()
	{
		MyDelegate callback_delegate = new MyDelegate( CallBackFunction );
		// Convert callback_delegate into a function pointer that can be
		// used in unmanaged code.
		IntPtr intptr_delegate = 
			Marshal.GetFunctionPointerForDelegate(callback_delegate);
		// Call the API passing along the function pointer.
		SetDebugFunction( intptr_delegate );
		Debug.Log ("rpc_path = " + rpc_path);
		unity_start_service (rpc_path);
	}

	// Start the ZeroTier service
	private void Init()
	{
		// TODO: Handle exceptions from unmanaged code
		ztThread = new Thread(() => {
			try {
				zt_service_thread();
			} catch(Exception e) {
				Debug.Log(e.Message.ToString());
			}
		});
		ztThread.IsBackground = true; // Allow the thread to be aborted safely
		ztThread.Start();
	}

	// Initialize the ZeroTier service with a given path
	public ZeroTierNetworkInterface(string path)
	{
		rpc_path = path;
		Init();
	}

	// Initialize the ZeroTier service
	public ZeroTierNetworkInterface()
	{
		Init();
	}

	// Initialize the ZeroTier service
	// Use the GlobalConfig to set things like the max packet size
	public ZeroTierNetworkInterface(GlobalConfig gConfig)
	{
		MaxPacketSize = gConfig.MaxPacketSize; // TODO: Do something with this!
		Init();
	}

	// Joins a ZeroTier virtual network
	public void JoinNetwork(string nwid)
	{
		zt_join_network(nwid);
	}

	// Leaves a ZeroTier virtual network
	public void LeaveNetwork(string nwid)
	{
		zt_leave_network(nwid);
	}

	// Creates a ZeroTier Socket and binds on the port provided
	// A client instance can now connect to this "host"
	public int AddHost(int port)
	{
		int sockfd = zt_socket ((int)AddressFamily.InterNetwork, (int)SocketType.Stream, (int)ProtocolType.Unspecified);
		if (sockfd < 0) {
			return -1;
		}
		GCHandle sockaddr_ptr = ZeroTierUtils.Generate_unmananged_sockaddr("0.0.0.0" + ":" + port);
		IntPtr pSockAddr = sockaddr_ptr.AddrOfPinnedObject ();
		int addrlen = Marshal.SizeOf (pSockAddr);

		// Set socket to non-blocking for RX polling in Receive()
		zt_set_nonblock(sockfd);

		return zt_bind (sockfd, pSockAddr, addrlen);
	}

	// hostId - host socket ID for this connection
	public int Connect(int hostId, string address, int port, out byte error)
	{
		int sockfd = zt_socket ((int)AddressFamily.InterNetwork, (int)SocketType.Stream, (int)ProtocolType.Unspecified);
		Debug.Log ("sockfd = " + sockfd);

		if (sockfd < 0) {
			error = (byte)sockfd;
			return -1;
		}
		GCHandle sockaddr_ptr = ZeroTierUtils.Generate_unmananged_sockaddr(address + ":" + port);
		IntPtr pSockAddr = sockaddr_ptr.AddrOfPinnedObject ();
		int addrlen = Marshal.SizeOf (pSockAddr);
		error = (byte)zt_connect (sockfd, pSockAddr, addrlen);

		// Set socket to non-blocking for RX polling in Receive()
		zt_set_nonblock(sockfd);

		return sockfd;
	}

	// Returns whether the ZeroTier service is currently running
	public bool IsRunning()
	{
		return zt_is_running ();
	}

	// Terminates the ZeroTier service 
	public void Terminate()
	{
		zt_terminate ();
	}

	// Shutdown a given connection
	public int Disconnect(int fd)
	{
		return zt_close (fd);
	}
		
	// Write data to a ZeroTier socket
	/*
	public int Send(int fd, char[] buf, int len, out byte error)
	{
		GCHandle buf_handle = GCHandle.Alloc (buf, GCHandleType.Pinned);
		IntPtr pBufPtr = buf_handle.AddrOfPinnedObject ();

		error = 0;
		int bytes_written;
		string str = new string (buf);
		if((bytes_written = zt_send (fd, str, len)) < 0) {
			error = (byte)bytes_written;
		}
		return bytes_written;
	}
	*/

	// Write data to a ZeroTier socket
	public int Send(int fd, char[] buf, int len, out byte error)
	{
		GCHandle handle = GCHandle.Alloc(buf, GCHandleType.Pinned);
		IntPtr ptr = handle.AddrOfPinnedObject();
		error = 0;
		int bytes_written;
		// FIXME: Sending a length of 2X the buffer size seems to fix the object pinning issue
		if((bytes_written = zt_send(fd, ptr, len*2)) < 0) {
			error = (byte)bytes_written;
		}
		return bytes_written;
	}

	// Checks for data to RX
	/*
	public enum NetworkEventType
	{
		DataEvent,
		ConnectEvent,
		DisconnectEvent,
		Nothing,
		BroadcastEvent
	}
	*/
	public NetworkEventType Receive(out int hostId, out int connectionId, out int channelId, byte[] buffer, int bufferSize, out int receivedSize, out byte error)
	{
		/*
		 *  If recBuffer is big enough to contain data, data will be copied in the buffer. 
		 *  If not, error will contain MessageToLong error and you will need reallocate 
		 *  buffer and call this function again. */



		for (int i = 0; i < connections.Count; i++) {
		
		}
			
		int res;
		res = zt_recv (connectionId, buffer, bufferSize);

		// FIXME: Not quite semantically the same, but close enough for alpha release?
		// FIXME: Get notifications of disconnect events?
		if (res == -1) {
			error = -1;
			return NetworkEventType.DisconnectEvent; 
		}
		if(res == 0) {
			error = 0;
			return NetworkEventType.Nothing; // No data read
		}
		if (res > 0) {
			receivedSize = res;
			Marshal.Copy(buffer, buffer, 0, res);
			return NetworkEventType.DataEvent; // Data read into buffer
		}
	}
}
