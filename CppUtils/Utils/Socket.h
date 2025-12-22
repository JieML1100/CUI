#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include "defines.h"
#include <iostream>

#pragma comment(lib, "ws2_32.lib")
class TCPSocket {
public:
	UINT_PTR Handle;
	TCPSocket();
	TCPSocket(UINT_PTR h);
	~TCPSocket();
	bool Connect(const char* ip, int port);
	bool Listen(int port, int backlog = SOMAXCONN);
	TCPSocket* Accept();
	int Send(const char* data, int length);
	int Receive(char* buffer, int length);
	std::string GetRemoteIP();
	int GetRemotePort();
	void Close();
	bool IsConnected();
};
class UDPSocket {
public:
	UINT_PTR Handle;

	UDPSocket();
	~UDPSocket();
	bool Bind(int port);
	void Unbind();
	int SendTo(const char* data, int length, const char* ip, int port);
	int ReceiveFrom(char* buffer, int length, std::string& fromIP, int& fromPort);
	void Close();
};