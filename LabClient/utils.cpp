#include "utils.h"

#ifdef _WIN32
//Generate UUID's for identifying us as a client
std::string uuid_generate() {
	UUID uuid;
	UuidCreate(&uuid);

	WCHAR* wszUuid = NULL;
	UuidToStringW(&uuid, (RPC_WSTR*)&wszUuid);

	std::wstring uuid_s = wszUuid;

	if (wszUuid != NULL) {
		RpcStringFree((RPC_WSTR*)&wszUuid);
	}

	return wstoms(uuid_s);
}

//Open the disc tray
void openDiscTray() {
	mciSendStringA("open CDAudio", NULL, 0, NULL);
	mciSendStringA("set CDAudio door open", NULL, 0, NULL);
	mciSendStringA("close CDAudio", NULL, 0, NULL);
}

//Close the disc tray
void closeDiscTray() {
	mciSendStringA("open CDAudio", NULL, 0, NULL);
	mciSendStringA("set CDAudio door closed", NULL, 0, NULL);
	mciSendStringA("close CDAudio", NULL, 0, NULL);
}

// Get the horizontal and vertical screen sizes
void GetDesktopResolution(int& horizontal, int& vertical) {
	RECT desktop;

	HWND hDesktop = GetDesktopWindow();

	GetWindowRect(hDesktop, &desktop);

	horizontal = desktop.right;
	vertical = desktop.bottom;
}

std::string getCurrentExecutablePath() {
	char pathe[MAX_PATH];
	GetModuleFileNameA(NULL, pathe, MAX_PATH);
	return std::string(pathe);
}

std::string getLocalIp() {
	DWORD rv, size;
	PIP_ADAPTER_ADDRESSES adapter_addresses;
	PIP_ADAPTER_UNICAST_ADDRESS ua;

	rv = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &size);
	if (rv != ERROR_BUFFER_OVERFLOW) {
		fprintf(stderr, "GetAdaptersAddresses() failed...");
		return false;
	}
	adapter_addresses = (PIP_ADAPTER_ADDRESSES)malloc(size);

	rv = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, adapter_addresses, &size);
	if (rv != ERROR_SUCCESS) {
		fprintf(stderr, "GetAdaptersAddresses() failed...");
		free(adapter_addresses);
		return false;
	}
	char buf[BUFSIZ];
	ua = adapter_addresses->FirstUnicastAddress;
	memset(buf, 0, BUFSIZ);
	getnameinfo(ua->Address.lpSockaddr, ua->Address.iSockaddrLength, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
	return std::string(buf);
}
#endif

#ifdef __linux__
//Generate UUID's for identifying us as a client
std::string uuid_generate() {
	uuid_t id;
	uuid_generate(id);
	char * uuid_ptr = new char[37];
	uuid_unparse(id, uuid_ptr);
	std::string ret = uuid_ptr;
	
	delete uuid_ptr;
	return ret;
}

//wcstombs_s hack-port to make linux compiling easier on the eyes
int wcstombs_s(size_t* convertedNum, char* dst, size_t dstSize, wchar_t const* src, size_t maxCount) {
	return wcstombs(dst, src, dstSize);
}

//strcpy_s hack-port
int strcpy_s(char* dest, int size, char const* source) {
	strcpy(dest, source);
	return 0;
}

//Sleep hack so we don't have to change the call per-OS
void Sleep(long millis) {
	usleep(millis * 1000);
}

//Open the disc tray
void openDiscTray() {
	int fd = open("/dev/cdrom", O_RDONLY | O_NONBLOCK);
	ioctl(fd, CDROMEJECT);
	close(fd);
}

//Close the disc tray
void closeDiscTray() {
	int fd = open("/dev/cdrom", O_RDONLY | O_NONBLOCK);
	ioctl(fd, CDROMCLOSETRAY);
	close(fd);
}

//Get screen resolution
void GetDesktopResolution(int& horizontal, int& vertical) {
	Display *pdsp = XOpenDisplay(NULL);

	//Bail if there's no window manager
	if (pdsp == 0) {
		horizontal = -1;
		vertical = -1;
		XCloseDisplay(pdsp);
		return;
	}

	Window wid = DefaultRootWindow(pdsp);
	XWindowAttributes xwAttr;
	XGetWindowAttributes(pdsp, wid, &xwAttr);

	horizontal = xwAttr.width;
	vertical = xwAttr.height;
	XCloseDisplay(pdsp);
}

std::string getCurrentExecutablePath() {
	char result[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	const char * pathe;
	if (count != -1) {
		pathe = dirname(result);
	}
}

std::string getLocalIp() {
	return std::string("0.0.0.0");
}
#endif

//Convert a wstring to a string
std::string wstoms(std::wstring input) {
	char * tempMS = new char[input.length() + 1];

	size_t conv = 0;
	wcstombs_s(&conv, tempMS, input.length() + 1, input.c_str(), input.length());
	std::string ret = tempMS;

	delete tempMS;
	return ret;
}

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

void errorLite(const char *msg)
{
	perror(msg);
}

/**
* C++ version 0.4 char* style "itoa":
* Written by Luk�s Chmela
* Released under GPLv3.
*/
char* _itoa_s(int value, char* result, int base) {
	// check that the base if valid
	if (base < 2 || base > 36) { *result = '\0'; return result; }

	char* ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
	} while (value);

	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr-- = *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}

//Check if a file exists, provided the path
bool file_exists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}