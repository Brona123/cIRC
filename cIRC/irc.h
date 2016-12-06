#ifndef IRC_H
#define IRC_H

#define MOTD "372"
#define CHANNELINFO "322"
#define NAMES "353"

bool irc_connect(char *connData, void (*callback)(char *response));
void irc_terminateConnection();
bool irc_sendText(const char *text);

#endif