#include "core.h"
#include "utils.h"
#include "networking.h"
#include "commands.h"
#include "fileTransfer.h"
#include <thread>

static MoonNetworking::Client client;

int parseMessages(MoonNetworking::Server *s, char * buf, int length) {
	printf("TCP: %s\n", buf);
	parseFile(s, buf, length);
	return 0;
}

int parseUDPMessages(MoonNetworking::Server *s, char * buf, int length) {
	char * c = new char[length + 1];
	memset(c, 0, length + 1);
	memcpy(c, buf, length);
	printf("UDP: %s\n", c);
	delete[] c;

	parseFile(s, buf, length);
	return 0;
}

int core() {
	std::cout << "Server or client today? [s/c]: ";
	char input[BUFLEN];
	memset(input, 0, BUFLEN);
	fgets(input, BUFLEN, stdin);
	if (input[strlen(input) - 1] == '\n') {
		input[strlen(input) - 1] = '\0';
	}

	if (strncmp(input, "c", 1) == 0) {
		//Start the networking
		if (!client.connectTCP("m.servequake.com", "10150")) {
			Sleep(6000);
			std::cout << "TCP FAILED\n";
			return 1;
		}

		//Create thread to receive data
		client.receiveTCP(&parseMessages);

		if (!client.connectUDP("m.servequake.com", "10152")) {
			Sleep(6000);
			std::cout << "UDP FAILED\n";
			return 1;
		}

		client.receiveUDP(&parseUDPMessages);

		//Loop for input
		while (true) {
			char buffer[BUFLEN];
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN, stdin);
			if (buffer[strlen(buffer) - 1] == '\n') {
				buffer[strlen(buffer) - 1] = '\0';
			}
            if (strncmp(buffer, "/file-send ", 11) == 0) {
                send_file(&client, &buffer[11]);
				continue;
            }
			client.sendUDP(buffer);
		}
	}
	else if (strncmp(input, "s", 1) == 0) {
		MoonNetworking::Server::listenTCP(&parseMessages, "10150");
		MoonNetworking::Server::listenUDP(&parseUDPMessages, "10152");

		while (true)
		{
			char buffer[BUFLEN];
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN, stdin);
			if (buffer[strlen(buffer) - 1] == '\n') {
				buffer[strlen(buffer) - 1] = '\0';
			}
            if (strncmp(buffer, "/file-send ", 11) == 0) {
                send_file(MoonNetworking::Server::clientList.at(0), &buffer[11]);
				continue;
            }
			//MoonNetworking::Server::broadcastTCP(buffer);
			MoonNetworking::Server::broadcastUDP(buffer);
		}
	}

	return 0;
}