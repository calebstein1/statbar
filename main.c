#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <machine/apmvar.h>
#include <sys/ioctl.h>

#include <X11/Xlib.h>

#define MAX_STATBAR_LEN 64
#define TICK_INTERVAL 100000000L

static bool shutdown = false;

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

void
get_battery(char *battery_string, int fd)
{
	struct apm_power_info pinfo;
	char pstate;

	if (ioctl(fd, APM_IOC_GETPOWER, &pinfo) < 0)
	{
		perror("ioctl");
		strcpy(battery_string, "Bat: ?");

		return;
	}

	switch (pinfo.battery_state)
	{
		case 2:
			pstate = '!';
			break;
		case 3:
			pstate = '+';
			break;
		default:
			pstate = ' ';
	}
	snprintf(battery_string, 10, "Bat: %3d%c", pinfo.battery_life, pstate);
}

void
shutdown_handler(int sig, siginfo_t *info, void *context)
{
	(void)sig;
	(void)info;
	(void)context;

	shutdown = true;
}

void
install_signal_handlers(void)
{
	struct sigaction shutdown;

      shutdown.sa_sigaction = shutdown_handler;
      shutdown.sa_flags = SA_SIGINFO | SA_RESTART;
      (void)sigemptyset(&shutdown.sa_mask);

      if (sigaction(SIGTERM, &shutdown, NULL) == -1) perror("sigaction");
      if (sigaction(SIGINT, &shutdown, NULL) == -1) perror("sigaction");
}

int
main(void)
{
	Display *display = XOpenDisplay(NULL);
	Window root;
	char statbar_text[MAX_STATBAR_LEN];
	char clock_string[26];
	char battery_string[10];
	bool dirty = true;
	bool apm_open = false;
	unsigned long tick = 0;
	unsigned long clock_last_updated = 0;
	unsigned long battery_last_updated = 0;
	struct timespec timeout = { .tv_nsec = TICK_INTERVAL };
	struct timespec remain;
	int apm_fd;

	if (display == NULL)
	{
		(void)fputs("Failed to get display\n", stderr);
		return 1;
	}
	install_signal_handlers();

	statbar_text[0] = '\0';
	root = DefaultRootWindow(display);

	apm_fd = open("/dev/apm", O_RDONLY);
	if (apm_fd < 0)
	{
		perror("open");
		(void)strcpy(battery_string, "Bat: ?");
	}
	else
	{
		get_battery(battery_string, apm_fd);
		apm_open = true;
	}
	get_clock(clock_string);

	while (!shutdown)
	{
		/* Clock */
		if (tick >= clock_last_updated + 9)
		{
			get_clock(clock_string);
			clock_last_updated = tick;
			dirty = true;
		}

		/* Battery */
		if (apm_open && tick >= battery_last_updated + 49)
		{
			get_battery(battery_string, apm_fd);
			battery_last_updated = tick;
			dirty = true;
		}

		if (dirty)
		{
			(void)snprintf(statbar_text, MAX_STATBAR_LEN, "%s | %s",
				battery_string,
				clock_string);
			XStoreName(display, root, statbar_text);
			XFlush(display);
			dirty = false;
		}
		if (nanosleep(&timeout, &remain) < 0 && errno != EINTR)
		{
			perror("nanosleep");

			return 1;
		}
		while (remain.tv_nsec > 0)
		{
			if (nanosleep(&remain, &remain) < 0 && errno != EINTR)
			{
				perror("nanosleep");

				return 1;
			}
		}
		tick++;
	}

	if (apm_open) (void)close(apm_fd);

	return 0;
}

