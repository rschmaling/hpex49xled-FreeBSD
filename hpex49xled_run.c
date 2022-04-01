/////////////////////////////////////////////////////////////////////////////
/////// @file hpex49xled_run.c
///////
/////// Daemon for controlling the LEDs on the HP MediaSmart Server EX49X
/////// FreeBSD Support - written for FreeBSD 12.3 or greater. 
///////
/////// -------------------------------------------------------------------------
///////
/////// Copyright (c) 2022 Robert Schmaling
/////// 
/////// All LED code ported from https://github.com/merelin/mediasmartserverd 
/////// written by Chris Byrne/Kai Hendrik Behrends/Brian Teague
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
#include <assert.h>
#include <math.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <devstat.h>
#include <camlib.h>
#include <getopt.h>
#include <pwd.h>
#include <syslog.h>
#include <pthread.h>
#include <pthread_np.h>
#include <machine/cpufunc.h>

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "hpled.h"

struct statinfo cur;
kvm_t *kd = NULL;
char *devicename;
char **specified_devices;
size_t num_devices;
struct device_selection *dev_select;
struct devstat_match *matches;
devstat_select_mode select_mode;
long generation;
long select_generation;
int num_matches;
int num_devices_specified;
int num_selected, num_selections;
size_t maxshowdevs;
size_t thread_run = 0;
size_t dev_change = 0;
size_t hpdisks = 0;
char *HD = "IDE";
size_t debug = 0;
size_t HP = 1; /* for now set all options to HP */
int io; 
int openkvm = 1; /* track changes to kvm_t *kd status */

struct hpled ide0, ide1, ide2, ide3 ;
struct hpled hpex49x[4];

const char *VERSION = "1.0.1";
const char *progname;
extern const char *hardware;

pthread_attr_t attr; // attributes for threads
pthread_t hpexled_led[4]; /* there can be only 4! */
/* using spinlocks vs. mutex as the thread should spin vs. sleep */
pthread_spinlock_t  hpex49x_gpio_lock; 
pthread_spinlock_t	hpex49x_gpio_lock2;

char* curdir(char *str);
int show_help(char * progname);
int show_version(char * progname );
void drop_priviledges( void );
size_t disk_init(void);
size_t run_mediasmart(void);
void* hpex49x_thread_run (void *arg);
void* acer_thread_run (void *arg);
void sigterm_handler(int s);
const char* desc(void);
extern void setsystemled( int led_type, int state );
extern void set_hpex_led( int led_type, int state, size_t led );
extern void set_acer_led( int led_type, int state, size_t led );
extern size_t init_hpex49x_led(void);

char* curdir(char *str)
{
	char *cp = strrchr(str, '/');
	return cp ? cp+1 : str;
}

int show_help(char * progname ) 
{

	char *this = curdir(progname);
	printf("%s %s %s", "Usage: ", this,"\n");
	printf("-d, --debug 	Print Debug Messages\n");
	printf("-D, --daemon 	Detach and Run as a Daemon - do not use this in service setup \n");
	printf("-h, --help	Print This Message\n");
	printf("-v, --version	Print Version Information\n");

       return 0;
};

int show_version(char * progname ) 
{
	char *this = curdir(progname);
        printf("%s v%s %s %s %s %s %s",this,VERSION,"compiled on", __DATE__,"at", __TIME__ ,"\n") ;
        return 0;
};

void drop_priviledges( void ) 
{
	struct passwd* pw = getpwnam( "nobody" );
	if ( !pw ) return; /* huh? */
	if ( (setgid( pw->pw_gid )) && (setuid( pw->pw_uid )) != 0 )
		err(1, "Unable to set gid or uid to nobody");

	if(debug) {
		printf("Successfully dropped priviledges to %s \n",pw->pw_name);
		printf("We should now be safe to continue \n");
	}
};

const char* desc(void) 
{ 
		return hardware;	
};

size_t disk_init(void) 
{
    size_t dn, di;
    u_int64_t total_bytes, total_bytes_read, total_bytes_write; 
    char *devicename;
	struct cam_device *cam_dev = NULL;
    long double etime = 1.00; /* unneeded for our needs but passed in case of future need */
	size_t disks = 0;
	num_matches = 0;
	openkvm = 1;

	matches = NULL;

	if (devstat_buildmatch(HD, &matches, &num_matches) != 0)
		errx(1, "%s in %s line %d", devstat_errbuf,__FUNCTION__, __LINE__);

	if(debug) printf("\nAfter devstat_buildmatch - Matches = %d Number of Matches = %d \n", matches->num_match_categories, num_matches);

	if (devstat_checkversion(kd) < 0)
		errx(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);

	if ((num_devices = devstat_getnumdevs(kd)) < 0)
		err(1, "can't get number of devices in %s line %d", __FUNCTION__, __LINE__);

	if(debug) printf("Number of devices is: %ld \n", num_devices);

	cur.dinfo = (struct devinfo *)calloc(1, sizeof(struct devinfo));

	if (cur.dinfo == NULL)
		err(1, "calloc failed in %s line %d", __FUNCTION__, __LINE__);

    if (devstat_getdevs(kd, &cur) == -1)
        err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);
	
    specified_devices = calloc(num_matches, sizeof(char *));
	
	if (specified_devices == NULL)
		err(1, "calloc failed for specified_device in %s line %d", __FUNCTION__, __LINE__);

	specified_devices[0] = malloc(strlen("11") * sizeof(char *));

	if( specified_devices[0] == NULL )
		err(1, "malloc failed for specified_devices[a]");

	assert(sizeof(specified_devices[0]) > sizeof("4"));
	strlcpy(specified_devices[0], "4", sizeof(specified_devices[0]));

	maxshowdevs = 4;
	num_devices = cur.dinfo->numdevs;
	generation = cur.dinfo->generation;
	num_devices_specified = num_matches;

	/* calculate all updates since boot */
	cur.snap_time = 0;

	if(debug) {
		printf("Max Show Devices = %ld \n", maxshowdevs);
		printf("Number of Devices = %ld \n", num_devices);
		printf("Generation = %ld \n", generation);
		printf("Number of Devices Specified = %d \n", num_devices_specified);
		printf("Specified Devices is = %s \n", specified_devices[0]);
		printf("End of devstat selection section in %s line %d\n\n\n", __FUNCTION__, __LINE__);
	}
	dev_select = NULL;
	select_mode = DS_SELECT_ONLY;

	if (devstat_selectdevs(&dev_select, &num_selected,
                            &num_selections, &select_generation, generation,
                            cur.dinfo->devices, num_devices, matches,
                            num_matches, specified_devices,
                            num_devices_specified, select_mode, maxshowdevs,
                            0) == -1)
    		errx(1, "%s", devstat_errbuf);

    for (dn = 0; dn < num_devices; dn++) {
		//if ((dev_select[dn].selected == 0) || (dev_select[dn].selected > maxshowdevs))
		if (dev_select[dn].selected > maxshowdevs)
			continue;

        di = dev_select[dn].position;
		/* etime is not used for these statistics - passing for completeness */
        if (devstat_compute_statistics(&cur.dinfo->devices[di], NULL, etime,
        DSM_TOTAL_BYTES, &total_bytes,
        DSM_TOTAL_BYTES_READ, &total_bytes_read,
        DSM_TOTAL_BYTES_WRITE, &total_bytes_write,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        NULL,
        NULL,
        NULL,
        DSM_NONE) != 0)
			err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);

		if ((dev_select[dn].selected == 0) || (dev_select[dn].selected > maxshowdevs))
			continue;

        if (asprintf(&devicename, "/dev/%s%d", cur.dinfo->devices[di].device_name, cur.dinfo->devices[di].unit_number) == -1)
 			errx(1, "asprintf"); 

		cam_dev = cam_open_device(devicename, O_RDWR);

		if(debug) {
			printf("Struct devinfo device name after adding 0-3 is %s \n",devicename);
			printf("The string length of %s is %ld\n", devicename, (strlen(devicename)));
			printf("CAM device name is : %s \n", cam_dev->device_name);
			printf("The Unit Number is: %i \n", cam_dev->dev_unit_num);
			printf("The Sim Name is: %s \n", cam_dev->sim_name);
			printf("The sim_unit_number is: %i \n", cam_dev->sim_unit_number);
			printf("The bus_id is: %i \n", cam_dev->bus_id);
			printf("The target_lun is: %li \n",cam_dev->target_lun);
			printf("The target_id is: %i \n",cam_dev->target_id);
			printf("The path_id is: %i \n",cam_dev->path_id);
			printf("The pd_type is: %i \n",cam_dev->pd_type);
			printf("The value to be assined to hpled.dev_index is %ld\n", di);
			printf("The file descriptor is: %i \n\n\n",cam_dev->fd);
		}
		/* on a HP EX48x and EX49x there are only 4 IDE devices. These will always be the same */
		/* rather than mess around with dynamically allocating and figuring them out, Just if/else if them here */
		if( cam_dev->path_id == 1 && cam_dev->target_id == 0) {
			assert(sizeof(devicename) < sizeof(ide0.path));
			strlcpy(ide0.path,devicename, sizeof(ide0.path));
			ide0.target_id = cam_dev->target_id;		
			ide0.path_id = cam_dev->path_id;
			ide0.b_read = total_bytes_read;
			ide0.b_write = total_bytes_write;
			ide0.n_read = 0;
			ide0.n_write = 0;
			ide0.dev_index = di;
			ide0.HDD = 1;
			hpex49x[di] = ide0;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld\nTotal bytes write: %ld\n\n",ide0.HDD, ide0.b_read, ide0.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide0.path, ide0.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide0.path, ide0.HDD);
			++disks;	
		}
		else if ( cam_dev->path_id == 2 && cam_dev->target_id == 0) {
			assert(sizeof(devicename) < sizeof(ide1.path));
			strlcpy(ide1.path,devicename, sizeof(ide1.path));
			ide1.target_id = cam_dev->target_id;		
			ide1.path_id = cam_dev->path_id;
			ide1.b_read = total_bytes_read;
			ide1.b_write = total_bytes_write;
			ide1.n_read = 0;
			ide1.n_write = 0;
			ide1.dev_index = di;
			ide1.HDD = 2;
			hpex49x[di] = ide1;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld \nTotal bytes write: %ld\n\n",ide1.HDD, ide1.b_read, ide1.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide1.path, ide1.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide1.path, ide1.HDD);
			++disks;
		}
		else if ( cam_dev->path_id == 3 && cam_dev->target_id == 0) {
			assert(sizeof(devicename) < sizeof(ide2.path));
			strlcpy(ide2.path,devicename, sizeof(ide2.path));
			ide2.target_id = cam_dev->target_id;		
			ide2.path_id = cam_dev->path_id;
			ide2.b_read = total_bytes_read;
			ide2.b_write = total_bytes_write;
			ide2.n_read = 0;
			ide2.n_write = 0;
			ide2.dev_index = di;
			ide2.HDD = 3;
			hpex49x[di] = ide2;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld\nTotal bytes write: %ld\n\n",ide2.HDD, ide2.b_read, ide2.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide2.path, ide2.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide2.path, ide2.HDD);
			++disks;
		}
		else if ( cam_dev->path_id == 4 && cam_dev->target_id == 0) {
			assert(sizeof(devicename) < sizeof(ide3.path));
			strlcpy(ide3.path,devicename, sizeof(ide3.path));
			ide3.target_id = cam_dev->target_id;		
			ide3.path_id = cam_dev->path_id;
			ide3.b_read = total_bytes_read;
			ide3.b_write = total_bytes_write;
			ide3.n_read = 0;
			ide3.n_write = 0;
			ide3.dev_index = di;
			ide3.HDD = 4;
			hpex49x[di] = ide3;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld \nTotal bytes write: %ld\n\n",ide3.HDD, ide3.b_read, ide3.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide3.path, ide3.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide3.path, ide3.HDD);
			++disks;
		}
		else { /* something went wrong here */
			err(1, "unknown path_id or target_id in %s line %d", __FUNCTION__, __LINE__);
		}

		if(di > 3)
			err(1, "Illegal number of devices in devstat() in %s line %d", __FUNCTION__, __LINE__);

		cam_close_device(cam_dev);
		free(devicename);
	}
	free(specified_devices[0]);
	specified_devices[0] = NULL;
	free(specified_devices);
	specified_devices = NULL;
	if(debug)
		printf("\nsize_t disks is %ld before returning from %s line %d\n", disks, __FUNCTION__, __LINE__);
	return (disks);
};
/////////////////////////////////////////////////////////////
//// run disk monintoring thread for HP EX48x or EX49x
void* hpex49x_thread_run (void *arg)
{
	struct hpled mediasmart = *(struct hpled *)arg;
    u_int64_t total_bytes; 
    long double etime = 1.00;
	int led_state = 0;
	struct timespec tv = { .tv_sec = 0, .tv_nsec = BLINK_DELAY };
	int thID = pthread_getthreadid_np();

	while(thread_run) {

		if( (pthread_spin_lock(&hpex49x_gpio_lock)) == EDEADLK ){

			syslog(LOG_NOTICE, "Deadlock condition in HP disk %d function %s line %d",mediasmart.HDD, __FUNCTION__, __LINE__ );
			fprintf(stderr, "Deadlock return from pthread_spin_lock from thread %d for HP disk %d in %s line %d\n",thID, mediasmart.HDD, __FUNCTION__, __LINE__);
			thread_run = 0;
			pthread_spin_unlock(&hpex49x_gpio_lock);
		}
		/* check to see if a device change occured and was identified in another thread before lock acquisiton */
		if( openkvm <= 0 || cur.dinfo == NULL || thread_run == 0) {
			fprintf(stderr, "Thread %d terminating due to conditions: openkvm: %d cur.dinfo: %d thread_run: %ld in %s line %d\n", thID, openkvm, (cur.dinfo == NULL) ? 0 : 1, thread_run, __FUNCTION__, __LINE__);
			thread_run = 0;
			pthread_spin_unlock(&hpex49x_gpio_lock);
			break;
		}

		int retval = devstat_getdevs(kd, &cur);

		if( retval == 1 ) {
			thread_run = 0; /* end the threads so we can re-initialize */
			dev_change = 1; /* a device has changed and we must re-initialize */
			if( (pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
				err(1, "invalid return from pthread_spin_unlock in thread %d from HP disk %d in %s line %d", thID, mediasmart.HDD, __FUNCTION__, __LINE__);
			break;
		}
		if (retval == -1 ) {
			thread_run = 0; /* end the threads - we have a real problem */
			dev_change = 0; /* not a device change */
			
			syslog(LOG_CRIT, "Bad return from devstat_getdevs() in thread for HP disk %d function %s line %d",mediasmart.HDD, __FUNCTION__, __LINE__ );
			err(1, "invalid return from devstat_getdevs() from thread %d for HP disk %d in %s line %d", thID, mediasmart.HDD, __FUNCTION__, __LINE__);
			
			if( (pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
				err(1, "invalid return from pthread_spin_unlock in thread %d for HP disk %d in %s line %d", thID, mediasmart.HDD, __FUNCTION__, __LINE__);
			
			break; /* just in case */
		} 
        if (devstat_compute_statistics(&cur.dinfo->devices[mediasmart.dev_index], NULL, etime,
        DSM_TOTAL_BYTES, &total_bytes,
        DSM_TOTAL_BYTES_READ, &mediasmart.n_read,
        DSM_TOTAL_BYTES_WRITE, &mediasmart.n_write,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        NULL,
        NULL,
        NULL,
        DSM_NONE) != 0)
			err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);
		
		if( (pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
			err(1, "invalid return from pthread_spin_unlock from thread %d for HP disk %d in %s line %d",thID, mediasmart.HDD, __FUNCTION__, __LINE__);

		if( ( mediasmart.b_read != mediasmart.n_read ) && ( mediasmart.b_write != mediasmart.n_write) ) {

			mediasmart.b_read = mediasmart.n_read;
			mediasmart.b_write = mediasmart.n_write;

			if(debug) {
				printf("Read I/O = %li Write I/O = %li \n", mediasmart.n_read, mediasmart.n_write);
				printf("HP HDD is: %i \n", mediasmart.HDD);
				printf("Thread ID is: %d\n", thID);
			}
			set_hpex_led(LED_BLUE, ON, mediasmart.blue);
			set_hpex_led(LED_RED, ON, mediasmart.red);
			led_state = 1;

		}
		else if( mediasmart.b_read != mediasmart.n_read ) {

			mediasmart.b_read = mediasmart.n_read;

			if(debug) {
				printf("Read I/O only and is: %li \n", mediasmart.n_read);
				printf("HP HDD is: %i \n", mediasmart.HDD);
				printf("Thread ID is: %d\n", thID);
			}
			set_hpex_led(LED_BLUE, ON, mediasmart.blue);
			set_hpex_led(LED_RED, ON, mediasmart.red);
			led_state = 1;
		}
		else if( mediasmart.b_write != mediasmart.n_write ) {

			mediasmart.b_write = mediasmart.n_write;

			if(debug) {
				printf("Write I/O only and is: %li \n", mediasmart.n_write);
				printf("HP HDD is: %i \n", mediasmart.HDD);
				printf("Thread ID is: %d\n", thID);
			}
			set_hpex_led(LED_BLUE, ON, mediasmart.blue);
			set_hpex_led(LED_RED, OFF, mediasmart.red);
			// if(hpex49x.hphdd == 4) // this is how I discovered the race condition....
			//	printf("Firing %ld - Write IO is: %ld \n", hpex49x.hphdd, n_wio);
			led_state = 1;
		}
		else {
			/* turn off the active light */
			nanosleep(&tv, NULL);

			if ( (led_state != 0)) {
				set_hpex_led(LED_BLUE, OFF, mediasmart.blue);
				set_hpex_led(LED_RED, OFF, mediasmart.red);
				led_state = 0;
			}
			continue;

		}

	}
	pthread_exit(NULL);   	
};
/////////////////////////////////////////////////////////////
//// run disk monintoring thread for Acer and Lenovo H34x and Atmos
void* acer_thread_run (void *arg)
{
	struct hpled mediasmart = *(struct hpled *)arg;
    u_int64_t total_bytes; 
    long double etime = 1.00;
	int led_state = 0;
	struct timespec tv = { .tv_sec = 0, .tv_nsec = BLINK_DELAY };
	int thID = pthread_getthreadid_np();

	while(thread_run) {

		if( (pthread_spin_lock(&hpex49x_gpio_lock)) == EDEADLK ){

			syslog(LOG_NOTICE, "Deadlock condition in thread %d for HP disk %d function %s line %d",thID, mediasmart.HDD, __FUNCTION__, __LINE__ );
			fprintf(stderr, "Deadlock return from pthread_spin_lock from thread %d for HP disk %d in %s line %d",thID, mediasmart.HDD, __FUNCTION__, __LINE__);
			thread_run = 0;
			pthread_spin_unlock(&hpex49x_gpio_lock);
		}
		/* check to see if a device change occured and was identified in another thread before we acquired the lock */
		if( openkvm <= 0 || cur.dinfo == NULL || thread_run == 0) {
			fprintf(stderr, "Thread %d terminating due to conditions: openkvm: %d cur.dinfo: %d thread_run: %ld in %s line %d\n", thID, openkvm, (cur.dinfo == NULL) ? 0 : 1, thread_run, __FUNCTION__, __LINE__);
			thread_run = 0;
			pthread_spin_unlock(&hpex49x_gpio_lock);
			break;
		}

		int retval = devstat_getdevs(kd, &cur);

		if( retval == 1 ) {
			thread_run = 0; /* end the threads so we can re-initialize */
			dev_change = 1; /* a device has changed and we must re-initialize */
			if( (pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
				err(1, "invalid return from pthread_spin_unlock from thread %d for HP disk %d in %s line %d",thID, mediasmart.HDD, __FUNCTION__, __LINE__);
			break;
		}
		if (retval == -1 ) {
			thread_run = 0; /* end the threads - we have a real problem */
			dev_change = 0; /* not a device change */
			
			syslog(LOG_CRIT, "Bad return from devstat_getdevs() in thread %d for HP disk %d function %s line %d", thID, mediasmart.HDD, __FUNCTION__, __LINE__ );
			err(1, "invalid return from devstat_getdevs() from thread %d for HP disk %d in %s line %d",thID, mediasmart.HDD, __FUNCTION__, __LINE__);

			if( (pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
				err(1, "invalid return from pthread_spin_unlock from thread %d for HP disk %d in %s line %d",thID, mediasmart.HDD, __FUNCTION__, __LINE__);
			break;
		} 
        if (devstat_compute_statistics(&cur.dinfo->devices[mediasmart.dev_index], NULL, etime,
        DSM_TOTAL_BYTES, &total_bytes,
        DSM_TOTAL_BYTES_READ, &mediasmart.n_read,
        DSM_TOTAL_BYTES_WRITE, &mediasmart.n_write,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        NULL,
        NULL,
        NULL,
        DSM_NONE) != 0)
			err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);
		
		if( (pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
			err(1, "invalid return from pthread_spin_unlock from thread %d for HP disk %d in %s line %d", thID, mediasmart.HDD, __FUNCTION__, __LINE__);

		if( ( mediasmart.b_read != mediasmart.n_read ) && ( mediasmart.b_write != mediasmart.n_write) ) {

			mediasmart.b_read = mediasmart.n_read;
			mediasmart.b_write = mediasmart.n_write;

			if(debug) {
				printf("Read I/O = %li Write I/O = %li \n", mediasmart.n_read, mediasmart.n_write);
				printf("HP HDD is: %i \n", mediasmart.HDD);
				printf("Thread ID is: %d\n", thID);
			}
			set_acer_led(LED_BLUE, ON, mediasmart.blue);
			set_acer_led(LED_RED, ON, mediasmart.red);
			led_state = 1;

		}
		else if( mediasmart.b_read != mediasmart.n_read ) {

			mediasmart.b_read = mediasmart.n_read;

			if(debug) {
				printf("Read I/O only and is: %li \n", mediasmart.n_read);
				printf("HP HDD is: %i \n", mediasmart.HDD);
				printf("Thread ID is: %d\n", thID);
			}
			set_acer_led(LED_BLUE, ON, mediasmart.blue);
			set_acer_led(LED_RED, ON, mediasmart.red);
			led_state = 1;
		}
		else if( mediasmart.b_write != mediasmart.n_write ) {

			mediasmart.b_write = mediasmart.n_write;

			if(debug) {
				printf("Write I/O only and is: %li \n", mediasmart.n_write);
				printf("HP HDD is: %i \n", mediasmart.HDD);
				printf("Thread ID is: %d\n", thID);
			}
			set_hpex_led(LED_BLUE, ON, mediasmart.blue);
			set_hpex_led(LED_RED, OFF, mediasmart.red);
			// if(hpex49x.hphdd == 4) // this is how I discovered the race condition....
			//	printf("Firing %ld - Write IO is: %ld \n", hpex49x.hphdd, n_wio);
			led_state = 1;
		}
		else {
			/* turn off the active light */
			nanosleep(&tv, NULL);

			if ( (led_state != 0)) {
				set_acer_led(LED_BLUE, OFF, mediasmart.blue);
				set_acer_led(LED_RED, OFF, mediasmart.red);
				led_state = 0;
			}
			continue;

		}

	}
	pthread_exit(NULL);   	
};
/////////////////////////////////////////////////////////////////////////////
//// Run the threads and return if a drive is added/removed
size_t run_mediasmart(void)
{
	int num_threads = 0;

	/* System LED function takes from enum { LED_OFF, LED_ON, LED_BLINK } in header */
	setsystemled( LED_RED, LED_OFF);
	setsystemled( LED_BLUE, LED_OFF);

	for(int i = 0; i < hpdisks; i++) {
        if ( (pthread_create(&hpexled_led[i], &attr, &hpex49x_thread_run, &hpex49x[i])) != 0)
			err(1, "Unable to create thread for hpex47x_thread_run in %s line %d", __FUNCTION__, __LINE__);
        ++num_threads;

        if(debug)
			printf("HP HDD is %i - created thread %i \n", hpex49x[i].HDD, num_threads);
    }

	syslog(LOG_NOTICE,"Initialized monitor threads. Monitoring with %i threads", num_threads);
	syslog(LOG_NOTICE,"Now monitoring for drive activity");
		
	for(size_t i = 0; i < hpdisks; i++) {
        if ( (pthread_join(hpexled_led[i], NULL)) != 0) {
			/* unsure why thread joining keeps failing on FreeBSD. This works fine on Linux */
			perror("pthread_join()");
			syslog(LOG_NOTICE, "Unable to join threads - this is only informational - in %s line %d", __FUNCTION__, __LINE__);
    	}
	}
	if(HP) {
			for(size_t i = 0; i < MAX_HDD_LEDS; i++){
				set_hpex_led(LED_BLUE, i, OFF);
				set_hpex_led(LED_RED, i , OFF);
		}
	}
	else {
		for(size_t i = 0; i< MAX_HDD_LEDS; i++){
			set_acer_led(LED_BLUE, i, OFF);
			set_acer_led(LED_RED, i, OFF);
		}
	}
	thread_run = 0;
	return dev_change;
};
////////////////////////////////////////////////////////////////////////////////
//// MAIN Function 
int main (int argc, char **argv) 
{
	int run_as_daemon = 0;

	if (geteuid() !=0 ) {
		printf("Try running as root to avoid Segfault and core dump \n");
		errx(1, "not running as root user");
	}
	
	progname = curdir(argv[0]);

  	const struct option long_opts[] = {
        { "debug",          no_argument,       0, 'd' },
        { "daemon",         no_argument,       0, 'D' },
        { "help",           no_argument,       0, 'h' },
        { "version",        no_argument,       0, 'v' },
        { 0, 0, 0, 0 },
    };

    // pass command line arguments
    while ( 1 ) {
        const int c = getopt_long( argc, argv, "dDhv?", long_opts, 0 );
        if ( -1 == c ) break;

        switch ( c ) {
			case 'D': // daemon
				run_as_daemon++;
				break;
            case 'd': // debug
                debug++;
                break;
            case 'h': // help!
                return show_help(argv[0]);
            case 'v': // our version
                return show_version(argv[0] );
            case '?': // no idea
                return show_help(argv[0] );
            default:
                printf("++++++....\n"); 
        }
    }
	
	if( ( io = open("/dev/io", O_RDWR)) < 0 ) 
		perror("open");
	
	openlog("hpex49xled:", LOG_CONS | LOG_PID, LOG_DAEMON );

	signal( SIGTERM, sigterm_handler);
    signal( SIGINT, sigterm_handler);
    signal( SIGQUIT, sigterm_handler);
    signal( SIGILL, sigterm_handler);

	hpdisks = disk_init() ;

	if(hpdisks <= 0)
		err(1, "Unknown return from disk initialization in %s line %d", __FUNCTION__, __LINE__);

	if (init_hpex49x_led() != 1 )
		err(1, "Unknown return from led initialization in %s line %d", __FUNCTION__, __LINE__);

	if ( run_as_daemon ) {
		if (daemon( 0, 0 ) > 0 )
			err(1, "Unable to daemonize :");
		syslog(LOG_NOTICE,"Forking to background, running in daemon mode");
	}
	if( (pthread_spin_init(&hpex49x_gpio_lock, PTHREAD_PROCESS_PRIVATE)) !=0 )
		err(1,"Unable to initialize first spin_lock in %s at %d", __FUNCTION__, __LINE__);

	if( (pthread_spin_init(&hpex49x_gpio_lock2, PTHREAD_PROCESS_PRIVATE)) !=0 )
		err(1,"Unable to initialize second spin_lock in %s at %d", __FUNCTION__, __LINE__);

	if ((pthread_attr_init(&attr)) < 0 )
		err(1, "Unable to execute pthread_attr_init(&attr) in main()");
	
	if ((pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0) {
		perror("pthread_attr_setdetatchstate()");
		err(1, "Unable to set pthread_attr_setdetachstate() in %s line %d", __FUNCTION__, __LINE__);
	}
	if((pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) != 0){
		perror("pthread_attr_setscope()");
		err(1, "Unable to set pthread_attr_setscope() in %s line %d", __FUNCTION__, __LINE__);
	}

	/* Try and drop root priviledges now that we have initialized */
	drop_priviledges();

	thread_run = 1;

	int run = 1;
	while (run) {
		switch (thread_run){
			case 1: {
				int retval;
				retval = run_mediasmart(); /* returns value of dev_change */

				switch(retval) {
					case 1:
						free(cur.dinfo);
						cur.dinfo = NULL;
						openkvm = kvm_close(kd); /* should return -1 */
						free(dev_select); /* found this out the hard way - see man devstat_buildmatch */
						free(matches); /* same here */
						syslog(LOG_NOTICE, "New or removed device detected - reinitializing");
						if(debug)
							printf("\n\n**** New/Removed Device Detected - re-initializing ****\n\n");
						hpdisks = disk_init();
						init_hpex49x_led();
						if(hpdisks <= 0)
							err(1, "Unknown return from disk initialization in %s line %d", __FUNCTION__, __LINE__);
						dev_change = 0;
						thread_run = 1;
						break;

					default:
						break;	
				}
			}
			default:
				printf("In default case in while loop - function %s line %d\n", __FUNCTION__, __LINE__);
				break;
		}

	}

	return 1;
};
//////////////////////////////////////////////////////////////////////////
//// general signal handler
void sigterm_handler(int s)
{
	thread_run = 0;

	setsystemled( LED_RED, LED_OFF);
	setsystemled( LED_BLUE, LED_OFF);

	for(size_t i = 0; i < hpdisks; i++) {
        if ( (pthread_join(hpexled_led[i], NULL)) != 0) {
			if( pthread_cancel(hpexled_led[i]) != 0) {
				perror("pthread_cancel()");
				err(1, "Unable to cancel threads - pthread_cancel in %s line %d", __FUNCTION__, __LINE__);
			}
		}
	}
	if(HP) {
		for(size_t i = 0; i < MAX_HDD_LEDS; i++){
			set_hpex_led(LED_BLUE, i, OFF);
			set_hpex_led(LED_RED, i , OFF);
		}
	}
	else {
		for(size_t i = 0; i< MAX_HDD_LEDS; i++){
			set_acer_led(LED_BLUE, i, OFF);
			set_acer_led(LED_RED, i, OFF);
		}
	}

	if( (pthread_spin_destroy(&hpex49x_gpio_lock)) != 0 )
		perror("pthread_spin_destroy lock 1");
	if( (pthread_spin_destroy(&hpex49x_gpio_lock2)) != 0 )
		perror("pthread_spin_destroy lock 2");

	pthread_attr_destroy(&attr);

	free(cur.dinfo);
	cur.dinfo = NULL;
	kvm_close(kd);
	free(dev_select); /* found this out the hard way - see man devstat_buildmatch */
	free(matches); /* same here */
	syslog(LOG_NOTICE,"Signal Received. Exiting");
	closelog();
	close(io);
	errx(0, "\nExiting From Signal Handler\n");

};