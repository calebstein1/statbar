#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include <curl/curl.h>

#include "statbar.h"

char weather_string[48] = "...";

static pthread_t weather_pthread;
static char *weather_location;

char **
get_weather_location_ptr(void)
{
	return &weather_location;
}

size_t
write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	char *lbrk;
	size_t d_len;

	(void)size;
	(void)userdata;

	lbrk = strchr(ptr, '\n');
	if (lbrk) *lbrk = '\0';

	d_len = strlen(ptr);
	if (d_len >= 47)
	{
		if (logfile_open)
			(void)fprintf(logfile, "weather: too much data from server, got %lu, maximum is 47\n", d_len);

		return 0;
	}
	(void)strcpy(weather_string, ptr);
	weather_string[d_len] = '\0';

	return nmemb;
}

void *
weather_thread(void *arg)
{
	CURL *curl;
	char *wttr_url;
	bool *weather_dirty = arg;
	char err_buff[CURL_ERROR_SIZE];
	CURLcode curle;
	int request_count = 0;

	if (!weather_location) return NULL;

	if (asprintf(&wttr_url, "https://wttr.in/%s?format=1&u", weather_location) == -1)
	{
		logerr("weather: asprintf");

		return NULL;
	}
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
	{
		(void)fprintf(logfile_open ? logfile : stderr, "Failed to init libcurl\n");

		return NULL;
	}

	curl = curl_easy_init();
	if (!curl)
	{
		(void)fputs("weather: failed to init libcurl\n", logfile);
		weather_string[0] = '\0';

		return NULL;
	}
	(void)curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_buff);
	(void)curl_easy_setopt(curl, CURLOPT_URL, wttr_url);
	(void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	do
	{
		curle = curl_easy_perform(curl);
		if (curle != CURLE_OK)
		{
			if (logfile_open) (void)fprintf(logfile, "weather request failed: %s\n", err_buff);
			(void)sleep(1);
		}
		else
		{
			*weather_dirty = true;
			break;
		}
	} while (request_count++ < 2);

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	free(wttr_url);
	if (logfile_open) (void)fputs("Destroying weather thread\n", logfile);

	return NULL;
}

void
get_weather(bool *weather_dirty)
{
	if (logfile_open) (void)fputs("Creating weather thread\n", logfile);
	if (pthread_create(&weather_pthread, NULL, weather_thread, weather_dirty) != 0)
	{
		logerr("weather: pthread_create");

		return;
	}
}

void
close_weather(void)
{
	free(weather_location);
}

