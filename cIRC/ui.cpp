#include <Windows.h>
#include <stdio.h>  // tmp debugging
#include <CommCtrl.h>
#include "ui.h"
#include "custom_msg.h"

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

// Handles to old process handlers used for custom msg handling
WNDPROC oldEditProc;
WNDPROC oldTabCtrlProc;

char channelTextBuf[MAX_CHANNEL_TABS][MAX_TEXT_BUF_PER_CHANNEL] = {};
char channelNameBuf[MAX_CHANNEL_TABS][MAX_NICK_LEN * MAX_USERS_PER_CHANNEL] = {};
int currTabSelection = 0;
static UI ui;

void (*uiCallback)(Action action, const char *data);

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

void ui_init(void (*callback)(Action action, const char *data)) {
	uiCallback = callback;
}

bool ui_clickedTab(LPNMHDR tc) {
	return (tc->code == TCN_SELCHANGE) && (tc->hwndFrom == ui.tabCtrl);
}

void ui_appendText(const char *text) {
	int left, right;
	int len = GetWindowTextLength(ui.textField);
	SendMessageA(ui.textField, EM_GETSEL, (WPARAM)&left, (LPARAM)&right);
	SendMessageA(ui.textField, EM_SETSEL, len, len);
	SendMessageA(ui.textField, EM_REPLACESEL, 0, (LPARAM)text);
	SendMessageA(ui.textField, EM_SETSEL, left, right);
}

void ui_addTab(const char *tabName) {
	static TCITEM t;
	t.mask = TCIF_TEXT;
	t.pszText = (LPSTR)tabName;
	t.cchTextMax = strlen(tabName);

	int tabCount = SendMessage(ui.tabCtrl, TCM_GETITEMCOUNT, 0, 0);
	PostMessage(ui.tabCtrl, TCM_INSERTITEM, tabCount, (LPARAM)(&t));
	PostMessage(ui.tabCtrl, TCM_SETCURSEL, tabCount, 0);
}

void ui_handleForeignJoin(const char *channelName, const char *userName) {
	ui_appendText("<<Foreign Join>>");
	ui_appendText(channelName);
	ui_appendText("--");
	ui_appendText(userName);
	ui_appendText("\r\n");

	SendMessage(ui.tabCtrl, FOREIGN_JOIN, (WPARAM)channelName, (LPARAM)userName);
}

void ui_addChannel(const char *channelName) {
	SendMessage(ui.channelList, LB_ADDSTRING, 0, (LPARAM)channelName);
}

void ui_addUser(const char *userName) {
	SendMessage(ui.userList, LB_ADDSTRING, 0, (LPARAM)userName);
}

// Function used when a user clicks channel from the UI
void ui_changeChannel() {
	ui_changeChannel(TabCtrl_GetCurSel(ui.tabCtrl));
}

// Function used when joining a new channel for the first time
void ui_changeChannel(int newChannelIndex) {
	char tmpbuf[128];
	sprintf(tmpbuf, "CHANNEL INDEX: %d\n", newChannelIndex);
	OutputDebugStringA(tmpbuf);


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
					uiCallback(IRC_TERMINATE, "QUIT :Client terminated\r\n");
				}
			}
		} break;
		case TCM_SETCURSEL: {
			ui_changeChannel(wParam);
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
					uiCallback(IRC_CONNECT, strtok(NULL, " "));
					//irc_connect(strtok(NULL, " "), callback);
				} else if (strcmp(word, "/join") == 0) {
					char cmd[MAX_TEXT_LEN] = {};
					append("JOIN ", strtok(NULL, " "), cmd);
					append(cmd, "\r\n", cmd);

					uiCallback(IRC_SEND_TEXT, cmd);
					//irc_sendText(cmd);
				} else if (strcmp(word, "/quit") == 0) {
					char cmd[MAX_TEXT_LEN] = {};
					append("QUIT ", strtok(NULL, " "), cmd);
					append(cmd, "\r\n", cmd);

					uiCallback(IRC_SEND_TEXT, cmd);
					//irc_sendText(cmd);
				} else if (strcmp(word, "/list") == 0) {
					uiCallback(IRC_SEND_TEXT, "LIST");
					//irc_sendText("LIST");
				} else {
					append(tmp_buf, "\r\n", tmp_buf);
					uiCallback(IRC_SEND_TEXT, tmp_buf);
					//irc_sendText(tmp_buf);
				}

				SetWindowTextA(hwnd, "");
				return 0; // to prevent messsage beep
			}
		} break;
	}

	return CallWindowProc(oldEditProc, hwnd, msg, wParam, lParam);
}

void ui_resizeComponents(int wWidth, int wHeight) {
	int margin = 20;
	int globalY = 20;

	int listboxWidth = wWidth / 6;
	int listboxHeight = wHeight - globalY;

	int inputFieldHeight = wHeight / 20;

	int terminateBtnHeight = inputFieldHeight;

	int textviewX = 0 + margin;
	int textviewHeight = wHeight - globalY - margin - inputFieldHeight;
	int textviewWidth = wWidth - (listboxWidth * 2 + margin * 2);

	// Resize main window child components
	SetWindowPos(ui.channelCount, 0, 0, globalY - 20, listboxWidth, 20, 0);
	SetWindowPos(ui.channelList, 0, 0, globalY, listboxWidth, wHeight - globalY, 0);
	SetWindowPos(ui.tabCtrl, 0, listboxWidth + margin, globalY, wWidth - (listboxWidth + margin), wHeight - globalY, 0);
	
	// Resize tab control children
	SetWindowPos(ui.textField, 0, margin, globalY + margin, textviewWidth, textviewHeight - (margin * 2), 0);
	SetWindowPos(ui.userList, 0, textviewWidth + margin, globalY + margin, listboxWidth, wHeight - globalY, 0);
	SetWindowPos(ui.inputField, 0, margin, textviewHeight + margin, textviewWidth - 100, inputFieldHeight, 0);
	SetWindowPos(ui.terminateBtn, 0, margin + (textviewWidth - 100), textviewHeight + margin, 100, inputFieldHeight, 0);
}

void ui_createComponents(HWND root, int wWidth, int wHeight) {
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_TAB_CLASSES;
	InitCommonControlsEx(&icex);

	// Channel counter
	HWND channelCount = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"),
		TEXT("Channel count"), WS_CHILD | WS_VISIBLE | SS_LEFT,
		0, 30, 80, 20,
		root, (HMENU)42, NULL, NULL);

	// Channel list
	HWND channelList = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("ListBox"),
		TEXT(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS,
		0, 50, 80, wHeight - 100,
		root, (HMENU)CHANNELLIST, NULL, NULL);

	// Tab control
	HWND tabCtrl = CreateWindowEx(0, WC_TABCONTROL,
		TEXT("Channel count"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		300, 50, wWidth - 300, wHeight - 50,
		root, (HMENU)123, NULL, NULL);
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
}