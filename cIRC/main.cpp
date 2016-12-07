#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include "irc.h"
#include "ui.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define WINDOW_OFFSET_X 300
#define WINDOW_OFFSET_Y 100

//:keijo_!webchat@dsl-trebrasgw2-54f944-149.dhcp.inet.fi PRIVMSG #kekbottestchannel :moro
//:keijo_!webchat@dsl-trebrasgw2-54f944-149.dhcp.inet.fi MODE #kekbottestchannel +v kekbot
//:keijo_!webchat@dsl-trebrasgw2-54f944-149.dhcp.inet.fi PRIVMSG kekbot :moro

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

void socketCallback(char *response) {
	OutputDebugStringA(response);
	char tmpbuf[512];       // For tokenizing
	char resbuf[512] = {};  // For results
	strcpy(tmpbuf, response);
	
	int i = 0;

	// Handle first argument
	char *word = strtok(tmpbuf, " ");

	if (strcmp(word, "PING") == 0) {
		word = strtok(NULL, " ");
		append("PONG ", word, resbuf);

		irc_sendText(resbuf);
		ui_appendText("<<Responded to PING>>\n");
	} else if (strcmp(word, "NOTICE") == 0) {
		word = strtok(NULL, " ");
		append("<<Notice>>", word, resbuf);
		append(resbuf, "\r\n", resbuf);

		ui_appendText(resbuf);
	} else if (strcmp(word, "PRIVMSG") == 0) {
	    // TODO handle privmsg
		ui_appendText("<<Privmsg>>");
		OutputDebugStringA(tmpbuf);
	} else {
		// Handle second argument
		word = strtok(NULL, " ");

		if (strcmp(word, "JOIN") == 0) { // Successful channel join
			char *channel = strtok(NULL, " ");
			char *name = strtok(response, "!");

			// Discard the endline
			channel[strlen(channel) - 1] = 0;

			// Discard the initial ":"
			for (int i = 0; i < strlen(name); ++i)
				name[i] = name[i+1];

			name[strlen(name)] = 0;

			// We joined a new channel
			if (strcmp(name, "kekbot") == 0) {
				ui_addTab(channel);
			// Someone else joined our channel
			} else {
				ui_handleForeignJoin(channel, name);
			}

			//appendText(ui->textField, recvbuf);
		} else if (strcmp(word, MOTD) == 0) {
			char *msg = strstr(response, ":-");  // The actual message

			if (msg) {
				append("<<MOTD>>", msg, resbuf);

				ui_appendText(resbuf);
			}
		} else if (strcmp(word, CHANNELINFO) == 0) {
			char *substr = strstr(response, "#");
			char *topic = strstr(substr, ":");
			char *channel = strtok(substr, " ");

			append("<<CHANNEL>> ", channel, resbuf);
			append(resbuf, "\r\n", resbuf);

			ui_addChannel(channel);
			
			// TODO handle this in ui code
			//char buf[128];
			//int itemCount = GetListBoxInfo(ui.channelList);
			//sprintf(buf, "Current LBS item count: %d\n", itemCount);
			//OutputDebugStringA(buf);

			// If channel has topic
			if (strlen(topic) > 1) {
				//s1 = ":Topic:";
				//s2 = topic;

				//appendText(*ui, (s1 + s2).c_str());
			}

		} else if (strcmp(word, NAMES) == 0) {
			ui_appendText(response);
			char *res = strstr(response, "=");
			char *channel = strstr(res, "#");
			char *namelist = strstr(res, ":");

			char *name = strtok(namelist, " ");
			strncpy(name, name + 1, strlen(name) - 1); // To remove the starting ":"
			name[strlen(name) - 1] = 0;

			while (name) {
				ui_addUser(name);
				name = strtok(NULL, " ");
			}
			
		} else {
			// Leftover messages (not handled yet)
			ui_appendText(response);
		}
	}
}

void uiCallback(Action action, const char *data) {
	switch(action) {
		case IRC_CONNECT: {
			irc_connect((char*)data, socketCallback);
		} break;
		case IRC_TERMINATE: {
			irc_sendText(data);
			irc_terminateConnection();
		} break;
		case IRC_SEND_TEXT: {
			irc_sendText(data);
		} break;
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
		case WM_NOTIFY: {
			LPNMHDR	tc = (LPNMHDR)lParam;

			// If user clicked channel tab
			if (ui_clickedTab(tc)) {
				ui_changeChannel();
			}
		} break;
		case WM_CREATE: {
			ui_init(uiCallback);
			ui_createComponents(hwnd, WINDOW_WIDTH, WINDOW_HEIGHT);
		} break;
		case WM_SIZE: {
			RECT clientRect;
			GetClientRect(hwnd, &clientRect);
			int w = clientRect.right - clientRect.left;
			int h = clientRect.bottom - clientRect.top;
			ui_resizeComponents(w, h);
		} break;
		case WM_DESTROY: {
			exit(0);
		} break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	MSG msg;
	WNDCLASS wc = {};
	wc.lpszClassName = TEXT("cIRC");
	wc.hInstance = hInstance;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpfnWndProc = WndProc;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	
	RegisterClass(&wc);
	HWND handle = CreateWindow(wc.lpszClassName, TEXT("cIRC"),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		WINDOW_OFFSET_X, WINDOW_OFFSET_Y, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, hInstance, 0);

	while (true) {
		while (GetMessage(&msg, NULL, 0, 0)) {

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}