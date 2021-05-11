#ifndef _NETWORKING_H
#define _NETWORKING_H

#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#endif

#ifdef __linux__
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#define BUFLEN 4096

namespace MoonNetworking {
	int initWinsock();

	//----- CONNECTION -----//
	class Connection {
	public:
		virtual void sendTCP(std::string) = 0;
		virtual void sendTCP(char * buf) = 0;
		virtual void sendTCP(char * buf, int length) = 0;
		virtual void sendUDP(std::string) = 0;
		virtual void sendUDP(char * buf) = 0;
		virtual void sendUDP(char * buf, int length) = 0;
		virtual bool hasTCP() = 0;
		virtual bool hasUDP() = 0;
	};

	//----- CLIENT -----//
	class Client : public Connection {
		//TCP
		int sockfd;
		bool hasTCPb;

		//UDP
		int sock, port;
		unsigned int slength;
		struct sockaddr_in server;
		bool hasUDPb;

		//Parser setup
		typedef int(*PARSER)(Client *s, char * buf, int length);

		struct PARSESTRUCT {
			Client *s;
			PARSER p;
		};


		public:
			Client();
			std::string UUID;

			//TCP
			std::string hostTCP;
			std::string portTCP;

			//UDP
			std::string hostUDP;
			std::string portUDP;

			//TCP
			void sendTCP(std::string);
			void sendTCP(char * buf);
			void sendTCP(char * buf, int length);
			int receiveTCP(PARSER p);
			int receive_low(PARSER p);
			static void waitForRecvFunc(void * v);
			int connectTCP(std::string, std::string);
			void resetTCP();
			bool hasTCP();

			//UDP
			void sendUDP(std::string);
			void sendUDP(char * buf);
			void sendUDP(char * buf, int length);
			int receiveUDP(PARSER p);
			int receiveUDP_low(PARSER p);
			static void waitForUDPFunc(void * v);
			int connectUDP(std::string, std::string);
			bool hasUDP();
	};

	//----- SERVER -----//
	class Server : public Connection {
		//Global
		std::string UUID;

		//Misc
		bool hasTCPb;
		bool hasUDPb;
		void setHasTCP(bool);
		void setHasUDP(bool);

		//TCP
		static void internalTCPParser(void*, std::string);

		//UDP
		int sock;
		socklen_t fromlen;

		//Parser setup
		typedef int (*PARSER)(Server *c, char * s, int l);
		struct PARSESTRUCT {
			Server *c;
			PARSER p;
			int TCPPort;
			int UDPPort;
		};
    
		public:
			//Misc
			//Static list of all clients, just in case we need to single out one
			//for an action
			//This only declares, does not initialize them. Will do that
			//in the implementation.
			static std::vector<Server*> clientList;
			std::string getUUID();
			void setUUID(std::string);
			void attachTCP(int);
			void attachUDP(int, struct sockaddr_in, socklen_t);
			bool hasTCP();
			bool hasUDP();

			//Both
			bool equals(Server c);

			//Listener/Destructor
			~Server();

			//TCP
			int sockfd; //Initialized when tcp client is added to list
						//Also public for comparision purposes
			Server(int);
			void sendTCP(std::string);
			void sendTCP(char * buf);
			void sendTCP(char * buf, int length);
			static void broadcastTCP(std::string);
			static int listenTCP(PARSER p, std::string port);
			static void startListeningTCP(void * v); //Threaded
			static void listen_tcp_low(void * v); //Threaded

			//UDP
			struct sockaddr_in from; //Needs to be public for comparison purposes
			unsigned int length = sizeof(from);
			Server(int, sockaddr_in, socklen_t);
			void sendUDP(std::string);
			void sendUDP(char *);
			void sendUDP(char *, int);
			static void broadcastUDP(std::string);
			static int listenUDP(PARSER p, std::string port);
			static void startListeningUDP(void * v); //Threaded
	};
}
#endif