#include <Windows.h>
#include <stdarg.h>
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
#define TAB_CTRL 6

#define MAX_CHANNEL_NAME_LEN 128
#define MAX_TEXT_LEN 512
#define MAX_CHANNEL_TABS 10             // max amount of channels open at any time
#define MAX_TEXT_BUF_PER_CHANNEL 65535  // max amount of bytes stored per channel
#define MAX_NICK_LEN 100                // max length of user nickname
#define MAX_USERS_PER_CHANNEL 100

#define TEXT_VIEW_BG RGB(27, 28, 22)
#define TEXT_VIEW_FG RGB(255, 0, 0)

const char* INIT_TEXT_DH = "/connect dreamhack.se.quakenet.org:6667";
const char* INIT_TEXT_HMN = "/connect irc.handmade.network:7666";

struct UI {
	HWND textField;
	HWND channelList;
	HWND userList;
	HWND inputField;
	HWND terminateBtn;
	HWND channelQueryBtn;
	HWND tabCtrl;
};

// Handles to old process handlers used for custom msg handling
WNDPROC oldEditProc;
WNDPROC oldTabCtrlProc;
WNDPROC oldQueryChannelsProc;
WNDPROC oldChannelListProc;

char channelTextBuf[MAX_CHANNEL_TABS][MAX_TEXT_BUF_PER_CHANNEL] = {};
// Names in the current channel, not the channel names themselves
char channelNameBuf[MAX_CHANNEL_TABS][MAX_NICK_LEN * MAX_USERS_PER_CHANNEL] = {};
int currTabSelection = 0;
static UI ui;

// Text view background brush
HBRUSH textViewBg;

void (*uiCallback)(Action action, const char *data);

static void str_append(char *resbuf, int argc, ...) {
	va_list ap;
	va_start(ap, argc);

	int currLen = 0;

	for (int i = 0; i < argc; ++i) {
		const char *p = va_arg(ap, char*);
		
		for (int j = currLen, k = 0; j < currLen + strlen(p); ++j, ++k) {
			resbuf[j] = p[k];
		}

		currLen += strlen(p);
	}

	va_end(ap);
}

static void currentTabName(char *outputbuf) {
	TCITEM t = {};
	t.mask = TCIF_TEXT;
	t.pszText = outputbuf;
	t.cchTextMax = MAX_CHANNEL_NAME_LEN;

	TabCtrl_GetItem(ui.tabCtrl, currTabSelection, &t);
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

void ui_handlePrivMsg(const char *sender, const char *receiver, const char *msg) {
	OutputDebugStringA("SENDER-");
	OutputDebugStringA(sender);
	OutputDebugStringA("RECEIVER-");
	OutputDebugStringA(receiver);
	OutputDebugStringA("MSG-");
	OutputDebugStringA(msg);
	OutputDebugStringA("ENDL");
	char channelName[MAX_CHANNEL_NAME_LEN] = {};
	currentTabName(channelName);

	// Replaces newlines from the channel name
	channelName[strcspn(channelName, "\r\n")] = 0;

	// PRIVMSG sent directly to us
	if (strcmp(receiver, "kekbot") == 0) {
		// Currently looking at the main tab
		if (strcmp(channelName, "Main") == 0) {
			char buf[MAX_TEXT_LEN] = {};
			str_append(buf, 5, "[", sender, "] ", msg, "\n");
			ui_appendText(buf);
		} else {
			char buf[MAX_TEXT_LEN] = {};
			str_append(buf, 5, "[", sender, "] ", msg, "\n");
			str_append(channelTextBuf[0], 2, channelTextBuf[0], buf);
		}
	} else {
		// PRIVMSG sent to the current visible channel
		OutputDebugStringA("CURRENTLY VISIBLE CHANNEL-");
		OutputDebugStringA(channelName);
		OutputDebugStringA("ENDL\n");

		if (strcmp(channelName, receiver) == 0) {
			char buf[MAX_TEXT_LEN] = {};
			str_append(buf, 5, "[", sender, "] ", msg, "\n");
			ui_appendText(buf);
		} else {
			// PRIVMSG sent to a channel currently not visible
			int tabCount = SendMessage(ui.tabCtrl, TCM_GETITEMCOUNT, 0, 0);

			// Find the correct channel and append to its text buffer
			for (int i = 0; i < tabCount; ++i) {
				char tabName[MAX_CHANNEL_NAME_LEN] = {};

				TCITEM t = {};
				t.mask = TCIF_TEXT;
				t.pszText = tabName;
				t.cchTextMax = MAX_CHANNEL_NAME_LEN;

				TabCtrl_GetItem(ui.tabCtrl, i, &t);

				tabName[strcspn(tabName, "\r\n")] = 0;

				if (strcmp(receiver, tabName) == 0) {
					char buf[MAX_TEXT_LEN] = {};
					str_append(buf, 5, "[", sender, "] ", msg, "\n");
					str_append(channelTextBuf[i], 2, channelTextBuf[i], buf);
					OutputDebugStringA("APPENDED TO CORRECT TEXT BUFFER\n");
					break;
				}
			}
		}
	}
}

void ui_handleForeignJoin(const char *channelName, const char *userName) {
	ui_appendText("<<Foreign Join>>");
	ui_appendText(channelName);
	ui_appendText("--");
	ui_appendText(userName);
	ui_appendText("\r\n");

	SendMessage(ui.tabCtrl, FOREIGN_JOIN, (WPARAM)channelName, (LPARAM)userName);
}

void ui_addChannel(const char *channelName, const char *userCount, const char *topic) {
	LVITEM lvi = {};
	lvi.mask = LVIF_TEXT;
	lvi.pszText = (LPSTR)channelName;
	lvi.iSubItem = 0;
	lvi.iItem = 0;
	lvi.cchTextMax = strlen(channelName);

	int index = ListView_InsertItem(ui.channelList, &lvi);
	ListView_SetItemText(ui.channelList, index, 1, (LPSTR)userCount);
	ListView_SetItemText(ui.channelList, index, 2, (LPSTR)topic);

	//SendMessage(ui.channelList, LB_ADDSTRING, 0, (LPARAM)channelName);
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
		// TODO maybe change str_append to work better for these situations?
		str_append(channelNameBuf[currTabSelection], 3, channelNameBuf[currTabSelection], name, "\n");
		SendMessage(ui.userList, LB_DELETESTRING, 0, 0);
	}

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

LRESULT CALLBACK ChannelListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_COMMAND: {
			// Clicked a channel in the list
			if (HIWORD(wParam) == LBN_DBLCLK) {
				int index = SendMessage(hwnd, LB_GETCURSEL, 0, 0);

				char buf[MAX_CHANNEL_NAME_LEN] = {};
				SendMessage(hwnd, LB_GETTEXT, index, (LPARAM)buf);

				OutputDebugStringA("CHANNEL NAME\n");
				OutputDebugStringA(buf);

				// TODO prompt dialog for joining this channel
			}
		} break;
	}

	return CallWindowProc(oldChannelListProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK QueryChannelsBtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_COMMAND: {
			if (HIWORD(wParam) == BN_CLICKED) {
				uiCallback(IRC_QUERY_CHANNELS, "LIST\r\n");
				OutputDebugStringA("QUERY CHANNELS\n");
			}
		} break;
	}
	
	return CallWindowProc(oldQueryChannelsProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TabControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_DESTROY: {
			// When app is closing
			DeleteObject(textViewBg);
		} break;
		case WM_PARENTNOTIFY: {
			switch(LOWORD(wParam)) {
				case WM_CREATE: {
					// When this child window was created
					if (HIWORD(wParam) == TEXTVIEW) {
						textViewBg = CreateSolidBrush(TEXT_VIEW_BG);
					}
				} break;
			}
		} break;
		case WM_CTLCOLORSTATIC: {
			// Set the colors of the text view
		    HDC hdc = (HDC)wParam;
			SetBkColor(hdc, TEXT_VIEW_BG);
		    SetTextColor(hdc, TEXT_VIEW_FG);

		    return (LRESULT)textViewBg;
		}
		case WM_CTLCOLOREDIT: {
			// Set the colors of the text input
		    HDC hdc = (HDC)wParam;
			SetBkColor(hdc, TEXT_VIEW_BG);
		    SetTextColor(hdc, TEXT_VIEW_FG);

		    return (LRESULT)textViewBg;
		}
		case FOREIGN_JOIN: {
			char *channel = (char*)wParam;
			char *userNick = (char*)lParam;

			int tabCount = TabCtrl_GetItemCount(hwnd);

			// Get the index of the channel where the foreigner joined to
			char buf[MAX_CHANNEL_NAME_LEN] = {};
			currentTabName(buf);

			int channelIndex = -1;
			for (int i = 0; i < tabCount; ++i) {
				if (strcmp(buf, channel) == 0) {
					channelIndex = i;
				}
			}

			if (channelIndex == currTabSelection) {
				// Foreigner joined currently visible channel
				SendMessage(ui.userList, LB_ADDSTRING, 0, (LPARAM)userNick);
			} else {
				// Foreigner joined a channel we're not currently viewing
				str_append(channelNameBuf[channelIndex], 2, userNick, "\n");
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
				} else if (strcmp(word, "/join") == 0) {
					char cmd[MAX_TEXT_LEN] = {};
					str_append(cmd, 3, "JOIN ", strtok(NULL, " "), "\r\n");

					uiCallback(IRC_SEND_TEXT, cmd);
				} else if (strcmp(word, "/quit") == 0) {
					char cmd[MAX_TEXT_LEN] = {};
					str_append(cmd, 3, "QUIT ", strtok(NULL, " "), "\r\n");

					uiCallback(IRC_SEND_TEXT, cmd);
				} else if (strcmp(word, "/list") == 0) {
					uiCallback(IRC_SEND_TEXT, "LIST");
				} else {
					char buf[MAX_CHANNEL_NAME_LEN] = {};
					currentTabName(buf);
					buf[strlen(buf) - 1] = '\0'; // replace '\n' with '0'

					
					char tmp_msg[MAX_TEXT_LEN * 2] = {};
					str_append(tmp_msg, 5, "PRIVMSG ", buf, " :", tmp_buf, "\r\n");

					uiCallback(IRC_SEND_TEXT, tmp_msg);
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
	SetWindowPos(ui.channelQueryBtn, 0, 0, globalY - 20, listboxWidth, 20, 0);
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

	// Query channels button
	HWND channelQueryBtn = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Button"),
		TEXT("Query channels"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
		0, 30, 80, 20,
		root, (HMENU)42, NULL, NULL);
	oldQueryChannelsProc = (WNDPROC)SetWindowLongPtr(channelQueryBtn, GWLP_WNDPROC, (LONG_PTR)QueryChannelsBtnProc);

	// Channel list
	//HWND channelList = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("ListBox"),
	//	TEXT(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_HASSTRINGS | LBS_NOTIFY,
	//	0, 50, 80, wHeight - 100,
	//	root, (HMENU)CHANNELLIST, NULL, NULL);
	//oldChannelListProc = (WNDPROC)SetWindowLongPtr(channelList, GWLP_WNDPROC, (LONG_PTR)ChannelListProc);

	HWND channelList = CreateWindow(WC_LISTVIEW, "", 
		WS_VISIBLE|WS_BORDER|WS_CHILD | LVS_REPORT | LVS_EDITLABELS | LVS_SORTASCENDING, 
         0, 50, 200, wHeight - 100, 
         root, (HMENU)1234, NULL, NULL);
	ui.channelList = channelList;
	
	// ListView Column data
	LVCOLUMN lvc = {};
	lvc.mask = LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 50;
	lvc.pszText = "Channel";

	ListView_InsertColumn(channelList, 1, &lvc);
	lvc.cx = 50;
	lvc.pszText = "People";
	ListView_InsertColumn(channelList, 2, &lvc);
	lvc.pszText = "Topic";
	lvc.cx = 200;
	ListView_InsertColumn(channelList, 2, &lvc);

	// Tab control
	HWND tabCtrl = CreateWindowEx(0, WC_TABCONTROL,
		TEXT("Channel count"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		300, 50, wWidth - 300, wHeight - 50,
		root, (HMENU)TAB_CTRL, NULL, NULL);
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

	HFONT hFont = CreateFont (16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, 
										OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 
										DEFAULT_PITCH, TEXT("Arial"));

	SendMessage(textField, WM_SETFONT, (WPARAM)hFont, TRUE);

	// Input field
	HWND inputField = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"),
		TEXT(INIT_TEXT_HMN), WS_CHILD | WS_VISIBLE | ES_WANTRETURN,
		0, wHeight - 80, wWidth - 300, 40,
		tabCtrl, (HMENU)INPUT, NULL, NULL);
	oldEditProc = (WNDPROC)SetWindowLongPtr(inputField, GWLP_WNDPROC, (LONG_PTR)EditControlProc);

	SendMessage(inputField, WM_SETFONT, (WPARAM)hFont, TRUE);


	// Terminate connection button
	HWND terminateBtn = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Button"),
		TEXT("Terminate"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
		wWidth - 300, wHeight - 80, 100, 40,
		tabCtrl, (HMENU)TERMINATE_BUTTON, NULL, NULL);

	ui.inputField = inputField;
	ui.terminateBtn = terminateBtn;
	ui.textField = textField;
	//ui.channelList = channelList;
	ui.userList = userList;
	ui.channelQueryBtn = channelQueryBtn;
	ui.tabCtrl = tabCtrl;

	// TODO dynamic text
	//SetDlgItemText(channelCount, 42, "asd regards");

	SendMessage(inputField, EM_SETLIMITTEXT, MAX_TEXT_LEN, 0); // Max num of bytes in input field
	SendMessage(textField, EM_SETLIMITTEXT, MAX_TEXT_BUF_PER_CHANNEL, 0); // Max num of bytes in text view (INTEGER MAX)

	SetFocus(inputField);
}