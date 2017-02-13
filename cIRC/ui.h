#ifndef UI_H
#define UI_H

enum Action {
	IRC_TERMINATE,
	IRC_CONNECT,
	IRC_LIST,
	IRC_SEND_TEXT,
	IRC_QUERY_CHANNELS
};

void ui_init(void (*callback)(Action action, const char *data));
void ui_appendText(const char *text);
void ui_addTab(const char *tabName);
void ui_handleForeignJoin(const char *channelName, const char *userName);
void ui_handlePrivMsg(const char *sender, const char *receiver, const char *msg);
void ui_createComponents(HWND root, int wWidth, int wHeight);
void ui_changeChannel();
void ui_changeChannel(int newChannelIndex);
void ui_resizeComponents(int wWidth, int wHeight);
void ui_addChannel(const char *channelName, const char *userCount, const char *topic);
void ui_addUser(const char *userName);
bool ui_clickedTab(LPNMHDR tc);

#endif