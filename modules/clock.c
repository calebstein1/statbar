#include <string.h>
#include <time.h>

#include "statbar.h"

char clock_string[26];

void
get_clock(void)
{
	time_t current_time;
	char *nline;

	current_time = time(NULL);
	if (ctime_r(&current_time, clock_string) == NULL) return;

	nline = strchr(clock_string, '\n');
	if (nline) *nline = '\0';
}
