#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <machine/apmvar.h>
#include <sys/ioctl.h>

#include "statbar.h"

char battery_string[11];

static int fd;

bool
init_battery(void)
{
	fd = open("/dev/apm", O_RDONLY);
	if (fd < 0)
	{
		perror("open");
		(void)strcpy(battery_string, "Bat: ?");

		return false;
	}
	get_battery();

	return true;
}

void
get_battery()
{
	struct apm_power_info pinfo;
	char *pstate;

	if (ioctl(fd, APM_IOC_GETPOWER, &pinfo) < 0)
	{
		perror("ioctl");
		strcpy(battery_string, unknown_glyph);

		return;
	}

	if (pinfo.ac_state == APM_AC_ON)
		pstate = plug_glyph;
	else if (pinfo.battery_state == APM_BATT_CRITICAL)
		pstate = battery_low_glyph;
	else if (pinfo.ac_state == APM_AC_UNKNOWN || pinfo.battery_state == APM_BATT_UNKNOWN)
		pstate = unknown_glyph;
	else
		pstate = battery_glyph;

	(void)snprintf(battery_string, 11, "%s  %3d%%", pstate, pinfo.battery_life);
}

void
close_battery(void)
{
	(void)close(fd);
}

