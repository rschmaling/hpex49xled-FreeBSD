/////////////////////////////////////////////////////////////////////////////
/////// @file hpex49xled_led.c
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
#include <kvm.h>
#include <devstat.h>
#include <camlib.h>
#include <getopt.h>
#include <pwd.h>
#include <pthread.h>
#include <syslog.h>
#include <machine/cpufunc.h>

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "hpex49x_led.h"
#include "hpled.h"

extern pthread_spinlock_t hpex49x_gpio_lock2;
extern struct hpled hpex49x[4];

///////////////////////////////////////////////////////////
//// Initialize the SCH5127 Interface - where applicable
size_t initsch5127(const unsigned int vendor) 
{
	enum {
		IDX_LDN		= 0x07,	///< Logical Device Number
		IDX_ID		= 0x20,	///< device identification
		IDX_BASE_MSB	= 0x60,	///< base address MSB register
		IDX_BASE_LSB	= 0x61,	///< base address LSB register
		IDX_ENTER	= 0x55,	///< enter configuration mode
		IDX_EXIT	= 0xaa,	///< exit configuration mode
	};
		
	enum {
		CONF_VENDOR_ID	= 0x8000F800,   ///< Vendor Identification (enable, bus 0, device 31, function 0, register 0x00)
		CONF_GPIOBASE	= 0x8000F848,   ///< GPIO Base address     (enable, bus 0, device 31, function 0, register 0x48)
	};

	const unsigned int PCI_CONFIG_ADDRESS = 0x0CF8;
	const unsigned int PCI_CONFIG_DATA = 0x0CFC;

	// retrieve vendor and device identification
	// LINUX is the opposite of FreeBSD regarding outl or outw etc.  outl( CONF_VENDOR_ID, PCI_CONFIG_ADDRESS );
	outl(PCI_CONFIG_ADDRESS, CONF_VENDOR_ID);
	const unsigned int did_vid = inl( PCI_CONFIG_DATA );
	
	if ( vendor != did_vid ) {
		fprintf(stderr,"GPIO Vendor %d did not match return from inl() %d in %s line %d\n", vendor, did_vid, __FUNCTION__, __LINE__);
		return 0;
	}
	// retrieve GPIO Base Address
	outl( PCI_CONFIG_ADDRESS, CONF_GPIOBASE );
	gpiobase = inl( PCI_CONFIG_DATA );

	if (debug)
		printf("in %s gpiobase is: %#08X on line %d\n",__FUNCTION__, gpiobase, __LINE__);
		
	// sanity check the address
	// (only bits 15:6 provide an address while the rest are reserved as always being zero)
	if ( 0x1 != (gpiobase & 0xFFFF007F) ) {
		if ( debug ) 
			printf("%s : Expected 0x1 but got - %#08X on line %d \n",desc(), (gpiobase & 0xFFFF007F), __LINE__ );
		return 0;
	}
	gpiobase &= ~0x1; // remove hardwired 1 which indicates I/O space

	if (debug)
	 	printf("In %s gpiobase after 0x1 line %d application is: %#08x \n",__FUNCTION__, __LINE__, gpiobase);
		
	// finished with these ports
	
	// try LPC SIO @ 0x2e
	unsigned int sio_addr = 0x2e;
	unsigned int sio_data = sio_addr + 1;
		
	// enter configuration mode
	outb( sio_addr, IDX_ENTER );
		
	// retrieve identification
	outb( sio_addr, IDX_ID );
	const unsigned int device_id = inb( sio_data );
	if ( debug ) 
		printf("Device 0x %#08X in %s on line %d \n", device_id,__FUNCTION__, __LINE__);
	outb( sio_addr, 0x26 );
	const unsigned int in = inb( sio_data );
	if( debug )
		printf("in from inb() is 0x%#08X in %s on line %d \n",in,__FUNCTION__, __LINE__);
	if ( 0x4e == in ) {
		outb( sio_addr, IDX_EXIT );
			
		// and switch to these if we are told to
		if ( debug ) 
			printf("%s: Using 0x4e\n", desc());
		sio_addr = 0x4e;
		sio_data = sio_addr + 1;
				
		outb( sio_addr, IDX_ENTER );
	}
	// select logical device 0x0a (base address?)
	outb( sio_addr, IDX_LDN );
	outb( sio_data, 0x0a );
		
	// get base address of runtime registers
	outb( sio_addr, IDX_BASE_MSB );
	const unsigned int index_msb = inb( sio_data );
	if( debug )
		printf("in %s and index_msb is: %#08X on line %d \n",__FUNCTION__, index_msb, __LINE__);
	outb( sio_addr, IDX_BASE_LSB );
	const unsigned int index_lsb = inb( sio_data );
	if( debug )
		printf("in %s and index_lsb is: %#08X on line %d \n",__FUNCTION__, index_lsb, __LINE__);
	
	sch5127_regs = index_msb << 8 | index_lsb;

	if( debug ) 
		printf("in %s and sch5127_regs is now: %#08X on line %d \n",__FUNCTION__, sch5127_regs, __LINE__);
		
	// exit configuration
	outb( sio_addr, IDX_EXIT );
		
	// watchdog registers to zero out
	const int WDT_REGS[] = { REG_WDT_TIME_OUT, REG_WDT_VAL, REG_WDT_CFG, REG_WDT_CTRL };
	const size_t WDT_REGS_CNT = sizeof(WDT_REGS) / sizeof(WDT_REGS[0]);
		
	// zero them out
	for ( size_t i = 0; i < WDT_REGS_CNT; ++i ) {
		outb( sch5127_regs + WDT_REGS[i], 0 );
	}
	
	if(debug)
		printf("In %s and disabled watchdog registers - about to return from call line %d \n", __FUNCTION__, __LINE__);

	return 1;
};
/////////////////////////////////////////////////////////////////////////
/// attempt to initialise HP EX48x and EX49x device LED
size_t init_hpex49x_led(void) 
{	
	int bits1 = 0, bits2 = 0;
	// ISA bridge [0601]: Intel Corporation 82801IR (ICH9R) LPC Interface Controller [8086:2916] (rev 02)
	const unsigned int HPEX49X = 0x29168086;

	const int IO_LEDS_BLUE[] = { OUT_BLUE0, OUT_BLUE1, OUT_BLUE2, OUT_BLUE3 };
	const int IO_LEDS_RED[] = { OUT_RED0, OUT_RED1, OUT_RED2, OUT_RED3 };

	out_system_blue = OUT_SYSTEM_BLUE;
	out_system_red = OUT_SYSTEM_RED;
	
	if(debug)
		printf("\n\nIn %s line %d - initializing LED registers. About to initialize SCH5127\n", __FUNCTION__, __LINE__);

	if ( !initsch5127(HPEX49X) ) 
		return 0;

	/////////////////////////////////////////////////////////////////////////
	/// enable LEDs for HP
	for ( size_t i = 0; i < MAX_HDD_LEDS; ++i ) {
		setbits32( ioledblue( i ), &bits1, &bits2 );
		setbits32( ioledred( i ),  &bits1, &bits2 );
	}	

	setbits32( OUT_USB_DEVICE,  &bits1, &bits2 );
	setbits32( OUT_SYSTEM_BLUE, &bits1, &bits2 );
	setbits32( OUT_SYSTEM_RED,  &bits1, &bits2 );
	
	setgpioselinput( bits1, bits2 );

	if(debug)
		printf("In %s() line %d performed I/O port initialization\n",__FUNCTION__, __LINE__);

	for(int i = 0; i < hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].HDD) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].HDD) - 1) ];
	}
	
	if(debug)
		printf("In %s() line %d set hpex49x[].blue and hpex49x[].red values - about to return\n",__FUNCTION__, __LINE__);
	
	return 1;
};
/////////////////////////////////////////////////////////////////////////////
//// initialize the Acer Altos
size_t init_acer_altos(void)
{
	// ISA bridge [0601]: Intel Corporation 82801IR (ICH9R) LPC Interface Controller [8086:2916] (rev 02)
	const unsigned int ALTOS = 0x27B88086;
	int bits1 = 0, bits2 = 0;

	const int IO_LEDS_BLUE[] = { ALTOS_BLUE0, ALTOS_BLUE1, ALTOS_BLUE2, ALTOS_BLUE3 };
	const int IO_LEDS_RED[] = { ALTOS_RED0, ALTOS_RED1, ALTOS_RED2, ALTOS_RED3 };

	out_system_blue = ALTOS_SYSTEM_BLUE;
	out_system_red = ALTOS_SYSTEM_RED;

	if ( !initsch5127(ALTOS) ) 
		return 0;

	setbits32( ALTOS_USB_DEVICE, &bits1, &bits2 );
	setbits32( ALTOS_USB_LED,	 &bits1, &bits2 );
	setbits32( ALTOS_POWER,		 &bits1, &bits2 );
	setbits32( ALTOS_SYSTEM_BLUE,&bits1, &bits2 );
	setbits32( ALTOS_SYSTEM_RED, &bits1, &bits2 );
		
	setgpioselinput( bits1, bits2 );

	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);
	
	for(int i = 0; i < hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].HDD) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].HDD) - 1) ];
	}

	return 1;
}
//////////////////////////////////////////////////////////////////////////////
//// initialize the H340 
size_t init_h340(void)
{
	int bits1 = 0, bits2 = 0;
	// ISA bridge [0601]: Intel Corporation 82801IR (ICH9R) LPC Interface Controller [8086:2916] (rev 02)
	const unsigned int H340 = 0x27B88086;

	const int IO_LEDS_BLUE[] = { H340_BLUE0, H340_BLUE1, H340_BLUE2, H340_BLUE3 };
	const int IO_LEDS_RED[] = { H340_RED0, H340_RED1, H340_RED2, H340_RED3 };

	out_system_blue = H340_SYSTEM_BLUE;
	out_system_red = H340_SYSTEM_RED;

	if ( !initsch5127(H340) ) 
		return 0;

	setbits32( H340_USB_DEVICE,	&bits1, &bits2 );
	setbits32( H340_USB_LED,	&bits1, &bits2 );
	setbits32( H340_POWER,		&bits1, &bits2 );
	setbits32( H340_SYSTEM_BLUE,&bits1, &bits2 );
	setbits32( H340_SYSTEM_RED,	&bits1, &bits2 );
		
	setgpioselinput( bits1, bits2 );

	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);

	for(int i = 0; i < hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].HDD) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].HDD) - 1) ];
	}

	return 1;
}
//////////////////////////////////////////////////////////////////////////////
//// initialize the H341/H342
size_t init_h341(void)
{
	int bits1 = 0, bits2 = 0;
	const unsigned int H341 = 0x29168086;

	const int IO_LEDS_BLUE[] = { H341_BLUE0, H341_BLUE1, H341_BLUE2, H341_BLUE3 };
	const int IO_LEDS_RED[] = { H341_RED0, H341_RED1, H341_RED2, H341_RED3 };

	out_system_blue = H341_SYSTEM_BLUE;
	out_system_red = H341_SYSTEM_RED;

	if ( !initsch5127(H341) ) 
		return 0;

	setbits32( H341_USB_DEVICE,	&bits1, &bits2 );
	setbits32( H341_USB_LED,	&bits1, &bits2 );
	setbits32( H341_POWER,		&bits1, &bits2 );
	setbits32( H341_SYSTEM_BLUE,&bits1, &bits2 );
	setbits32( H341_SYSTEM_RED,	&bits1, &bits2 );
		
	setgpioselinput( bits1, bits2 );

	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);
	
	for(int i = 0; i < hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].HDD) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].HDD) - 1) ];
	}

	return 1;
}
////////////////////////////////////////////////////////////////////
//// setbit32 function
void setbits32( int bit, int *bits1, int *bits2 ) 
{
	int *bits;
	bits = (bit < 32) ? bits1 : bits2;
	*bits |= 1 << bit;
}
/////////////////////////////////////////////////////////////////////
//// Set GPL Level
void setgplpllvl( int bit, int state ) 
{
	dobits( (1 << (bit % 32)), gpiobase + ((bit < 32) ? GP_LVL : GP_LVL2),state);
};
//////////////////////////////////////////////////////////////////////
//// Set General Purpose Registers
void setgpregslvl( int bit, int state ) 
{
	const int reg = ((bit >> 4) & 0xF) - 1;
	assert( reg >= 0 );
						
	dobits( (1 << (bit & 0xF)), sch5127_regs + REG_GP1 + reg, state );
};
/////////////////////////////////////////////////////////
//// Set the bits
void dobits( unsigned int bits, unsigned int port, int state ) 
{
	const unsigned int val = inl( port );
	const unsigned int new_val = ( state ) ? val | bits : val & ~bits;

	if ( val != new_val ) outl( port, new_val );
};
////////////////////////////////////////////////////////
//// Set GPIO Select Input
void setgpioselinput( int bits1, int bits2 ) 
{
	// Use Select (0 = native function, 1 = GPIO)

	const unsigned int gpio_use_sel  = gpiobase + GPIO_USE_SEL;
	const unsigned int gpio_use_sel2 = gpiobase + GPIO_USE_SEL2;
	
	outl( gpio_use_sel, inl(gpio_use_sel)  | bits1 );
	outl( gpio_use_sel2, inl(gpio_use_sel2) | bits2 );
	
	// Input/Output select (0 = Output, 1 = Input)
	
	const unsigned int gp_io_sel  = gpiobase + GP_IO_SEL;
	const unsigned int gp_io_sel2 = gpiobase + GP_IO_SEL2;
		
	outl( gp_io_sel, inl(gp_io_sel) & ~bits1 );
	outl( gp_io_sel2, inl(gp_io_sel2) & ~bits2 );
			
};
/////////////////////////////////////////////////////////////////////////
/// function to set the System LED (blinking light on front of HPEX4xx) pass blue, red 
/// or combine turning each on at once for purple
void setsystemled( int led_type, int state ) 
{
	const int on_off_state = ( LED_ON == state );
	if ( led_type & LED_BLUE ) setgplpllvl( out_system_blue, !on_off_state );
	if ( led_type & LED_RED  ) setgplpllvl( out_system_red,  !on_off_state );
	
	const int blink_state  = ( LED_BLINK == state );
	int val = 0;
	if ( led_type & LED_BLUE ) val |= 1 << out_system_blue;
	if ( led_type & LED_RED  ) val |= 1 << out_system_red;
	if ( val ) dobits( val, gpiobase + GPO_BLINK, blink_state );
};
/////////////////////////////////////////////////////////////////////////
/// set brightness level
/// @param val Brightness level from 0 to 9
void setbrightness( int val ) 
{
	/// SCH5127 Hardware monitoring register set
	static const unsigned int HWM_PWM3_DUTY_CYCLE = 0x32;	///< PWM3 Current Duty Cycle
	static const unsigned char LED_BRIGHTNESS[] = { 0x00, 0xbe, 0xc3, 0xcb, 0xd3, 0xdb, 0xe3, 0xeb, 0xf3, 0xff };
	val = fmax( 0, fmin( val, sizeof(LED_BRIGHTNESS) / sizeof(LED_BRIGHTNESS[0]) - 1 ) );
	outb( sch5127_regs + REG_HWM_INDEX, HWM_PWM3_DUTY_CYCLE );
	outb( sch5127_regs + REG_HWM_DATA, LED_BRIGHTNESS[val] );
};
/////////////////////////////////////////////////////////////////////////
/// blue LED mappings for HP disks only - *UNUSED*
int ioledblue( size_t led_idx )
{
	const int IO_LEDS_BLUE[] = { OUT_BLUE0, OUT_BLUE1, OUT_BLUE2, OUT_BLUE3 };
	assert( led_idx < MAX_HDD_LEDS );
	return IO_LEDS_BLUE[ led_idx ];
};
/////////////////////////////////////////////////////////////////////////
/// red LED mappings for HP disks only - *UNUSED*
int ioledred( size_t led_idx )
{
	const int IO_LEDS_RED[] = { OUT_RED0, OUT_RED1, OUT_RED2, OUT_RED3 };
	assert( led_idx < MAX_HDD_LEDS );
	return IO_LEDS_RED[ led_idx ];
};
/////////////////////////////////////////////////////////////////////////
/// function to set the LEDs for HP devices - pass blue, red 
/// or combine turning each on at once for purple
/// gcc atomic built-in while(__sync_lock_test_and_set(&hpex49xgpio_lock2, 1));	and __sync_lock_release(&hpex49xgpio_lock2);
void set_hpex_led( int led_type, int state, size_t led )
{
	if( (pthread_spin_lock(&hpex49x_gpio_lock2)) == EDEADLK ) {
		thread_run = 0; /* nuclear option - this should never happen */
		pthread_spin_unlock(&hpex49x_gpio_lock2);
		err(1,"Deadlock condition returned from pthread_spin_lock in %s line %d", __FUNCTION__, __LINE__);
	}

	if ( led_type & LED_BLUE ) setgplpllvl( led, !state );
	if ( led_type & LED_RED  ) setgplpllvl( led, !state );

	if( (pthread_spin_unlock(&hpex49x_gpio_lock2)) != 0)
		err(1, "Invalid return from pthread_spin_unlock in %s line %d", __FUNCTION__, __LINE__);
};
/////////////////////////////////////////////////////////////////////////
/// control leds
/// @param led_type LED type to turn on/off LED_BLUE, LED_RED, LED_BLUE | LED_RED
/// @param led Which LED to turn on/off (0 -> 3)
/// @param state Whether we are turning LED on or off 
void set_acer_led( int led_type, int state, size_t led ) 
{
	if( (pthread_spin_lock(&hpex49x_gpio_lock2)) == EDEADLK ){
		thread_run = 0; /* nuclear option - this should never happen */
		err(1,"Invalid return from pthread_spin_lock in %s line %d",  __FUNCTION__, __LINE__);
	}

	if ( led_type & LED_BLUE ) setgpregslvl( led, state );
	if ( led_type & LED_RED  ) setgpregslvl( led, state );

	if( (pthread_spin_unlock(&hpex49x_gpio_lock2)) != 0)
		err(1, "Invalid return from pthread_spin_unlock in %s line %d", __FUNCTION__, __LINE__);
};
