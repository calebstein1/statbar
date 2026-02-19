#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>

#include "statbar.h"

char mail_string[5];

static char *mail_path;

char **
get_mail_path_ptr(void)
{
	return &mail_path;
}

bool
get_mail(void)
{
	DIR *dir;
	struct dirent *dp;

	dir = opendir(mail_path);
	if (dir == NULL)
	{
		perror("opendir");

		return false;
	}
	for (;;)
	{
		dp = readdir(dir);

		if (dp == NULL ||
			(strcmp(dp->d_name, ".") != 0
			&& strcmp(dp->d_name, "..") != 0)) break;
	}
	(void)snprintf(mail_string, 4, "%s", dp ? mail_glyph : " ");
	(void)closedir(dir);

	return true;
}

void
close_mail(void)
{
	free(mail_path);
}

