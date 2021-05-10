// ----------------------------------------------------------------------------
/*!
 * @file         readtime.c
 * @author    John Steggall
 * @date       08 April 2021
 * @brief       Spams the i2c bus with requests to the RTC to 
 *                  get its time until it changes or ioctl finds an error.
 *                  Essentially mimics hwclock.
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include <asm/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

struct linux_rtc_time 
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

# define RTC_RD_TIME	_IOR('p', 0x09, struct linux_rtc_time)	


int main()
{
	struct tm rtctime, nexttime;
	int rc, i, rtc_dev_fd;

	rtc_dev_fd = open("/dev/rtc", O_RDONLY);

	rc = ioctl(rtc_dev_fd, RTC_RD_TIME, &rtctime);

	if (rc == -1)
	{

		printf("Failed to read time\n");
	}
	else
	{
		printf("Time - %d:%d:%d\n", rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);
	}


	while (1)
	{
		rc = ioctl(rtc_dev_fd, RTC_RD_TIME, &nexttime);

		if (rtctime.tm_sec != nexttime.tm_sec || rc)
		{
			break;
		}
	}

	if (rc == -1)
	{
		printf("Failed to read time\n");
	}
	else
	{
		printf("Seconds changed!\nTime - %d:%d:%d\n", nexttime.tm_hour, nexttime.tm_min, nexttime.tm_sec);
	}

	close(rtc_dev_fd);

	return rc;
}

