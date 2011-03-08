/*
 * $Id: timestr.c 346460 2009-11-14 05:06:47Z ssiano $
 * 
 * Time management code
 *
 * Paul Traina, October 1997
 *
 * Copyright (c) 1997-1998, 2000-2001, 2003, Juniper Networks, Inc.
 * All rights reserved.
 */

#include <strings.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/time.h>
#include <libjuise/time/timestr.h>

/*
 * ISO compatible replacement for ctime(3).
 * NOTE: takes time_t by reference, not value.
 */
char *
time_isostr (const time_t *t)
{
    static char buf[80];

    if (strftime(buf, sizeof(buf), "%Y-%m-%d %T %Z", localtime(t)) == 0)
	return NULL;

    return buf;
}

/*
 * ISO compatible replacement for ctime(3).
 * NOTE: takes time_t by reference, not value.
 */
char *
time_isostr_utc (const time_t *t)
{
    static char buf[80];

    if (strftime(buf, sizeof(buf), "%Y-%m-%d-%T", gmtime(t)) == 0)
	return NULL;

    return buf;
}

/*
 * Return a time string corresponding to the number of seconds of time
 * difference.
 */

#define	MINUTE	(60)
#define	HOUR	(60*MINUTE)
#define	DAY	(24*HOUR)
#define	WEEK	(7*DAY)

char *
time_diffstr (const time_t *diff)
{
    static char buf[80];
    time_t t = *diff;

    int week = (t / WEEK);
    int day  = (t % WEEK) / DAY;
    int hour = (t % DAY)  / HOUR;
    int min  = (t % HOUR) / MINUTE;
    int sec  = (t % MINUTE);

    if (week > 0)
	snprintf(buf, sizeof(buf), "%dw%dd %02d:%02d", week, day, hour, min);
    else if (day > 0)
	snprintf(buf, sizeof(buf), "%dd %02d:%02d", day, hour, min);
    else
	snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);

    return buf;
}


/*
 * Return a time string corresponding to a duration in seconds.
 * This is just time_diffstr as call-by-value.
 */

char *
time_valstr (const time_t duration)
{
    return(time_diffstr(&duration));
}
