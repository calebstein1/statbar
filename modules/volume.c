#include <stdio.h>
#include <string.h>

#include <poll.h>

#include <sndio.h>

#include "statbar.h"

char volume_string[11];

static struct sioctl_hdl *hdl;

void
onval(void *arg, unsigned int addr, unsigned int val)
{
	(void)arg;
	(void)addr;
	(void)snprintf(volume_string, 11, "%s  %3d%%", volume_glyph, val * 100 / 255);
}

void
ondesc(void *arg, struct sioctl_desc *desc, int val)
{
	static bool did_init = false;

	(void)arg;
	if (!did_init && desc != NULL && strcmp(desc->func, "level") == 0)
	{
		(void)snprintf(volume_string, 11, "%s  %3d%%", volume_glyph, val * 100 / 255);
		did_init = true;
	}
}

bool
init_volume(int *nfds)
{
	hdl = sioctl_open(SIO_DEVANY, SIOCTL_READ, 0);
	if (hdl == NULL)
	{
		(void)fputs("Could not open sndio\n", stderr);
		(void)strcpy(volume_string, "Vol: ?");

		return false;
	}
	*nfds += sioctl_nfds(hdl);
	(void)sioctl_ondesc(hdl, ondesc, NULL);
	(void)sioctl_onval(hdl, onval, NULL);

	return true;
}

int
get_volume_nev(struct pollfd *pfd)
{
	return sioctl_pollfd(hdl, pfd, POLLIN);
}

void
process_volume_events(struct pollfd *pfd, bool *hdl_open)
{
			if (sioctl_revents(hdl, pfd) & POLLHUP)
			{
				(void)fputs("Lost connection to sndio\n", stderr);
				sioctl_close(hdl);
				*hdl_open = false;
			}
}

void
close_volume(void)
{
	sioctl_close(hdl);
}

