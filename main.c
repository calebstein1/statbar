#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <machine/apmvar.h>
#include <sys/ioctl.h>

#include <sndio.h>
#include <X11/Xlib.h>

#define MAX_STATBAR_LEN 64
#define TICK_INTERVAL 100000000L

static pthread_t volume_thread;
static bool shutdown = false;
static bool dirty = true;

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

	if (pinfo.ac_state == 1)
		pstate = '+';
	else if (pinfo.battery_state == 2)
		pstate = '!';
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

void *
volume_poll(void *arg)
{
	static struct sioctl_hdl *hdl;
	static struct pollfd *pfd;
	char *volume_string = arg;
	int nfds;
	int nev;

	hdl = sioctl_open(SIO_DEVANY, SIOCTL_READ, 0);
	if (hdl == NULL)
	{
		(void)fputs("Could not open sndio\n", stderr);
		(void)strcpy(volume_string, "Vol: ?");
		pthread_exit(NULL);
	}

	nfds = sioctl_nfds(hdl);
	pfd = malloc(nfds * sizeof(struct pollfd));
	if (pfd == NULL)
	{
		perror("malloc");
		pthread_exit(NULL);
	}
	(void)sioctl_ondesc(hdl, init_volume, volume_string);
	(void)sioctl_onval(hdl, get_volume, volume_string);

	while (!shutdown)
	{
		nev = sioctl_pollfd(hdl, pfd, POLLIN);

		if (poll(pfd, nev, -1) > 0)
		{
			if (sioctl_revents(hdl, pfd) & POLLHUP)
				goto cleanup;

			dirty = true;
		}
	}

cleanup:
	sioctl_close(hdl);
	free(pfd);

	return NULL;
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
	struct sigaction volume;

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
	char volume_string[10];
	bool apm_open = false;
	unsigned long tick = 0;
	unsigned long clock_last_updated = 0;
	unsigned long battery_last_updated = 0;
	struct timespec timeout = { .tv_nsec = TICK_INTERVAL };
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
	if (pthread_create(&volume_thread, NULL, volume_poll, volume_string) != 0)
		(void)fputs("Failed to create volume thread\n", stderr);
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
		if (apm_open && tick >= battery_last_updated + 99)
		{
			get_battery(battery_string, apm_fd);
			battery_last_updated = tick;
			dirty = true;
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
		if (nanosleep(&timeout, NULL) < 0 && errno != EINTR)
		{
			perror("nanosleep");

			return 1;
		}
		tick++;
	}

	if (apm_open) (void)close(apm_fd);

	return 0;
}

