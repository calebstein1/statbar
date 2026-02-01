#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>

#define MAX_STATBAR_LEN 64
#define TICK_INTERVAL 100000000L

void
get_clock(char *clock_string)
{
	time_t current_time;
	char *nline;

	current_time = time(NULL);
	if (ctime_r(&current_time, clock_string) == NULL) return;

	nline = strchr(clock_string, '\n');
	if (nline) *nline = '\0';
}

int
main(void)
{
	Display *display = XOpenDisplay(NULL);
	Window root;
	char statbar_text[MAX_STATBAR_LEN];
	char clock_string[26];
	bool dirty = false;
	unsigned long tick = 0;
	unsigned long clock_last_updated = 0;
	struct timespec timeout = { .tv_nsec = TICK_INTERVAL };

	if (display == NULL)
	{
		(void)fputs("Failed to get display\n", stderr);
		return 1;
	}

	statbar_text[0] = '\0';
	root = DefaultRootWindow(display);

	for (;;)
	{
		if (nanosleep(&timeout, NULL) < 0 && errno != EINTR)
		{
			perror("nanosleep");

			return 1;
		}
		tick++;

		/* Clock */
		if (tick >= clock_last_updated + 9)
		{
			get_clock(clock_string);
			(void)snprintf(statbar_text, MAX_STATBAR_LEN, "%s", clock_string);
			clock_last_updated = tick;
			dirty = true;
		}

		if (!dirty) continue;

		XStoreName(display, root, statbar_text);
		XFlush(display);
		dirty = false;
	}

	return 0;
}

