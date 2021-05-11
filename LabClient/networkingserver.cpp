#include "networking.h"
#include "utils.h"

#ifdef _WIN32
#define close closesocket
#endif

namespace MoonNetworking {
    //Static list of all Clients, just in case we need to single out one
    //for an action
    std::vector<Server*> Server::clientList;

    //Both
    bool Server::equals(Server c) {
        return this->UUID == c.UUID;
    }

    std::string Server::getUUID() {
        return UUID;
    }

    void Server::setUUID(std::string id) {
        UUID = id;
    }

    void Server::attachTCP(int s) {
        sockfd = s;
        hasTCPb = true; //Set indicator to say we have tcp on this Server
        printf("Server: ATTACH TCP: %s\n", UUID.c_str());
    }

    void Server::attachUDP(int s, struct sockaddr_in a, socklen_t length) {
        sock = s;
        from = a;
        fromlen = length;
        hasUDPb = true; //Set indicator to say we have udp on this Server
        printf("Server: ATTACH UDP: %s\n", UUID.c_str());

        //Acknowledge the attachment by sending any data at all
        sendUDP(const_cast<char*>(">/udp-ack"));
    }

    bool Server::hasTCP() {
        return hasTCPb;
    }

    bool Server::hasUDP() {
        return hasUDPb;
    }

    void Server::setHasTCP(bool b) {
        hasTCPb = b;
    }

    void Server::setHasUDP(bool b) {
        hasUDPb = b;
    }

    Server::~Server() {
        printf("Server: Client with UUID: %s and sockfd: %d destroyed.\n", UUID.c_str(), sockfd);
    }

    //TCP
    Server::Server(int s)
    {
        sockfd = s;
        hasTCPb = true;
        hasUDPb = false; //No udp to be seen if we're in this constructor
    }

    void Server::broadcastTCP(std::string buf)
    {
        for (unsigned int i = 0; i < clientList.size(); i++) {
            if (clientList.at(i)->hasTCP()) {
                clientList.at(i)->sendTCP(buf);
            }
        }
    }

    void Server::broadcastUDP(std::string buf)
    {
        for (unsigned int i = 0; i < clientList.size(); i++) {
            if (clientList.at(i)->hasUDP()) {
                clientList.at(i)->sendUDP(buf);
            }
        }
    }

    void Server::sendTCP(std::string buf)
    {
        sendTCP(const_cast<char*>(buf.c_str()));    
    }

    void Server::sendTCP(char * buf) {
        sendTCP(buf, strlen(buf));
    }

    void Server::sendTCP(char * buf, int length) {
        //Append <EOF> to the end.
        char * sends = new char[length + 6]; //+6 for <EOF>\0
        memcpy(sends, buf, length); //Memcpy just in case of null bytes
        strcpy_s(&sends[length], length + 6, "<EOF>\0"); //Copy it over

        printf("SEND TCP: %s\n", sends);

        int n = send(sockfd, sends, length + 6, 0);
        if (n < 0) {
            error("Error writing to socket.");
        }
    }

    int Server::listenTCP(PARSER p, std::string port) {
		initWinsock();

        //Create a new structure that isn't bounded by our current scope
        //Ideally this should be freed when we're done using it,
        //but because in an ideal situation we're never done,
        //I'll just leave it for now
        PARSESTRUCT *pst = static_cast<PARSESTRUCT*>(malloc(sizeof(PARSESTRUCT)));
        pst->p = p;
        pst->TCPPort = atoi(port.c_str());
        
		std::thread t1(Server::startListeningTCP, pst);
		t1.detach();
        return 0;
    }

	void Server::startListeningTCP(void *v)
	{
		PARSESTRUCT *p = static_cast<PARSESTRUCT*>(v);

		struct sockaddr_in serv_addr, cli_addr;
		socklen_t clilen = sizeof(cli_addr);

		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			#ifdef __linux__
			error("socket tcp");
			#endif

			#ifdef _WIN32
			printf("socket tcp %d\n", WSAGetLastError());
			#endif
			return;
		}

		memset((char *)&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(p->TCPPort);

		if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			#ifdef __linux__
			error("bind tcp");
			#endif

			#ifdef _WIN32
			printf("bind tcp %d\n", WSAGetLastError());
			#endif

		}
		int n = listen(sockfd, 0x7fffffff);
		if (n == -1) {
			
			
            #ifdef __linux__
			error("listen tcp");
			#endif

			#ifdef _WIN32
            printf("listen failed with error: %d\n", WSAGetLastError());
			WSACleanup();
			#endif

            close(sockfd);
			return;
		}

		//Keep accepting new Clients
		while (true) {
			int newsockfd = accept(sockfd,
				(struct sockaddr *)&cli_addr,
				&clilen);
			if (newsockfd < 0) {
				#ifdef __linux__
				error("accept tcp");
				#endif

				#ifdef _WIN32
				printf("accept tcp %d %d\n", n, WSAGetLastError());
				#endif
			}

			//Add our Client to the list
			Server * c = new Server(newsockfd);
			//clientList.push_back(c); //Don't push until we get an ID

			//Start listener for our new Client
			//Create a new structure that isn't bounded by our current scope
			//Ideally this should be freed when we're done using it,
			//but because in an ideal situation we're never done,
			//I'll just leave it for now
			PARSESTRUCT *pst = static_cast<PARSESTRUCT*>(malloc(sizeof(PARSESTRUCT)));
			pst->p = p->p;
			pst->c = c;

			std::thread t1(listen_tcp_low, pst);
			t1.detach();
		}
	}

    //Helper function so we can avoid having a nasty
    //repetitive few lines of code in listen_tcp_low()
	//This smart lil guy ties new tcp connections to existing udp connections
    void Server::internalTCPParser(void * v, std::string s) {
        PARSESTRUCT *p = static_cast<PARSESTRUCT*>(v);
        if (strncmp(s.c_str(), ">/connect ", 10) == 0) {
            //See if the client already exists as a UDP connection
            //If so, attach to it and clean up our mess
            std::string UUID = &(s.c_str()[10]);
            for (unsigned int i = 0; i < clientList.size(); i++) {
                if (!clientList.at(i)->hasTCP() && (clientList.at(i)->getUUID() == UUID)) {
                    clientList.at(i)->attachTCP(p->c->sockfd);
                    delete p->c;
                    p->c = clientList.at(i);
                    return;
                }
                else if (clientList.at(i)->hasTCP() && (clientList.at(i)->getUUID() == UUID)) {
                    return; //This Server is already connected, we have nothing to do here
                }
            }
            //If we make it here, this is a new Server connecting
            clientList.push_back(p->c);
            p->c->setUUID(UUID); //Set our uuid
        }
        p->p(p->c, (char*)s.c_str(), s.length());   //parse everything up to <EOF>
    }


    //TODO: Rewrite this to accept null bytes from the socket, like UDP
    void Server::listen_tcp_low(void * v) {
        PARSESTRUCT *p = static_cast<PARSESTRUCT*>(v);

        //Receive from a single
        while (true)
        {
            char buffer[BUFLEN];
            memset(buffer, 0, BUFLEN);

            //Recieve
            int n = recv(p->c->sockfd, buffer, BUFLEN, 0);
            if (n <= 0)
            {
                printf("Error reading from socket.\n");
                break; //Break out and remove Client from list
            }

            //p->p(p->c, buffer, n);

            //Break up the chunks of streams by <EOF>
            std::string temp_str(buffer, n); //Takes null bytes too
            while (temp_str.find("<EOF>") != -1)
            {   //While <EOF> is found in the buffer
                //std::cout << "LOOP: " << loopcount << " DATA: " << temp_str << "\n";

                if (temp_str.find("<EOF>") != (temp_str.length() - 5))
                {
                    //printf("ALIGNING\n");
                    std::string s = temp_str.substr(0, temp_str.find("<EOF>"));                                                       //if <EOF> isn't at the end of the buffer
                    internalTCPParser(v, s);
                    temp_str = temp_str.substr(temp_str.find("<EOF>") + 5);           //set the rest to the beginning of the next string
                }

                else
                {
                    std::string s = temp_str.substr(0, temp_str.find("<EOF>"));
                    internalTCPParser(v, s);
                    temp_str = ""; //Set string back to empty
                }
            }
        }

        close(p->c->sockfd); //Close socket

        //We've closed the socket! Remove from us from the list.
        for (unsigned int i = 0; i < clientList.size(); i++) {
            //If we have a UUID
            if (p->c->getUUID() != "") {
                if (clientList.at(i)->getUUID() == p->c->getUUID()) {
                    //Remove the Server from the list
                    //Notice we don't check for an existing UDP
                    //connection. This is because there's no
                    //way to tell if a UDP connection is alive,
                    //and if there was one, it's probably dead
                    //too if the TCP one is.
                    delete clientList.at(i); //Delete object
                    clientList.erase(clientList.begin() + i); //Remove element
                    free(p); //Free malloc'd struct
                    break;
                }
            }
            //No UUID? Good news! We're not in the list yet
            //If we don't have a UUID we can't have a UDP
            //counterpart anyway, so no worries.
            else {
                delete p->c; //Delete object
                free(p); //Free malloc'd struct
                break;
            }
        }
    }

    //UDP
    Server::Server(int s, sockaddr_in a, socklen_t length)
    {
        sock = s;
        from = a;
        fromlen = length;
        hasUDPb = true;
        hasTCPb = false; //If we're in this constructor, we don't have a tcp connection yet
    }

    void Server::sendUDP(std::string buf)
    {
        sendUDP((char*)buf.c_str());
    }

    void Server::sendUDP(char * buf)
    {
        sendUDP(buf, strlen(buf));
    }

    void Server::sendUDP(char * buf, int length)
    {
        printf("SEND UDP: %s\n", buf);
        int n = sendto(sock, buf, length, 0, (struct sockaddr *)&from, fromlen);
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

    int Server::listenUDP(PARSER p, std::string port) {
		initWinsock();

        //Create a new structure that isn't bounded by our current scope
        //Ideally this should be freed when we're done using it,
        //but because in an ideal situation we're never done,
        //I'll just leave it for now
        PARSESTRUCT *pst = static_cast<PARSESTRUCT*>(malloc(sizeof(PARSESTRUCT)));
        pst->p = p;
        pst->UDPPort = atoi(port.c_str());
        
		std::thread t1(startListeningUDP, pst);
		t1.detach();
        return 0;
    }

    void Server::startListeningUDP(void *v)
    {
        PARSESTRUCT *p = static_cast<PARSESTRUCT*>(v);
        int length, n;
        struct sockaddr_in server;
        char buf[BUFLEN];

        //Open socket instance
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0)
        {
			#ifdef __linux__
			error("Opening socket");
			#endif

			#ifdef _WIN32
			printf("Opening socketS %d %d\n", sock, WSAGetLastError());
			#endif
        }
        length = sizeof(server);
        memset(&server, 0, length);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(p->UDPPort);

        //Bind to socket
		if (bind(sock, (struct sockaddr *)&server, length) < 0) {
			#ifdef __linux__
			error("binding udp");
			#endif

			#ifdef _WIN32
			printf("binding udp %d\n", WSAGetLastError());
			#endif
		}
        socklen_t fromlen = sizeof(struct sockaddr_in);

        //Receive from various Clients
        while (true)
        {
            //Recieve
            struct sockaddr_in from;
            n = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *)&from, &fromlen);
            if (n <= 0)
            {
                //TODO: Remove Server from list... maybe?
                error("recvfrom");
            }
			
			//If this isn't a full size message, let's be nice and add a \0 to the end
			if (n < BUFLEN) {
				buf[n] = '\0';
			}

            //Pointer to the Client
            Server *c;

            bool connection_message = false;
            std::string UUID;
            //If this is a connection message...
            if (strncmp(buf, ">/connect ", 10) == 0) {
                connection_message = true;
				UUID = &buf[10];
            }

			//TODO: This is a time complexity hog. Might be worth looking into tagging
			//every udp message with a UUID instead of tying it through address identification
			//--------
            //Add the udp client to the list if it's not already there
            bool exists = false;
            for (unsigned int i = 0; i < clientList.size(); i++) {
                //If the currently tested client has udp
                if (clientList.at(i)->hasUDP()) {
                    //Compare addresses of existing clients and the new one
                    if ((clientList.at(i)->from.sin_addr.s_addr == from.sin_addr.s_addr) && 
                        (clientList.at(i)->from.sin_port == from.sin_port)) {
                        //Ah, so we exist. Delete the newly created object
                        //and replace it with the existing one.
                        exists = true;
                        c = clientList.at(i);
                    }
                }
                //If this is a connection message, let's check to see
                //if the connecting Server already has a TCP counterpart
                else if (connection_message) {
                    if (clientList.at(i)->getUUID() == UUID) {
                        exists = true;
                        c = clientList.at(i);
                        c->attachUDP(sock, from, fromlen);
                    }
                }
            }
            //If it's none of the above, it's a new connection with a UUID
            //Add it to the list. If it's not this either, it's SOL
            if (!exists && connection_message) {
                c = new Server(sock, from, fromlen);
                clientList.push_back(c);
                c->setUUID(UUID);
            }

            if (c) {
                p->p(c, buf, n);
            }

            memset(&buf, 0, BUFLEN);
            memset(&from, 0, sizeof(sockaddr_in));
        }
    }
}