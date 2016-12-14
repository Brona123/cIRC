#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <process.h>

#include "irc.h"

SOCKET connectSocket = INVALID_SOCKET;
bool listeningSocket = true;
HANDLE threadHandle;

// The function to be called with the data from the listener socket
void (*_callback)(char *response);

// TODO move this function into its own header?
static void append(const char *s1, const char *s2, char *resbuf) {
	size_t s1len = strlen(s1);
	for (int i = 0; i < s1len; ++i) {
		resbuf[i] = s1[i];
	}

	for (int i = 0; i < strlen(s2); ++i) {
		resbuf[s1len + i] = s2[i];
	}
}

unsigned int _stdcall socketListener(void* data) {
	char recvbuf[512];
	char current = 0;
	int iResult;

	do {
		ZeroMemory(recvbuf, sizeof(recvbuf));
		// TODO reading 1 byte at a time is really inefficient
		iResult = recv(connectSocket, &current, 1, 0);
		
		if (iResult <= 0) break;

		int i = 0;

		while (current != '\n') {
			recvbuf[i++] = current;
			iResult = recv(connectSocket, &current, 1, 0);
		}
		recvbuf[i] = '\n';
		_callback(recvbuf);
	} while (iResult > 0 && listeningSocket);

	_callback("Socket listening stopped: \n");

	return 0;
}

// connData contains server and port in the form of <server>:<port>
bool irc_connect(char *connData, void (*callback)(char *response)) {
	_callback = callback;
	WSADATA wsa;
	const int fbsz = 512;
	char formattedBuf[fbsz];

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		return false;
	}

	struct addrinfo *result = NULL,
					*ptr = NULL,
					hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char *server = strtok(connData, ":");
	char *port = strtok(NULL, ":");

	int iResult = getaddrinfo(server, port, &hints, &result);
	if (iResult != 0) {
		WSACleanup();
		return false;
	}

	ptr = result;

	connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
	if (connectSocket == INVALID_SOCKET) {
		freeaddrinfo(result);
		WSACleanup();
		return false;
	}

	// Connect to server.
	iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(connectSocket);
		connectSocket = INVALID_SOCKET;
	}

	freeaddrinfo(result);
	if (connectSocket == INVALID_SOCKET) {
		WSACleanup();
		return false;
	}

	char *sendbuf[2] = { {"NICK kekbot\r\n"}, {"USER kekbot 0 * :kekbotti\r\n"} };

	for (int i = 0; i < 2; ++i) {
		iResult = send(connectSocket, sendbuf[i], (int)strlen(sendbuf[i]), 0);

		if (iResult == SOCKET_ERROR) {
			closesocket(connectSocket);
			WSACleanup();
			return false;
		}
	}

	threadHandle = (HANDLE)_beginthreadex(0, 0, socketListener, 0, 0, 0);

	return true;
}

bool irc_sendText(const char *text) {
	int iResult = send(connectSocket, text, strlen(text), 0);

	if (iResult == SOCKET_ERROR) {
		closesocket(connectSocket);
		WSACleanup();
		return false;
	}

	return true;
}

void irc_terminateConnection() {
	if (!listeningSocket) return;

	listeningSocket = false;
	CloseHandle(threadHandle);
	int iResult = shutdown(connectSocket, SD_BOTH);
	closesocket(connectSocket);
	WSACleanup();
}