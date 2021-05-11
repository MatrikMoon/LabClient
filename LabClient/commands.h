#ifndef _COMMANDS_H
#define _COMMANDS_H

#ifdef _WIN32
#include <sstream>
#endif

//Custom
#include "networking.h"

int parseMessages(MoonNetworking::Client *c, char * buf, int length);
int parseUDPMessages(MoonNetworking::Client *c, char * buf, int length);
void sendTCPIntro(MoonNetworking::Client * c);

#endif