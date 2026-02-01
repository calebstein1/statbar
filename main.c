#include <stdio.h>

#include <X11/Xlib.h>

#define MAX_STATBAR_LEN 64

int
main(void)
{
	Display *display = XOpenDisplay(NULL);
	Window root;
	char statbar_text[MAX_STATBAR_LEN];

	if (display == NULL)
	{
		(void)fputs("Failed to get display\n", stderr);
		return 1;
	}

	root = DefaultRootWindow(display);
	XStoreName(display, root, "Hello, world");

	XFlush(display);

	return 0;
}

