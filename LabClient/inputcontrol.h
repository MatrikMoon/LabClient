#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>

void SetCursorPos(int x, int y);
#endif

void m_mouseDown();
void m_rmouseDown();
void m_mouseUp();
void m_rmouseUp();