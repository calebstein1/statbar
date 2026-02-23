#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include <poll.h>

#define logerr(msg) \
	do \
	{ \
		if (logfile_open) \
			(void)fprintf(logfile, "%s: %s\n", msg, strerror(errno)); \
		else \
			perror(msg); \
	} while (0)

extern FILE *logfile;
extern bool logfile_open;

/* Glyphs */
extern char mail_glyph[5];
extern char volume_glyph[5];
extern char battery_glyph[5];
extern char battery_mid_glyph[5];
extern char plug_glyph[5];
extern char battery_low_glyph[5];
extern char unknown_glyph[5];

extern void init_glyphs(void);

/* Clock */
extern char clock_string[26];

extern void get_clock(void);

/* Battery */
extern char battery_string[11];

extern bool init_battery(void);
extern void get_battery(void);
extern void close_battery(void);

/* Volume */
extern char volume_string[11];

extern bool init_volume(int *nfds);
extern int get_volume_nev(struct pollfd *pfd);
extern void process_volume_events(struct pollfd *pfd, bool *hdl_open);
extern void close_volume(void);

/* Mail */
extern char mail_string[5];

extern bool get_mail(void);
extern char **get_mail_path_ptr(void);
extern void close_mail(void);

/* Weather */
extern char weather_string[48];

extern void get_weather(bool *weather_dirty);
extern char **get_weather_location_ptr(void);
extern void close_weather();

