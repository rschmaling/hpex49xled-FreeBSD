/////////////////////////////////////////////////////////////////////////////
/////// @file camtest.c
///////
/////// Utility to provide drive setup insight for hard drives 
/////// Used in conjunction with the hpex49xled program 
///////
/////// written for FreeBSD 12.3 or greater. 
///////
/////// -------------------------------------------------------------------------
///////
/////// Copyright (c) 2022 Robert Schmaling
/////// 
/////// 
/////// This software is provided 'as-is', without any express or implied
/////// warranty. In no event will the authors be held liable for any damages
/////// arising from the use of this software.
/////// 
/////// Permission is granted to anyone to use this software for any purpose,
/////// including commercial applications, and to alter it and redistribute it
/////// freely, subject to the following restrictions:
/////// 
/////// 1. The origin of this software must not be misrepresented; you must not
/////// claim that you wrote the original software. If you use this software
/////// in a product, an acknowledgment in the product documentation would be
/////// appreciated but is not required.
/////// 
/////// 2. Altered source versions must be plainly marked as such, and must not
/////// be misrepresented as being the original software.
/////// 
/////// 3. This notice may not be removed or altered from any source
/////// distribution.
///////
/////////////////////////////////////////////////////////////////////////////////
/////// 
/////// Changelog
/////// March 31, 2022
/////// - Initial Release
/////// - 
#include <stdio.h>
#include <err.h>
#include <limits.h>
#include <signal.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <kvm.h>
#include <devstat.h>
#include <camlib.h>
#include <paths.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <machine/cpufunc.h>

#define __BSD_VISIBLE 1
#define _STANDALONE 1

/* link with -ldevstat -lcam -lkvm */

char* curdir(char *str)
{
	char *cp = strrchr(str, '/');
	return cp ? cp+1 : str;
}

static struct statinfo cur;
int num_devices;
static struct device_selection *dev_select;
struct devstat_match *matches = NULL;
int maxshowdevs;
char *HD = "ide";

int
main (int argc, char **argv)
{
	char *devicename;
	char **specified_devices;
	kvm_t *kd = NULL;
	struct cam_device *cam_dev = NULL;
	devstat_select_mode select_mode;
	long generation;
	long select_generation;
	/* etime is needed for devstat_compute_statistics() but not for the variables we want so set to 0.00 */
	// long double etime = 0.00;
	int num_matches = 0;
	int num_devices_specified;
	int num_selected, num_selections;
	// const char *errbuff;

	if (geteuid() !=0 ) {
		printf("Try running as root to avoid Segfault and core dump \n");
		err(1, "not running as root user");
	}

	matches = NULL;

	if (devstat_buildmatch(HD, &matches, &num_matches) != 0)
		errx(1, "%s in %s line %d", devstat_errbuf,__FUNCTION__, __LINE__);

	printf("\nAfter devstat_buildmatch - Matches = %d Number of Matches = %d \n", matches->num_match_categories, num_matches);

	if (devstat_checkversion(kd) < 0)
		errx(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);

	if ((num_devices = devstat_getnumdevs(kd)) < 0)
		err(1, "can't get number of devices in %s line %d", __FUNCTION__, __LINE__);

	printf("Number of devices from devstat_getnumddevs() : %d \n", num_devices);

	cur.dinfo = (struct devinfo *)calloc(1, sizeof(struct devinfo));

	if (cur.dinfo == NULL)
		err(1, "calloc failed in %s line %d", __FUNCTION__, __LINE__);

    if (devstat_getdevs(kd, &cur) == -1)
        err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);
	
    specified_devices = calloc(num_matches, sizeof(char *));
	
	if (specified_devices == NULL)
		err(1, "calloc failed for specified_device in %s line %d", __FUNCTION__, __LINE__);

	specified_devices[0] = calloc(1, strlen("111"));

	if( specified_devices[0] == NULL )
		err(1, "malloc failed for specified_devices[a]");

	size_t ret = strlcpy(specified_devices[0], "4", sizeof(specified_devices[0]));
	if(ret > sizeof(specified_devices[0]))
		err(1, "ERROR: incorrect sizeof() value in %s line %d", __FUNCTION__, __LINE__);

	if(num_devices != cur.dinfo->numdevs)
		err(1, "Number of devices is inconsistent in %s line %d", __FUNCTION__, __LINE__);

	maxshowdevs = 4;
	num_devices = cur.dinfo->numdevs;
	generation = cur.dinfo->generation;
	num_devices_specified = num_matches;

	/* calculate all updates since boot */
	cur.snap_time = 0;

	printf("\nLooking at %s devices only\n", HD);
	printf("Max Show Devices            : %d \n", maxshowdevs);
	printf("Number of Devices           : %d \n", num_devices);
	printf("Generation                  : %ld \n", generation);
	printf("Number of Devices Specified : %d \n", num_devices_specified);
	printf("Specified Devices is        : %s \n", specified_devices[0]);
	printf("\n\n");
	dev_select = NULL;
	select_mode = DS_SELECT_ONLY;

	if (devstat_selectdevs(&dev_select, &num_selected,
		&num_selections, &select_generation, generation,
		cur.dinfo->devices, num_devices, matches,
		num_matches, specified_devices,
		num_devices_specified, select_mode, maxshowdevs,
		0) == -1)
			err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);

	for (int dn = 0; dn < num_devices; dn++){

		int di;

		if ((dev_select[dn].selected == 0) || (dev_select[dn].selected > maxshowdevs))
			continue;
		di = dev_select[dn].position;

		if (asprintf(&devicename, "/dev/%s%d", cur.dinfo->devices[di].device_name, cur.dinfo->devices[di].unit_number) == -1)
			errx(1, "asprintf in %s line %d", __FUNCTION__, __LINE__); 

		cam_dev = cam_open_device(devicename, O_RDWR);

		printf("Device Name                   : %s \n",devicename);	
		printf("This devices selection status : %d \n", dev_select[dn].selected);
		printf("Device Name                   : %s \n", cam_dev->device_name);
		printf("The Unit Number               : %i \n", cam_dev->dev_unit_num);
		printf("The Sim Name                  : %s \n", cam_dev->sim_name);
		printf("The sim_unit_number           : %i \n", cam_dev->sim_unit_number);
		printf("The bus_id                    : %i \n", cam_dev->bus_id);
		printf("The target_lun                : %li \n",cam_dev->target_lun);
		printf("The target_id                 : %i \n",cam_dev->target_id);
		printf("The path_id                   : %i \n",cam_dev->path_id);
		printf("The pd_type                   : %i \n",cam_dev->pd_type);
		printf("The file descriptor           : %i \n\n",cam_dev->fd);

		free((void *)devicename);
		cam_close_device(cam_dev);
	}
	free(cur.dinfo);
	cur.dinfo = NULL;
	free(specified_devices[0]);
	specified_devices[0] = NULL;
	free(specified_devices);
	specified_devices = NULL;
	free((void *)dev_select);
	free((void *)matches);
	return 0;

}
