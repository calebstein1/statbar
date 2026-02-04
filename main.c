#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <machine/apmvar.h>
#include <sys/ioctl.h>

#include <sndio.h>
#include <X11/Xlib.h>

#define MAX_STATBAR_LEN 128

enum clocks_e
{
	CLOCK_CLOCK,
	BATTERY_CLOCK,
	CLOCKS_COUNT
};

static volatile sig_atomic_t should_quit = 0;

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

	if (pinfo.ac_state == APM_AC_ON)
		pstate = '+';
	else if (pinfo.battery_state == APM_BATT_CRITICAL)
		pstate = '!';
	else if (pinfo.ac_state == APM_AC_UNKNOWN || pinfo.battery_state == APM_BATT_UNKNOWN)
		pstate = '?';
	else
		pstate = ' ';

	(void)snprintf(battery_string, 10, "Bat: %3d%c", pinfo.battery_life, pstate);
}

void
get_volume(void *arg, unsigned int addr, unsigned int val)
{
	char *volume_string = arg;

	(void)addr;
	(void)snprintf(volume_string, 10, "Vol: %3d%%", val * 100 / 255);
}

void
init_volume(void *arg, struct sioctl_desc *desc, int val)
{
	static bool did_init = false;
	char *volume_string = arg;

	if (!did_init && desc != NULL && strcmp(desc->func, "level") == 0)
	{
		(void)snprintf(volume_string, 10, "Vol: %3d%%", val * 100 / 255);
		did_init = true;
	}
}

void
quit_handler(int sig, siginfo_t *info, void *context)
{
	(void)sig;
	(void)info;
	(void)context;

	should_quit = 1;
}

void
install_signal_handlers(void)
{
	struct sigaction should_quit_action;

	should_quit_action.sa_sigaction = quit_handler;
	should_quit_action.sa_flags = SA_SIGINFO | SA_RESTART;
	(void)sigemptyset(&should_quit_action.sa_mask);

	if (sigaction(SIGTERM, &should_quit_action, NULL) == -1) perror("sigaction");
	if (sigaction(SIGINT, &should_quit_action, NULL) == -1) perror("sigaction");
}

int
main(void)
{
	Display *display = XOpenDisplay(NULL);
	Window root;
	char statbar_text[MAX_STATBAR_LEN];
	char clock_string[26];
	char battery_string[10];
	char volume_string[10];
	bool dirty = true;
	bool apm_open = false;
	bool hdl_open = false;
	struct timespec now;
	struct timespec clocks[CLOCKS_COUNT];
	struct timespec *next_event;
	struct timespec next_interval;
	struct timespec clock_interval = { .tv_sec = 1 };
	struct timespec battery_interval = { .tv_sec = 5 };
	struct sioctl_hdl *hdl;
	struct pollfd *pfd;
	int nfds = 0;
	int nev;
	int apm_fd;
	int i;

	if (display == NULL)
	{
		(void)fputs("Failed to get display\n", stderr);
		return 1;
	}
	install_signal_handlers();

	statbar_text[0] = '\0';
	root = DefaultRootWindow(display);

	/* Init components */
	(void)clock_gettime(CLOCK_MONOTONIC, &now);
	timespecadd(&now, &clock_interval, &clocks[CLOCK_CLOCK]);
	timespecadd(&now, &battery_interval, &clocks[BATTERY_CLOCK]);

	get_clock(clock_string);

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

	hdl = sioctl_open(SIO_DEVANY, SIOCTL_READ, 0);
	if (hdl == NULL)
	{
		(void)fputs("Could not open sndio\n", stderr);
		(void)strcpy(volume_string, "Vol: ?");
	}
	else
	{
		nfds += sioctl_nfds(hdl);
		(void)sioctl_ondesc(hdl, init_volume, volume_string);
		(void)sioctl_onval(hdl, get_volume, volume_string);
		hdl_open = true;
	}

	pfd = malloc(nfds * sizeof(struct pollfd));
	if (pfd == NULL)
	{
		perror("malloc");
		goto cleanup;
	}

	while (!should_quit)
	{
		(void)clock_gettime(CLOCK_MONOTONIC, &now);
		nev = sioctl_pollfd(hdl, pfd, POLLIN);

		next_event = NULL;
		for (i = 0; i < CLOCKS_COUNT; i++)
		{
			if (next_event == NULL)
			{
				next_event = &clocks[i];
			}
			else
			{
				if (timespeccmp(next_event, &clocks[i], >))
				{
					next_event = &clocks[i];
				}
			}
		}
		if (timespeccmp(&now, next_event, >=))
		{
			next_interval.tv_sec = 0;
			next_interval.tv_nsec = 0;
		}
		else
		{
			timespecsub(next_event, &now, &next_interval);
		}

		if (ppoll(pfd, nev, &next_interval, NULL) > 0)
		{
			if (hdl_open && sioctl_revents(hdl, pfd) & POLLHUP)
			{
				(void)fputs("Lost connection to sndio\n", stderr);
				sioctl_close(hdl);
				hdl_open = false;
			}

			dirty = true;
		}
		(void)clock_gettime(CLOCK_MONOTONIC, &now);

		/* Clock */
		if (timespeccmp(&now, &clocks[CLOCK_CLOCK], >=))
		{
			get_clock(clock_string);
			dirty = true;
			while (timespeccmp(&now, &clocks[CLOCK_CLOCK], >=))
				timespecadd(&clocks[CLOCK_CLOCK], &clock_interval, &clocks[CLOCK_CLOCK]);
		}
		/* Battery */
		if (apm_open && timespeccmp(&now, &clocks[BATTERY_CLOCK], >=))
		{
			get_battery(battery_string, apm_fd);
			dirty = true;
			while (timespeccmp(&now, &clocks[BATTERY_CLOCK], >=))
				timespecadd(&clocks[BATTERY_CLOCK], &battery_interval, &clocks[BATTERY_CLOCK]);
		}

		if (dirty)
		{
			(void)snprintf(statbar_text, MAX_STATBAR_LEN, "%s | %s | %s",
				volume_string,
				battery_string,
				clock_string);
			XStoreName(display, root, statbar_text);
			XFlush(display);
			dirty = false;
		}
	}

cleanup:
	if (apm_open) (void)close(apm_fd);
	if (hdl_open) sioctl_close(hdl);
	free(pfd);

	return 0;
}

