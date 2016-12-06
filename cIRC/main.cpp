#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <CommCtrl.h>
#include <string>
#include "irc.h"
#include "custom_msg.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define WINDOW_OFFSET_X 300
#define WINDOW_OFFSET_Y 100

// UI ids
#define INPUT 1
#define CHANNELLIST 2
#define USERLIST 3
#define TEXTVIEW 4
#define TERMINATE_BUTTON 5

#define MAX_CHANNEL_NAME_LEN 128
#define MAX_TEXT_LEN 512
#define MAX_CHANNEL_TABS 10             // max amount of channels open at any time
#define MAX_TEXT_BUF_PER_CHANNEL 65535  // max amount of bytes stored per channel
#define MAX_NICK_LEN 100                // max length of user nickname
#define MAX_USERS_PER_CHANNEL 100

struct UI {
	HWND textField;
	HWND channelList;
	HWND userList;
	HWND inputField;
	HWND terminateBtn;
	HWND channelCount;
	HWND tabCtrl;
};

WNDPROC oldEditProc;
WNDPROC oldTabCtrlProc;
static UI ui;
char channelTextBuf[MAX_CHANNEL_TABS][MAX_TEXT_BUF_PER_CHANNEL] = {};
char channelNameBuf[MAX_CHANNEL_TABS][MAX_NICK_LEN * MAX_USERS_PER_CHANNEL] = {};
int currTabSelection = 0;

//:keijo_!webchat@dsl-trebrasgw2-54f944-149.dhcp.inet.fi PRIVMSG #kekbottestchannel :moro
//:keijo_!webchat@dsl-trebrasgw2-54f944-149.dhcp.inet.fi MODE #kekbottestchannel +v kekbot
//:keijo_!webchat@dsl-trebrasgw2-54f944-149.dhcp.inet.fi PRIVMSG kekbot :moro

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
	appendText(ui.textField, "<<Foreign Join>>");
	appendText(ui.textField, channelName);
	appendText(ui.textField, "--");
	appendText(ui.textField, userName);
	appendText(ui.textField, "\r\n");

	SendMessage(tabCtrl, FOREIGN_JOIN, (WPARAM)channelName, (LPARAM)userName);
}

void callback(char *response) {
	OutputDebugStringA(response);
	char tmpbuf[512];
	strcpy(tmpbuf, response);
	
	int i = 0;

	// Handle first argument
	char *word = strtok(tmpbuf, " ");

	if (strcmp(word, "PING") == 0) {
		word = strtok(NULL, " ");

		std::string s1("PONG ");
		std::string s2(word);
		irc_sendText((s1 + s2).c_str());
		appendText(ui.textField, "<<Responded to PING>>\n");
	} else if (strcmp(word, "NOTICE") == 0) {
		word = strtok(NULL, " ");

		std::string s1("<<Notify>>");
		std::string s2(word + std::string("\r\n"));
		appendText(ui.textField, (s1 + s2).c_str());
	} else if (strcmp(word, "PRIVMSG") == 0) {
	    // TODO handle privmsg
		appendText(ui.textField, "<<Privmsg>>");
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
				addTab(ui.tabCtrl, channel);
			// Someone else joined our channel
			} else {
				handleForeignJoin(ui.tabCtrl, channel, name);
			}

			//appendText(ui->textField, recvbuf);
		} else if (strcmp(word, MOTD) == 0) {
			char *msg = strstr(response, ":-");  // The actual message

			if (msg) {
				std::string s1("<<MOTD>>");
				std::string s2(msg);

				appendText(ui.textField, (s1 + s2).c_str());
			}
		} else if (strcmp(word, CHANNELINFO) == 0) {
			char *substr = strstr(response, "#");
			char *topic = strstr(substr, ":");
			char *channel = strtok(substr, " ");

			std::string s1("<<CHANNEL>> ");
			std::string s2(std::string(channel) + std::string("\r\n"));

			SendMessage(ui.channelList, LB_ADDSTRING, 0, (LPARAM)channel);
			
			char buf[128];
			int itemCount = GetListBoxInfo(ui.channelList);
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
			appendText(ui.textField, response);
			char *res = strstr(response, "=");
			char *channel = strstr(res, "#");
			char *namelist = strstr(res, ":");

			char *name = strtok(namelist, " ");
			strncpy(name, name + 1, strlen(name) - 1); // To remove the starting ":"
			name[strlen(name) - 1] = 0;

			while (name) {
				SendMessage(ui.userList, LB_ADDSTRING, 0, (LPARAM)name);
				name = strtok(NULL, " ");
			}
			
		} else {
			// Leftover messages (not handled yet)
			appendText(ui.textField, response);
		}
	}
}

static void append(const char *s1, const char *s2, char *resbuf) {
	size_t s1len = strlen(s1);
	for (int i = 0; i < s1len; ++i) {
		resbuf[i] = s1[i];
	}

	for (int i = 0; i < strlen(s2); ++i) {
		resbuf[s1len + i] = s2[i];
	}
}

static void changeChannel(int newChannelIndex) {
	char buf[MAX_CHANNEL_NAME_LEN] = {};
	TCITEM t = {};
	t.mask = TCIF_TEXT;
	t.pszText = buf;
	t.cchTextMax = sizeof(buf);

	// Get the current text in the text view
	char currText[MAX_TEXT_BUF_PER_CHANNEL];
	GetWindowText(ui.textField, currText, MAX_TEXT_BUF_PER_CHANNEL);

	// Copy the text at the correct index
	for (int i = 0; i < strlen(currText); ++i) {
		channelTextBuf[currTabSelection][i] = currText[i];
	}

	// Store the current user list to buffer and delete them from the listbox
	ZeroMemory(channelNameBuf[currTabSelection], MAX_NICK_LEN * MAX_USERS_PER_CHANNEL);
	int userCount = (int)SendMessage(ui.userList, LB_GETCOUNT, 0, 0);

	for (int i = 0; i < userCount; ++i) {
		char name[MAX_NICK_LEN] = {};
		SendMessage(ui.userList, LB_GETTEXT, 0, (LPARAM)name);
		name[strlen(name)] = '\n';
		append(channelNameBuf[currTabSelection], name, channelNameBuf[currTabSelection]);
		SendMessage(ui.userList, LB_DELETESTRING, 0, 0);
	}

	OutputDebugStringA("CHANNEL NAME LIST \n");
	OutputDebugStringA(channelNameBuf[currTabSelection]);

	// Add the new channel users back to listbox
	char *name = strtok(channelNameBuf[newChannelIndex], "\n");

	while (name != NULL) {
		SendMessage(ui.userList, LB_ADDSTRING, 0, (LPARAM)name);
		name = strtok(NULL, "\n");
	}

	// Set the text to the corresponding channel text
	SetWindowText(ui.textField, channelTextBuf[newChannelIndex]);
	currTabSelection = newChannelIndex;
}

LRESULT CALLBACK TabControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
		case FOREIGN_JOIN: {
			char *channel = (char*)wParam;
			char *userNick = (char*)lParam;

			int tabCount = TabCtrl_GetItemCount(hwnd);

			// Get the index of the channel where the foreigner joined to
			char buf[MAX_CHANNEL_NAME_LEN] = {};
			TCITEM t = {};
			t.mask = TCIF_TEXT;
			t.pszText = buf;
			t.cchTextMax = MAX_CHANNEL_NAME_LEN;

			int channelIndex = -1;
			for (int i = 0; i < tabCount; ++i) {
				TabCtrl_GetItem(hwnd, i, &t);
				if (strcmp(buf, channel) == 0) {
					channelIndex = i;
				}
			}

			if (channelIndex == currTabSelection) {
				// Foreigner joined currently visible channel
				SendMessage(ui.userList, LB_ADDSTRING, 0, (LPARAM)userNick);
			} else {
				// Foreigner joined a channel we're not currently viewing
				char tmp[MAX_NICK_LEN] = {};
				append(tmp, userNick, tmp);
				tmp[strlen(tmp)] = '\n';   // Add name list delimeter
				append(channelNameBuf[channelIndex], tmp, channelNameBuf[channelIndex]);
			}

		} break;
		case WM_COMMAND: {
			if (HIWORD(wParam) == BN_CLICKED) {
				if ((HWND)lParam == ui.terminateBtn) {
					irc_sendText("QUIT Client terminated");
					irc_terminateConnection();
				}
			}
		} break;
		case TCM_SETCURSEL: {
			changeChannel((int)wParam);
		} break;
	}

	return CallWindowProc(oldTabCtrlProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK EditControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
		case WM_CHAR: {
			if (wParam == VK_RETURN) {
				char text_buffer[MAX_TEXT_LEN] = {};
				GetWindowTextA(hwnd, text_buffer, MAX_TEXT_LEN - 1);
				char tmp_buf[MAX_TEXT_LEN] = {};
				strcpy(tmp_buf, text_buffer);

				char *word = strtok(text_buffer, " ");

				if (!word) // return if empty text field
					return CallWindowProc(oldEditProc, hwnd, msg, wParam, lParam);
				

				if (strcmp(word, "/connect") == 0) {
					irc_connect(strtok(NULL, " "), callback);
				} else if (strcmp(word, "/join") == 0) {
					char cmd[MAX_TEXT_LEN] = {};
					append("JOIN ", strtok(NULL, " "), cmd);
					append(cmd, "\r\n", cmd);

					irc_sendText(cmd);
				} else if (strcmp(word, "/quit") == 0) {
					char cmd[MAX_TEXT_LEN] = {};
					append("QUIT ", strtok(NULL, " "), cmd);
					append(cmd, "\r\n", cmd);

					irc_sendText(cmd);
				} else if (strcmp(word, "/list") == 0) {
					irc_sendText("LIST");
				} else {
					append(tmp_buf, "\r\n", tmp_buf);
					irc_sendText(tmp_buf);
				}

				SetWindowTextA(hwnd, "");
				return 0; // to prevent messsage beep
			}
		} break;
	}

	return CallWindowProc(oldEditProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
		case WM_NOTIFY: {
			LPNMHDR	tc = (LPNMHDR)lParam;

			// If user clicked channel tab
			if ((tc->code == TCN_SELCHANGE) && (tc->hwndFrom == ui.tabCtrl)) {
				changeChannel(TabCtrl_GetCurSel(ui.tabCtrl));
			}
		} break;
		case WM_CREATE: {
			int wWidth = WINDOW_WIDTH;
			int wHeight = WINDOW_HEIGHT;

			INITCOMMONCONTROLSEX icex;
			icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
			icex.dwICC = ICC_TAB_CLASSES;
			InitCommonControlsEx(&icex);

			// Channel counter
			HWND channelCount = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"),
				TEXT("Channel count"), WS_CHILD | WS_VISIBLE | SS_LEFT,
				0, 30, 80, 20,
				hwnd, (HMENU)42, NULL, NULL);

			// Channel list
			HWND channelList = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("ListBox"),
				TEXT(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS,
				0, 50, 80, wHeight - 100,
				hwnd, (HMENU)CHANNELLIST, NULL, NULL);

			// Tab control
			HWND tabCtrl = CreateWindowEx(0, WC_TABCONTROL,
				TEXT("Channel count"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
				300, 50, wWidth - 300, wHeight - 50,
				hwnd, (HMENU)123, NULL, NULL);
			oldTabCtrlProc = (WNDPROC)SetWindowLongPtr(tabCtrl, GWLP_WNDPROC, (LONG_PTR)TabControlProc);


			// Add item to tab control
			TCITEM tie1;
			tie1.mask = TCIF_TEXT;
			tie1.pszText = "Main";

			TabCtrl_InsertItem(tabCtrl, 0, &tie1);

			// User list
			HWND userList = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("ListBox"),
				TEXT(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS,
				wWidth - 80, 50, 80, wHeight - 100,
				tabCtrl, (HMENU)USERLIST, NULL, NULL);

			// Text view
			HWND textField = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"),
				TEXT(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_READONLY | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL,
				0, 0, wWidth - 200, wHeight - 150,
				tabCtrl, (HMENU)TEXTVIEW, NULL, NULL);

			// Input field
			HWND inputField = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"),
				TEXT("/connect dreamhack.se.quakenet.org:6667"), WS_CHILD | WS_VISIBLE | ES_WANTRETURN,
				0, wHeight - 80, wWidth - 300, 40,
				tabCtrl, (HMENU)INPUT, NULL, NULL);
			oldEditProc = (WNDPROC)SetWindowLongPtr(inputField, GWLP_WNDPROC, (LONG_PTR)EditControlProc);

			// Terminate connection button
			HWND terminateBtn = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Button"),
				TEXT("Terminate"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
				wWidth - 300, wHeight - 80, 100, 40,
				tabCtrl, (HMENU)TERMINATE_BUTTON, NULL, NULL);

			ui.inputField = inputField;
			ui.terminateBtn = terminateBtn;
			ui.textField = textField;
			ui.channelList = channelList;
			ui.userList = userList;
			ui.channelCount = channelCount;
			ui.tabCtrl = tabCtrl;

			// TODO dynamic text
			SetDlgItemText(channelCount, 42, "asd regards");

			SendMessage(inputField, EM_SETLIMITTEXT, MAX_TEXT_LEN, 0); // Max num of bytes in input field
			SendMessage(textField, EM_SETLIMITTEXT, MAX_TEXT_BUF_PER_CHANNEL, 0); // Max num of bytes in text view (INTEGER MAX)

			SetFocus(inputField);
		} break;
		case WM_SIZE: {
			RECT clientRect;
			GetClientRect(hwnd, &clientRect);
			int w = clientRect.right - clientRect.left;
			int h = clientRect.bottom - clientRect.top;

			int margin = 20;
			int globalY = 20;

			int listboxWidth = w / 6;
			int listboxHeight = h - globalY;

			int inputFieldHeight = h / 20;

			int terminateBtnHeight = inputFieldHeight;

			int textviewX = 0 + margin;
			int textviewHeight = h - globalY - margin - inputFieldHeight;
			int textviewWidth = w - (listboxWidth * 2 + margin * 2);

			// Resize main window child components
			SetWindowPos(ui.channelCount, 0, 0, globalY - 20, listboxWidth, 20, 0);
			SetWindowPos(ui.channelList, 0, 0, globalY, listboxWidth, h - globalY, 0);
			SetWindowPos(ui.tabCtrl, 0, listboxWidth + margin, globalY, w - (listboxWidth + margin), h - globalY, 0);
			
			// Resize tab control children
			SetWindowPos(ui.textField, 0, margin, globalY + margin, textviewWidth, textviewHeight - (margin * 2), 0);
			SetWindowPos(ui.userList, 0, textviewWidth + margin, globalY + margin, listboxWidth, h - globalY, 0);
			SetWindowPos(ui.inputField, 0, margin, textviewHeight + margin, textviewWidth - 100, inputFieldHeight, 0);
			SetWindowPos(ui.terminateBtn, 0, margin + (textviewWidth - 100), textviewHeight + margin, 100, inputFieldHeight, 0);
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