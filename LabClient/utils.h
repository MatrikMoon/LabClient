#ifndef UTILS_H
#define UTILS_H

#include <iostream>

#ifdef _WIN32
//For Windows UUID generation
#define WIN32_LEAN_AND_MEAN //Don't interfere with potential Winsock2 inclusions
#include <Rpc.h>
#pragma comment (lib, "Rpcrt4.lib")

//For disc tray
#include <Mmsystem.h>
#pragma comment (lib, "winmm.lib")

//For ip enumeration
#include <WinSock2.h>
#include <IPHlpApi.h>
#include <WS2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#endif

#ifdef __linux__
#include <stdlib.h> //For wcstombs_s
#include <cstring> //For strncmp
#include <uuid/uuid.h>
#include <sys/stat.h>

//Disc tray
#include <fcntl.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

//Path enumeration
#include <libgen.h>
#include <sstream>
#include <cstring>
#include <unistd.h>
#define PATH_MAX        4096

//Resolution
#include <X11/Xlib.h>

//Hack-ports
int wcstombs_s(size_t* convertedNum, char* dst, size_t dstSize, wchar_t const* src, size_t maxCount);
int strcpy_s(char* dest, int size, char const* source);
char* _itoa_s(int value, char* result, int base);
void Sleep(long millis);
#endif

//Differs between os's
std::string uuid_generate();
void openDiscTray();
void closeDiscTray();
void GetDesktopResolution(int& horizontal, int& vertical);
std::string getCurrentExecutablePath();
std::string getLocalIp();

std::string wstoms(std::wstring input);
char* itoa(int value, char* result, int base);
void error(const char *msg);
void errorLite(const char *msg);
bool file_exists(const std::string& name);
#endif