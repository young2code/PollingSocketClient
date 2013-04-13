#pragma once

#include <winsock2.h>
#include <mswsock.h>
#include <Ws2tcpip.h>
#include <string>

struct addrinfo;

namespace Network
{
	bool Init();
	void Shutdown();

	SOCKET CreateSocket(bool bind = true, u_short port = 0, int aiFamily = AF_INET);
	void CloseSocket(SOCKET socket);

	BOOL AcceptEx(SOCKET listenSocket, SOCKET newSocket, LPOVERLAPPED overlapped);
	BOOL ConnectEx(SOCKET socket, sockaddr* addr, int addrlen, LPOVERLAPPED overlapped);

	bool GetLocalAddress(SOCKET socket, std::string& ip, u_short& port);
	bool GetRemoteAddress(SOCKET socket, std::string& ip, u_short& port);
};
