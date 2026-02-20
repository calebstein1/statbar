#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <X11/Xlib.h>

#include "statbar.h"

#define MAX_STATBAR_LEN 128

enum clocks_e
{
	CLOCK_CLOCK,
	BATTERY_CLOCK,
	WEATHER_CLOCK,
	MAIL_CLOCK,
	CLOCKS_COUNT
};

static volatile sig_atomic_t should_quit = 0;
static volatile sig_atomic_t reload_requested = 0;
static bool weather_loc_valid = false;
static bool mail_path_valid = false;

void
read_config(
	struct timespec *clock_interval,
	struct timespec *battery_interval,
	struct timespec *mail_interval)
{
	char *config_full_path;
	char *path_sep;
	time_t clock_ms;
	time_t battery_ms;
	uid_t uid;
	struct passwd *pw;
	int mkdir_ret;
	FILE *file;
	char *line = NULL;
	char *delim;
	char *nline;
	int val;
	int i = 0;
	size_t n = 0;

	reload_requested = 0;

	uid = getuid();
	pw = getpwuid(uid);

	if (pw == NULL)
	{
		perror("getpwuid");

		return;
	}

	if (asprintf(&config_full_path, "%s/.config/statbar/statbar.conf", pw->pw_dir) == -1)
	{
		perror("asprintf");

		return;
	}
	path_sep = config_full_path + 1;
	path_sep = strchr(path_sep, '/');
	while (path_sep)
	{
		*path_sep = '\0';
		mkdir_ret = mkdir(config_full_path, 0700);
		if (mkdir_ret != 0 && errno != EEXIST)
		{
			perror("mkdir");
			free(config_full_path);

			return;
		}
		*path_sep = '/';
		path_sep++;
		path_sep = strchr(path_sep, '/');
	}
	file = fopen(config_full_path, "r");
	if (file == NULL)
	{
		free(config_full_path);

		return;
	}
	free(config_full_path);

	while (getline(&line, &n, file) > 0)
	{
		i++;
		delim = strchr(line, ':');
		if (delim == NULL || *(delim + 1) == '\n' || *(delim + 1) == '\0')
			goto read_err;

		nline = strchr(delim, '\n');
		if (nline != NULL) *nline = '\0';

		if (strncmp(line, "weather location", strlen("weather location")) == 0)
		{
			if (weather_loc_valid && *get_weather_location_ptr() != NULL) close_weather();
			delim++;
			while (*delim == ' ') delim++;
			*get_weather_location_ptr() = strdup(delim);
			if (*get_weather_location_ptr() == NULL)
				perror("strdup");
			else
				weather_loc_valid = true;

			continue;
		}
		if (strncmp(line, "inbox directory", strlen("inbox directory")) == 0)
		{
			if (mail_path_valid && *get_mail_path_ptr() != NULL) close_mail();
			delim++;
			while (*delim == ' ') delim++;
			if (asprintf(get_mail_path_ptr(), "%s/new", delim) == -1)
				perror("asprintf");
			else
				mail_path_valid = true;

			continue;
		}

		/* 10 minute max is arbitrary */
		delim++;
		val = (int)strtonum(delim, 1, 600000, NULL);
		if (val == 0)
			goto read_err;

		if (strncmp(line, "clock interval", strlen("clock interval")) == 0)
		{
			clock_interval->tv_sec = val / 1000;
			clock_interval->tv_nsec = (val % 1000) * 1000000;
		}
		else if (strncmp(line, "battery interval", strlen("battery interval")) == 0)
		{
			battery_interval->tv_sec = val / 1000;
			battery_interval->tv_nsec = (val % 1000) * 1000000;
		}
		else if (strncmp(line, "mail interval", strlen("mail interval")) == 0)
		{
			mail_interval->tv_sec = val / 1000;
			mail_interval->tv_nsec = (val % 1000) * 1000000;
		}
		else
		{
			goto read_err;
		}

		n = 0;
		continue;

	read_err:
		(void)fprintf(stderr, "Config syntax error line %d: %s\n", i, line);
		free(line);
		(void)fclose(file);

		return;
	}
	free(line);
	if (ferror(file))
		perror("getline");
	(void)fclose(file);
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
reload_handler(int sig, siginfo_t *info, void *context)
{
	(void)sig;
	(void)info;
	(void)context;

	reload_requested = 1;
}

void
install_signal_handlers(void)
{
	struct sigaction should_quit_action;
	struct sigaction reload_requested_action;

	should_quit_action.sa_sigaction = quit_handler;
	should_quit_action.sa_flags = SA_SIGINFO | SA_RESTART;
	(void)sigemptyset(&should_quit_action.sa_mask);
	reload_requested_action.sa_sigaction = reload_handler;
	reload_requested_action.sa_flags = SA_SIGINFO | SA_RESTART;
	(void)sigemptyset(&reload_requested_action.sa_mask);

	if (sigaction(SIGTERM, &should_quit_action, NULL) == -1) perror("sigaction");
	if (sigaction(SIGINT, &should_quit_action, NULL) == -1) perror("sigaction");
	if (sigaction(SIGUSR1, &reload_requested_action, NULL) == -1) perror("sigaction");
}

int
main(void)
{
	Display *display = XOpenDisplay(NULL);
	Window root;
	char statbar_text[MAX_STATBAR_LEN];
	bool dirty = true;
	bool weather_dirty = false;
	bool apm_open = false;
	bool hdl_open = false;
	struct timespec now;
	struct timespec clocks[CLOCKS_COUNT];
	struct timespec *next_event;
	struct timespec next_interval;
	struct timespec clock_interval = { .tv_sec = 1 };
	struct timespec battery_interval = { .tv_sec = 5 };
	struct timespec weather_interval = { .tv_sec = 3600 };
	struct timespec mail_interval = { .tv_sec = 30 };
	struct pollfd *pfd;
	int nfds = 0;
	int nev;
	int i;

	if (display == NULL)
	{
		(void)fputs("Failed to get display\n", stderr);
		return 1;
	}
	install_signal_handlers();
	init_glyphs();

	statbar_text[0] = '\0';
	root = DefaultRootWindow(display);

	/* Init components */
	read_config(&clock_interval, &battery_interval, &mail_interval);
	(void)clock_gettime(CLOCK_MONOTONIC, &now);
	timespecadd(&now, &clock_interval, &clocks[CLOCK_CLOCK]);
	timespecadd(&now, &battery_interval, &clocks[BATTERY_CLOCK]);
	timespecadd(&now, &weather_interval, &clocks[WEATHER_CLOCK]);
	timespecadd(&now, &mail_interval, &clocks[MAIL_CLOCK]);

	get_clock();
	apm_open = init_battery();
	hdl_open = init_volume(&nfds);
	if (weather_loc_valid) get_weather(&weather_dirty);
	if (mail_path_valid) mail_path_valid = get_mail();

	pfd = malloc(nfds * sizeof(struct pollfd));
	if (pfd == NULL)
	{
		perror("malloc");
		goto cleanup;
	}

	while (!should_quit)
	{
		if (reload_requested) read_config(&clock_interval, &battery_interval, &mail_interval);
		if (mail_path_valid == false)
			close_mail();

		(void)clock_gettime(CLOCK_MONOTONIC, &now);
		nev = 0;
		nev += get_volume_nev(pfd);

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
			if (hdl_open)
				process_volume_events(pfd, &hdl_open);

			dirty = true;
		}
		(void)clock_gettime(CLOCK_MONOTONIC, &now);

		/* Clock */
		if (timespeccmp(&now, &clocks[CLOCK_CLOCK], >=))
		{
			get_clock();
			dirty = true;
			while (timespeccmp(&now, &clocks[CLOCK_CLOCK], >=))
				timespecadd(&clocks[CLOCK_CLOCK], &clock_interval, &clocks[CLOCK_CLOCK]);
		}
		/* Battery */
		if (apm_open && timespeccmp(&now, &clocks[BATTERY_CLOCK], >=))
		{
			get_battery();
			dirty = true;
			while (timespeccmp(&now, &clocks[BATTERY_CLOCK], >=))
				timespecadd(&clocks[BATTERY_CLOCK], &battery_interval, &clocks[BATTERY_CLOCK]);
		}

		/* Weather */
		if (weather_loc_valid && timespeccmp(&now, &clocks[WEATHER_CLOCK], >=))
		{
			get_weather(&weather_dirty);
			while (timespeccmp(&now, &clocks[WEATHER_CLOCK], >=))
				timespecadd(&clocks[WEATHER_CLOCK], &weather_interval, &clocks[WEATHER_CLOCK]);
		}
		if (weather_dirty)
		{
			dirty = true;
			weather_dirty = false;
		}

		/* Mail */
		if (mail_path_valid && timespeccmp(&now, &clocks[MAIL_CLOCK], >=))
		{
			mail_path_valid = get_mail();
			dirty = true;
			while (timespeccmp(&now, &clocks[MAIL_CLOCK], >=))
				timespecadd(&clocks[MAIL_CLOCK], &mail_interval, &clocks[MAIL_CLOCK]);
		}

		if (dirty)
		{
			(void)snprintf(statbar_text, MAX_STATBAR_LEN, "%s | %s | %s | %s | %s",
				mail_string,
				weather_string,
				volume_string,
				battery_string,
				clock_string);
			XStoreName(display, root, statbar_text);
			XFlush(display);
			dirty = false;
		}
	}

cleanup:
	if (apm_open) close_battery();
	if (hdl_open) close_volume();
	if (weather_loc_valid) close_weather();
	if (mail_path_valid) close_mail(); 
	free(pfd);

	return 0;
}

