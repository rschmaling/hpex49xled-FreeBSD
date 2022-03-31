include "hpex49x_led.h"

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
	
	if ( vendor != did_vid ) return 0;
		
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
size_t init_hpex49x(void) 
{	
	int bits1 = 0, bits2 = 0;
	// ISA bridge [0601]: Intel Corporation 82801IR (ICH9R) LPC Interface Controller [8086:2916] (rev 02)
	const unsigned int HPEX49X = 0x29168086;
	// Thread IDs
	// pthread_t tid;
	// Create Thread Attributes
	// pthread_attr_t attr;
	// const int IO_LEDS_BLUE[] = { OUT_BLUE0, OUT_BLUE1, OUT_BLUE2, OUT_BLUE3 };
	// const int IO_LEDS_RED[] = { OUT_RED0, OUT_RED1, OUT_RED2, OUT_RED3 };

	out_system_blue = OUT_SYSTEM_BLUE;
	out_system_red = OUT_SYSTEM_RED;

/*
	if ((pthread_attr_init(&attr)) < 0 )
		err(1, "Unable to execute pthread_attr_init(&attr) in %s line %d", __FUNCTION__, __LINE__);

	if( (pthread_create(&tid, &attr, disk_init, NULL)) != 0)
		err(1, "Unable to init disks in %s - bad return from disk_init() line %d", __FUNCTION__, __LINE__);
*/
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
/*
	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);
	if( (pthread_join(tid,(void**)&hpdisks)) != 0)
		err(1, "Unable to rejoin thread prior to execution in %s", __FUNCTION__);
	for(int i = 0; i < *hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].hphdd) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].hphdd) - 1) ];
	}
	if(debug)
		printf("Number of disks is: %d \n", *hpdisks);

	return *hpdisks;
*/	
	return 1;
};
/////////////////////////////////////////////////////////
//// Set 32 Bit Bits
void setbits32( int bit, int *bits1, int *bits2 ) 
{
	int *bits;
	bits = (bit < 32) ? bits1 : bits2;
	*bits |= 1 << bit;
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
//// Set GPL Level
void setgplpllvl( int bit, int state ) 
{
	dobits( (1 << (bit % 32)), gpiobase + ((bit < 32) ? GP_LVL : GP_LVL2),state);
};
////////////////////////////////////////////////////////
//// Set GP Registers
void setgpregslvl( int bit, int state ) 
{
	const int reg = ((bit >> 4) & 0xF) - 1;
	assert( reg >= 0 );
						
	dobits( (1 << (bit & 0xF)), sch5127_regs + REG_GP1 + reg, state );
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
/// function to set the LEDs - pass blue, red 
/// or combine turning each on at once for purple
/// gcc atomic built-in while(__sync_lock_test_and_set(&hpex49xgpio_lock2, 1));	and __sync_lock_release(&hpex49xgpio_lock2);
// void set_hpex_led( int led_type, int state, size_t led, pthread_t thread_id ) 
void set_hpex_led( int led_type, int state, size_t led )
{
//	if( (pthread_spin_lock(&hpex49x_gpio_lock2)) != 0 )
//		err(1,"Invalid return from pthread_spin_lock for thread %ld in %s line %d", thread_id, __FUNCTION__, __LINE__);
	if ( led_type & LED_BLUE ) setgplpllvl( led, !state );
	if ( led_type & LED_RED  ) setgplpllvl( led, !state );
//	if( (pthread_spin_unlock(&hpex49x_gpio_lock2)) != 0)
//		err(1, "Invalid return from pthread_spin_unlock for thread %ld in %s line %d", thread_id, __FUNCTION__, __LINE__);
};

int main (int argc, char **argv)
{
	if (geteuid() !=0 ) {
		printf("Try running as root to avoid Segfault and core dump \n");
		errx(1, "not running as root user");
	}
	if( ( io = open("/dev/io", O_RDWR)) < 0 ) 
		perror("Unable to open /dev/io");
	if(init_hpex49x() != 1)
		err(1, "unable to initialize HP 49x systems");
	setsystemled( LED_RED, LED_OFF );
	setsystemled( LED_BLUE, LED_OFF );
	set_hpex_led(LED_BLUE, OFF, OUT_BLUE0 );
	set_hpex_led(LED_BLUE, OFF, OUT_RED0 );
	if ( (close(io)) != 0)
		perror("Unable to close /dev/io file descriptor");
	return 0;
};
