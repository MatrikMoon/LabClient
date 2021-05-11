#include "networking.h"
#include "utils.h"

#ifdef _WIN32
#define close closesocket
#endif

namespace MoonNetworking {
	int timeout_counter = 0;

	//----GLOBAL----//
	Client::Client() {
		UUID = uuid_generate();
	}

	bool Client::hasTCP() {
		return hasTCPb;
	}

	bool Client::hasUDP() {
		return hasUDPb;
	}

	//-----TCP-----//
	void Client::sendTCP(std::string buf)
	{
		sendTCP(const_cast<char*>(buf.c_str()));
	}

	void Client::sendTCP(char * buf) {
		sendTCP(buf, strlen(buf));
	}

	void Client::sendTCP(char * buf, int length) {
		//Append <EOF> to the end.
		char * sends = new char[length + 6]; //+5 for <EOF> and null terminator
		memcpy(sends, buf, length); //Memcpy just in case of null bytes
		strcpy_s(&sends[length], 6, "<EOF>\0"); //Copy it over

		printf("SEND TCP: %s\n", sends);

		int n = send(sockfd, sends, length + 6, 0);
		if (n < 0) {
			printf("ERROR: SEND\n");
			return;
		}
		timeout_counter = 0; //A successful send indicates the server is still listening
	}

	//Start the real thread for receiving
	int Client::receiveTCP(PARSER p)
	{
		//Create a new structure that isn't bounded by our current scope
		//Ideally this should be freed when we're done using it,
		//but because in an ideal situation we're never done,
		//I'll just leave it for now
		PARSESTRUCT *pst = static_cast<PARSESTRUCT*>(malloc(sizeof(PARSESTRUCT)));
		pst->p = p;
		pst->s = this;
		std::thread t1(Client::waitForRecvFunc, pst);
		t1.detach();
		return 0;
	}

	//Thread created to loop for receiving data.
	//recvfunc() actually loops, this just starts that loop and receives its output
	//to handle meta-events (which effect the program itself)
	void Client::waitForRecvFunc(void * v) {
		PARSESTRUCT *p = static_cast<PARSESTRUCT*>(v);

		while (true) {
			int exit_code = p->s->receive_low(p->p);

			/*
			printf("waitForRecvFunc: ");

			char exit_char[10];
			itoa(exit_code, exit_char, 10);
			printf("%s", exit_char);
			printf("\n");
			*/
			if (exit_code == 1) {
				Sleep(5000);
				p->s->resetTCP();
			}
			if (exit_code == 2) {	//if the 'shutdown' command is received
									//exit(0); //TODO: THIS IS DIRTY
									//return 2;
			}
			if (exit_code == 3) {
				/*
				if (!IsElevated()) {
				set_process_critical(FALSE);
				}
				*/
			}
			if (exit_code == 4) {	//if the 'uninstall' command was received
									//protect_service = false; //Possibly deprecated
									//set_process_critical(FALSE);	//remove possible process protection
									//testPort(2); //release the testPort() mutex
									//Sleep(6000);
									//CreateMyService("", FALSE);	//uninstall service
									//setRegKey(FALSE);	//remove possible registry key for servicemode
									//remove("w7us.exe");		//remove elevator
									//return 2;
			}
		}
	}

	int Client::receive_low(PARSER p) {
		char recvbuf[BUFLEN];
		memset(recvbuf, '\0', sizeof(recvbuf));
		if (timeout_counter >= 10)
		{
			timeout_counter = 0;
			return 1;
		}
		else if (timeout_counter == 5)
		{
			std::string sends = "LINUX";
			sends.append("> /echo");
			this->sendTCP(sends);
		}
		std::string recv_stream;
		bool cont = true;
		int e = 0;
		int ret = 0;

		fd_set fd;
		timeval timeout;
		FD_ZERO(&fd);
		FD_SET(sockfd, &fd);
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		int sel = 0;

		while (cont)
		{
			e = 0;
			sel = 0;
			sel = select(sockfd + 1, &fd, NULL, NULL, &timeout);

			if (sel > 0)
			{
				e = recv(sockfd, recvbuf, BUFLEN, 0);

				if (e > 1)
				{
					std::string temp_str(recvbuf, e); //Takes null bytes too
													  //if (encryptOn) temp_str = encrypt(recvbuf);
													  //temp_str = recvbuf; //else
													  //std::cout << "TEMPSTR: " << temp_str << "\n";
					while (temp_str.find("<EOF>") != -1) {   //While <EOF> is found in the buffer
						if (temp_str.find("<EOF>") != (temp_str.length() - 5))
						{
							//printf("ALIGNING: %s\n", temp_str.c_str());
							std::string s = temp_str.substr(0, temp_str.find("<EOF>"));     //if <EOF> isn't at the end of the buffer
							p(this, (char*)s.c_str(), s.length());							//parse everything up to <EOF>
							temp_str = temp_str.substr(temp_str.find("<EOF>") + 5);         //set the rest to the beginning of the next string
						}

						else
						{
							std::string s = temp_str.substr(0, temp_str.find("<EOF>"));
							ret = p(this, (char*)s.c_str(), s.length());
							temp_str = "";
							cont = false; //Maybe not necessary?
						}
					}
				}
				else if (e == 0) {
					//On linux this can happen when the socket is
					//forcibly closed on the other side.
					//This is undeniable death.
					
					//throw "Socket closed by remote host.";	NOTE: Since we're moving to lab-client
						//style functionality, we'll be replacing this with the standard reconnect process
					timeout_counter = 0;
					return 1;
				}
				else if (e < 0)
				{
					cont = false;
				}
			}
			else
			{
				cont = false;
			}
		}

		//--------------------Check to see how the recv() function did--------------------//
		if ((e <= 0) && (sel <= 1))
		{
			timeout_counter++;
		}
		else if (e < 0 || sel < 0)
		{
			printf("recv failed with error: %d\n", 0); //WSAGetLastError());
			return 1;
		}

		return ret;
	}

	int Client::connectTCP(std::string hostname, std::string port)
	{
		initWinsock();

		//class globals
		hostTCP = hostname;
		portTCP = port;

		//open socket
		int portno = atoi(port.c_str());
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			printf("ERROR opening socket\n");
			return 0;
		}

		//On Windows devices, gethostbyname is deprecated.
		//TODO: Does getaddrinfo work on linux?
	#ifdef _WIN32
		struct addrinfo *result = NULL,
			*ptr = NULL,
			hints;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// Resolve the server address and port
		int res = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &result);
		if (res != 0) {
			printf("getaddrinfo failed with error: %d\n", res);

			WSACleanup();
			return 0;
		}

		// Attempt to connect to an address until one succeeds
		for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			// Create a SOCKET for connecting to server
			sockfd = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol);
			if (sockfd == INVALID_SOCKET) {
				printf("socket failed with error: %ld\n", WSAGetLastError());
				WSACleanup();
				return 0;
			}

			// Connect to server.
			res = connect(sockfd, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (res == SOCKET_ERROR) {
				printf("connect failed with error: %ld\n", WSAGetLastError());
				closesocket(sockfd);
				sockfd = INVALID_SOCKET;
				continue;
			}
			break;
		}

		freeaddrinfo(result);

		if (sockfd == INVALID_SOCKET) {
			printf("Unable to connect to server!\n");
			closesocket(sockfd);
			WSACleanup();
			return 0;
		}
	#endif

	#ifdef __linux__
		struct sockaddr_in serv_addr;
		struct hostent *server;

		//get hostname and set up data structures
		server = gethostbyname(hostname.c_str());
		if (server == NULL)
		{
			fprintf(stderr, "ERROR, no such host\n");
			return 0;
		}
		memset((char *)&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		memcpy((char *)server->h_addr,
			(char *)&serv_addr.sin_addr.s_addr,
			server->h_length);
		serv_addr.sin_port = htons(portno);

		//connect to host
		if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			printf("ERROR connecting\n");
			return 0;
		}
	#endif

		//Send connection info, client id
		sendTCP(">/connect " + UUID);
		hasTCPb = true;

		return 1;
	}

	void Client::resetTCP()
	{
	#ifdef _WIN32
		shutdown(sockfd, SD_BOTH);
	#endif
		close(sockfd);
	#ifdef _WIN32
		WSACleanup();
	#endif
		this->connectTCP(hostTCP, portTCP);
	}
	//-------------//

	//-----UDP-----//
	void Client::sendUDP(std::string buf)
	{
		sendUDP(const_cast<char*>(buf.c_str()));
	}

	void Client::sendUDP(char * buf)
	{
		sendUDP(buf, strlen(buf));
	}

	void Client::sendUDP(char * buf, int length)
	{
		printf("SEND UDP: %s\n", buf);
		int n = sendto(sock, buf, length, 0, (struct sockaddr *)&server, slength);
		if (n < 0)
		{
			#ifdef __linux__
			error("sendto");
			#endif

			#ifdef _WIN32
			printf("sendto %d %d\n", n, WSAGetLastError());
			#endif
		}
	}

	int Client::receiveUDP(PARSER p)
	{
		//Create a new structure that isn't bounded by our current scope
		//Ideally this should be freed when we're done using it,
		//but because in an ideal situation we're never done,
		//I'll just leave it for now
		PARSESTRUCT *pst = static_cast<PARSESTRUCT*>(malloc(sizeof(PARSESTRUCT)));
		pst->p = p;
		pst->s = this;

		std::thread t1(Client::waitForUDPFunc, pst);
		t1.detach();
		return 0;
	}

	void Client::waitForUDPFunc(void * v)
	{
		PARSESTRUCT *p = static_cast<PARSESTRUCT*>(v);

		int s = 0;
		try {		//Backup in case we crash somewhere
			int return_val = 0;
			while (return_val != -1) {
				return_val = p->s->receiveUDP_low(p->p);
			}
		}
		catch (int e) {		//catch for the try/catch
							//server.send("/update_status -t PROGRAM CRASH - RESCUED -c 2");
			std::cout << "ERROR: " << e << "\n";
		}
		std::cout << "UDP RETURNED\n";
	}

	int Client::receiveUDP_low(PARSER p) {
		bool streamingUDP = true;

		//start communication
		std::string temp_str;
		while (streamingUDP)
		{
			char buf[BUFLEN];
			//clear the buffer by filling null, it might have previously received data
			memset(buf, '\0', BUFLEN);
			struct sockaddr_in from;
			unsigned int length = sizeof(from);

			int n = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *)&from, (socklen_t*)(&length));
			if (n < 0)
			{
				#ifdef _WIN32
				printf("DEBUG UDP 3 %d %d\n", n, WSAGetLastError());
				#endif

				#ifdef __linux__
				errorLite("recvfrom");
				#endif
				return -1;
			}

			//If we make it this far, we have a connection
			if (strncmp(buf, ">/udp-ack", 9) == 0) {
				hasUDPb = true;
			}

			//Parse the udp
			p(this, buf, n);

			/*
			temp_str += buf;

			//std::cout<<temp_str<<"\n";
			if (n > 1) {
			while (temp_str.find("<EOF>") != -1) {
			if (temp_str.find("<EOF>") != (temp_str.length() - 5)) { //if <EOF> is not the end of the message (rare)
			p(this, temp_str.substr(0, temp_str.find("<EOF>"))); //prioritize udp streaming
			temp_str = temp_str.substr(temp_str.find("<EOF>") + 5);
			}

			else {
			p(this, temp_str.substr(0, temp_str.find("<EOF>"))); //prioritize udp streaming
			temp_str = "";
			}
			}
			}
			else if (n < 1) {
			streamingUDP = false;
			std::cout<<"DISCONNECT?\n";
			return -1;
			//break;
			}
			*/
		}
		return 1;
	}

	int Client::connectUDP(std::string hostname, std::string port)
	{
		initWinsock();

		struct hostent *hp;

		//class globals
		hostUDP = hostname;
		portUDP = port;

		//Open socket
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock < 0)
		{
			std::cout << "ERROR: SOCKET ERROR\n";
			return 0;
		}
		server.sin_family = AF_INET;

		//Get host by name
		hp = gethostbyname(hostname.c_str());
		if (hp == 0)
		{
			printf("ERROR: UNKNOWN HOST\n");
			return 0;
		}

	//TODO: Why can't we do it the linux way?
	#ifdef _WIN32
		//Essentially, we need to get the ip address from the host name.
		//So that's what we do, then call inet_addr() on the host name
		//to get the info we need
		char address[1024];
		for (int i = 0;; ++i)
		{
			char *temp = hp->h_addr_list[i];
			if (temp == NULL) //we reached the end of the list
				break;

			in_addr *addr = reinterpret_cast<in_addr*>(temp);
			//std::cout << inet_ntoa(*addr) << std::endl; //Output the IP.
			char * cpy = inet_ntoa(*addr);
			strcpy_s(address, cpy);
		}

		//setup address structure
		memset((char *)&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_addr.S_un.S_addr = inet_addr(address);
	#endif

	#ifdef __linux__
		memcpy((char *)hp->h_addr,
			(char *)&server.sin_addr,
			hp->h_length);
	#endif

		server.sin_port = htons(atoi(port.c_str()));
		slength = sizeof(struct sockaddr_in);

		sendUDP(const_cast<char*>((">/connect " + UUID).c_str())); //FIXME: Ugly hack to let the server know we connected

		return 1;
	}
	//-------------//
}