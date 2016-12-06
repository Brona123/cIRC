#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>
#include <process.h>
#include <CommCtrl.h>

#include "irc.h"
#include "custom_msg.h"

#define MOTD "372"
#define CHANNELINFO "322"
#define NAMES "353"

SOCKET connectSocket = INVALID_SOCKET;
bool listeningSocket = true;
HANDLE threadHandle;
UI localUI;

// Viestin lähetys
// PRIVMSG <nimi> :<teksti>
// Multiple
// PRIVMSG <nimi>,<nimi2> :<teksti>
// Channel
// PRIVMSG #<channelname> :<teksti>
// Mode
// MODE #<channelname> +o <nick>
// Name list from channel
// :dreamhack.se.quakenet.org 353 kekbot = #kekbottestchannel :kekbot @keijo_
// disconnect
// QUIT <reason>

//:dreamhack.se.quakenet.org 353 kekbot = #day9tv :kekbot Tuukster Ex0dus_RT Sarah|Imm Jattenalle leen nibblr Hiitsnick Keytar Porta AimHere Horrible Hexamethonium ew_ @FearBot namad7 Russeli HUSTLE @MarkyO @DayBot @miniBot Skrotiss tikax spinkler gamefreaktegel [Jako] CockRoach|42 @Ravager @Q Art tliff Azulez norec
//:dreamhack.se.quakenet.org 366 kekbot #day9tv :End of /NAMES list.

static void appendText(HWND textField, const char *text) {
	int left, right;
	int len = GetWindowTextLength(textField);
	SendMessageA(textField, EM_GETSEL, (WPARAM)&left, (LPARAM)&right);
	SendMessageA(textField, EM_SETSEL, len, len);
	SendMessageA(textField, EM_REPLACESEL, 0, (LPARAM)text);
	SendMessageA(textField, EM_SETSEL, left, right);
}

static void addTab(HWND tabCtrl, const char *tabName) {
	static TCITEM t;
	t.mask = TCIF_TEXT;
	t.pszText = (LPSTR)tabName;
	t.cchTextMax = strlen(tabName);

	int tabCount = SendMessage(tabCtrl, TCM_GETITEMCOUNT, 0, 0);
	PostMessage(tabCtrl, TCM_INSERTITEM, tabCount, (LPARAM)(&t));
	PostMessage(tabCtrl, TCM_SETCURSEL, tabCount, 0);
}

static void handleForeignJoin(HWND tabCtrl, const char *channelName, const char *userName) {
	appendText(localUI.textField, "<<Foreign Join>>");
	appendText(localUI.textField, channelName);
	appendText(localUI.textField, "--");
	appendText(localUI.textField, userName);
	appendText(localUI.textField, "\r\n");

	SendMessage(tabCtrl, FOREIGN_JOIN, (WPARAM)channelName, (LPARAM)userName);
}

unsigned int _stdcall socketListener(void* data) {
	char recvbuf[512];
	char current = 0;
	int iResult;
	UI *ui = (UI*)data;

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

		OutputDebugStringA(recvbuf);

		char tmpbuf[512];
		strcpy(tmpbuf, recvbuf);

		i = 0;

		// Handle first argument
		char *word = strtok(tmpbuf, " ");

		if (strcmp(word, "PING") == 0) {
			word = strtok(NULL, " ");

			std::string s1("PONG ");
			std::string s2(word);
			irc_sendText((s1 + s2).c_str(), (*ui));
			appendText(ui->textField, "<<Responded to PING>>\n");
		} else if (strcmp(word, "NOTICE") == 0) {
			word = strtok(NULL, " ");

			std::string s1("<<Notify>>");
			std::string s2(word + std::string("\r\n"));
			appendText(ui->textField, (s1 + s2).c_str());
		} else if (strcmp(word, "PRIVMSG") == 0) {
		    // TODO handle privmsg
			appendText(ui->textField, "<<Privmsg>>");
			OutputDebugStringA(tmpbuf);
		} else {
			// Handle second argument
			word = strtok(NULL, " ");

			if (strcmp(word, "JOIN") == 0) { // Successful channel join
				char *channel = strtok(NULL, " ");
				char *name = strtok(recvbuf, "!");

				// Discard the endline
				channel[strlen(channel) - 1] = 0;

				// Discard the initial ":"
				for (int i = 0; i < strlen(name); ++i)
					name[i] = name[i+1];

				name[strlen(name)] = 0;

				// We joined a new channel
				if (strcmp(name, "kekbot") == 0) {
					addTab(ui->tabCtrl, channel);
				// Someone else joined our channel
				} else {
					handleForeignJoin(ui->tabCtrl, channel, name);
				}

				//appendText(ui->textField, recvbuf);
			} else if (strcmp(word, MOTD) == 0) {
				char *msg = strstr(recvbuf, ":-");  // The actual message

				if (msg) {
					std::string s1("<<MOTD>>");
					std::string s2(msg);

					appendText(ui->textField, (s1 + s2).c_str());
				}
			} else if (strcmp(word, CHANNELINFO) == 0) {
				char *substr = strstr(recvbuf, "#");
				char *topic = strstr(substr, ":");
				char *channel = strtok(substr, " ");

				std::string s1("<<CHANNEL>> ");
				std::string s2(std::string(channel) + std::string("\r\n"));

				SendMessage(ui->channelList, LB_ADDSTRING, 0, (LPARAM)channel);
				
				char buf[128];
				int itemCount = GetListBoxInfo(ui->channelList);
				sprintf(buf, "Current LBS item count: %d\n", itemCount);
				OutputDebugStringA(buf);
				
				//appendText(*ui, (s1 + s2).c_str());

				// If channel has topic
				if (strlen(topic) > 1) {
					s1 = ":Topic:";
					s2 = topic;

					//appendText(*ui, (s1 + s2).c_str());
				}

			} else if (strcmp(word, NAMES) == 0) {
				appendText(ui->textField, recvbuf);
				char *response = strstr(recvbuf, "=");
				char *channel = strstr(response, "#");
				char *namelist = strstr(response, ":");

				char *name = strtok(namelist, " ");
				strncpy(name, name + 1, strlen(name) - 1); // To remove the starting ":"
				name[strlen(name) - 1] = 0;

				while (name) {
					SendMessage(ui->userList, LB_ADDSTRING, 0, (LPARAM)name);
					name = strtok(NULL, " ");
				}
				
			} else {
				// Leftover messages (not handled yet)
				appendText(ui->textField, recvbuf);
			}
		}

	} while (iResult > 0 && listeningSocket);

	appendText(ui->textField, "Socket listening stopped: \n");

	return 0;
}

// connData contains server and port in the form of <server>:<port>
bool irc_connect(char *connData, UI ui) {
	localUI = ui;
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

	// irc.cc.tut.fi
	// int iResult = getaddrinfo("192.98.101.230", "6667", &hints, &result);

	// dreamhack.se.quakenet.org 77.80.253.131
	char *server = strtok(connData, ":");
	char *port = strtok(NULL, ":");

	int iResult = getaddrinfo(server, port, &hints, &result);
	if (iResult != 0) {
		sprintf(formattedBuf, "address info failed with error %d\n", iResult);
		WSACleanup();
		return false;
	}

	ptr = result;

	connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
	if (connectSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return false;
	}

	// Connect to server.
	appendText(ui.textField, "Connecting to a server...\n");
	iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(connectSocket);
		connectSocket = INVALID_SOCKET;
	}

	freeaddrinfo(result);
	if (connectSocket == INVALID_SOCKET) {
		appendText(ui.textField, "Unable to connect to server!\n");
		WSACleanup();
		return false;
	}

	char *sendbuf[2] = { {"NICK kekbot\r\n"}, {"USER kekbot 0 * :kekbotti\r\n"} };
	
	// Send an initial buffer
	for (int i = 0; i < 2; ++i) {
		iResult = send(connectSocket, sendbuf[i], (int)strlen(sendbuf[i]), 0);
		sprintf(formattedBuf, "Sent %d bytes\n", iResult);
		OutputDebugStringA(formattedBuf);
		if (iResult == SOCKET_ERROR) {
			printf("send failed: %d\n", WSAGetLastError());
			closesocket(connectSocket);
			WSACleanup();
			return false;
		}
	}

	threadHandle = (HANDLE)_beginthreadex(0, 0, socketListener, &localUI, 0, 0);

	return true;
}

bool irc_sendText(const char *text, UI ui) {
	std::string endline = "\r\n";
	std::string t(text, strlen(text));
	std::string p3 = t + endline;
	appendText(ui.textField, p3.c_str());
	int iResult = send(connectSocket, p3.c_str(), p3.size(), 0);
	OutputDebugStringA("Sent text:\n");
	OutputDebugStringA(p3.c_str());

	if (iResult == SOCKET_ERROR) {
		printf("send failed: %d\n", WSAGetLastError());
		closesocket(connectSocket);
		WSACleanup();
		return false;
	}

	return true;
}

void irc_terminateConnection(UI ui) {
	appendText(ui.textField, "Terminating connection\n");

	listeningSocket = false;
	CloseHandle(threadHandle);

	int iResult = shutdown(connectSocket, SD_BOTH);
	if (iResult == SOCKET_ERROR) {
		OutputDebugStringA("shutdown failed: \n");
		closesocket(connectSocket);
		WSACleanup();
	}

	closesocket(connectSocket);
	WSACleanup();
}