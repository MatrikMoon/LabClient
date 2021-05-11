#include "networking.h"

namespace MoonNetworking {
	int initWinsock() {
		#ifdef _WIN32
		WSADATA wsaData;
		// Initialize Winsock
		int iResult1 = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult1 != 0) {
			printf("WSAStartup failed with error: %d\n", iResult1);
			return 0;
		}
		#endif
		return 1;
	}
}