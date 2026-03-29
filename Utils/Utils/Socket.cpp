#include <winsock2.h>
#include <ws2tcpip.h>
#include "Socket.h"
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <stdint.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
static WSADATA wsaData = { 0 };
TCPSocket::TCPSocket() :Handle(NULL) {
	if (wsaData.wVersion == 0) {
		int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != NO_ERROR) {
			std::ostringstream oss;
			oss << "WSAStartup failed with error: " << result;
			throw std::runtime_error(oss.str());
		}
	}
	Handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Handle == INVALID_SOCKET) {
		std::ostringstream oss;
		oss << "Socket creation failed with error: " << WSAGetLastError();
		throw std::runtime_error(oss.str());
	}
}
TCPSocket::TCPSocket(UINT_PTR h) :Handle(h) {}
TCPSocket::~TCPSocket() {
	int result = closesocket(Handle);
	if (result == SOCKET_ERROR) {
		std::cerr << "Socket closing failed with error: " << WSAGetLastError() << std::endl;
	}
}
bool TCPSocket::Connect(const char* ip, int port) {
	sockaddr_in address_;
	address_.sin_family = AF_INET;
	if (inet_pton(AF_INET, ip, &address_.sin_addr) != 1) {
		std::cerr << "Invalid IP address: " << ip << std::endl;
		return false;
	}
	address_.sin_port = htons(port);
	int result = ::connect(Handle, (sockaddr*)&address_, sizeof(address_));
	if (result == SOCKET_ERROR) {
		std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
		return false;
	}
	return true;
}
bool TCPSocket::Listen(int port, int backlog) {
	sockaddr_in address_;
	address_.sin_family = AF_INET;
	address_.sin_addr.s_addr = INADDR_ANY;
	address_.sin_port = htons(port);
	int result = bind(Handle, (sockaddr*)&address_, sizeof(address_));
	if (result == SOCKET_ERROR) {
		std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
		return false;
	}
	result = ::listen(Handle, backlog);
	if (result == SOCKET_ERROR) {
		std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
		return false;
	}
	return true;
}
TCPSocket* TCPSocket::Accept() {
	UINT_PTR client = ::accept(Handle, NULL, NULL);
	if (client == INVALID_SOCKET) {
		std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
		return nullptr;
	}
	return new TCPSocket(client);
}
int TCPSocket::Send(const char* data, int length) {
	int result = ::send(Handle, data, length, 0);
	if (result == SOCKET_ERROR) {
		std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
		return -1;
	}
	return result;
}
int TCPSocket::Receive(char* buffer, int length) {
	int result = ::recv(Handle, buffer, length, 0);
	if (result == SOCKET_ERROR) {
		std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
		return -1;
	}
	return result;
}
std::string TCPSocket::GetRemoteIP() {
	sockaddr_in addr;
	int len = sizeof(addr);
	if (getpeername(Handle, (sockaddr*)&addr, &len) != SOCKET_ERROR) {
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr, ipStr, INET_ADDRSTRLEN);
		return std::string(ipStr);
	}
	return "";
}
int TCPSocket::GetRemotePort() {
	sockaddr_in addr;
	int len = sizeof(addr);
	if (getpeername(Handle, (sockaddr*)&addr, &len) != SOCKET_ERROR) {
		return ntohs(addr.sin_port);
	}
	return -1;
}
void TCPSocket::Close() {
	int result = closesocket(Handle);
	if (result == SOCKET_ERROR) {
		std::cerr << "Socket closing failed with error: " << WSAGetLastError() << std::endl;
	}
}
bool TCPSocket::IsConnected() {
	fd_set writeSet;
	FD_ZERO(&writeSet);
	FD_SET(Handle, &writeSet);

	timeval timeout = { 0, 0 };

	if (select(0, NULL, &writeSet, NULL, &timeout) == SOCKET_ERROR) {
		std::cerr << "Select failed with error: " << WSAGetLastError() << std::endl;
		return false;
	}

	if (FD_ISSET(Handle, &writeSet)) {
		return true;
	}

	return false;
}


UDPSocket::UDPSocket() : Handle(INVALID_SOCKET) {
	if (wsaData.wVersion == 0) {
		int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != NO_ERROR) {
			throw std::runtime_error("WSAStartup failed");
		}
	}

	Handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (Handle == INVALID_SOCKET) {
		throw std::runtime_error("Socket creation failed");
	}
}

UDPSocket::~UDPSocket() {
	Close();
}

bool UDPSocket::Bind(int port) {
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	int result = ::bind(Handle, (sockaddr*)&address, sizeof(address));
	if (result == SOCKET_ERROR) {
		std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
		return false;
	}
	return true;
}

void UDPSocket::Unbind() {
	::bind(Handle, nullptr, 0);
}
int UDPSocket::SendTo(const char* data, int length, const char* ip, int port) {
	sockaddr_in address;
	address.sin_family = AF_INET;
	if (inet_pton(AF_INET, ip, &address.sin_addr) != 1) {
		std::cerr << "Invalid IP address: " << ip << std::endl;
		return -1;
	}
	address.sin_port = htons(port);

	int result = ::sendto(Handle, data, length, 0, (sockaddr*)&address, sizeof(address));
	if (result == SOCKET_ERROR) {
		std::cerr << "SendTo failed with error: " << WSAGetLastError() << std::endl;
		return -1;
	}
	return result;
}

int UDPSocket::ReceiveFrom(char* buffer, int length, std::string& fromIP, int& fromPort) {
	sockaddr_in fromAddr;
	int fromAddrLen = sizeof(fromAddr);

	int result = ::recvfrom(Handle, buffer, length, 0, (sockaddr*)&fromAddr, &fromAddrLen);
	if (result == SOCKET_ERROR) {
		std::cerr << "ReceiveFrom failed with error: " << WSAGetLastError() << std::endl;
		return -1;
	}

	char ipStr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &fromAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
	fromIP = std::string(ipStr);
	fromPort = ntohs(fromAddr.sin_port);
	return result;
}

void UDPSocket::Close() {
	if (Handle != INVALID_SOCKET) {
		closesocket(Handle);
		Handle = INVALID_SOCKET;
	}
}