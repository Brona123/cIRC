#ifndef IRC_H
#define IRC_H

struct UI {
	HWND textField;
	HWND channelList;
	HWND userList;
	HWND inputField;
	HWND terminateBtn;
	HWND channelCount;
	HWND tabCtrl;
};

bool irc_connect(char *connData, UI ui);
void irc_terminateConnection(UI ui);
bool irc_sendText(const char *text, UI ui);

#endif