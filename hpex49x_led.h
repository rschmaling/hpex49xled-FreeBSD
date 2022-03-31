#ifndef INCLUDED_HPEX49XLED_LED
#define INCLUDED_HPEX49XLED_LED
/////////////////////////////////////////////////////////////////////////////
/////// @file hpex49x_led.h
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
const char* desc(void);
size_t initsch5127(const unsigned int vendor);
void setbits32( int bit, int *bits1, int *bits2 );
void dobits( unsigned int bits, unsigned int port, int state );
void setgplpllvl( int bit, int state );
void setgpregslvl( int bit, int state );
void setbrightness( int val );
void setsystemled( int led_type, int state );
size_t init_hpex49x_led(void);
void set_acer_led( int led_type, int state, size_t led );
void set_hpex_led( int led_type, int state, size_t led );
size_t init_acer_altos_led(void);
size_t init_h340_led(void);
size_t init_h341_led(void);
int ioledblue( size_t led_idx );
int ioledred( size_t led_idx );
void setgpioselinput( int bits1, int bits2 );

/* some constants and globals */
unsigned int gpiobase; ///< I/O offset to LPC GPIO on the IHR9
unsigned int sch5127_regs; ///< I/O offset to SCH5127 runtime registers
const unsigned int pci_addr = 0x0CF8;
const unsigned int pci_data = 0x0CFC;
// PCI configuration space
const unsigned int PCI_CONFIG_ADDRESS = 0x0CF8;
const unsigned int PCI_CONFIG_DATA = 0x0CFC;
size_t out_system_blue; // determined by get_led_interface
size_t out_system_red; // determined by get_led_interface 
const char *hardware = "HP Mediasmart EX49x";
extern size_t debug;
extern size_t hpdisks;
extern size_t thread_run;

/// SCH5127 Runtime Registers
enum {
	REG_GP1		 = 0x4B,	///< General Purpose I/O Data Register 1
	REG_GP2		 = 0x4C,	///< General Purpose I/O Data Register 2
	REG_GP3		 = 0x4D,	///< General Purpose I/O Data Register 3
	REG_GP4		 = 0x4E,	///< General Purpose I/O Data Register 4
	REG_GP5		 = 0x4F,	///< General Purpose I/O Data Register 5
	REG_GP6		 = 0x50,	///< General Purpose I/O Data Register 6
		
	REG_WDT_TIME_OUT = 0x65,	///< Watch-dog timeout
	REG_WDT_VAL	 = 0x66,	///< Watch-dog timer time-out Value
	REG_WDT_CFG	 = 0x67,	///< Watch-dog timer Configuration
	REG_WDT_CTRL	 = 0x68,	///< Watch-dog timer Control

	REG_HWM_INDEX	 = 0x70,	///< HWM Index Register (SCH5127 runtime register)
	REG_HWM_DATA	 = 0x71,	///< HWM Data Register (SCH5127 runtime register)
};	
/////////////////////////////////////////////////////////////////////////
/// IHR9 General Purpose I/O Registers
enum {
	GPIO_USE_SEL = 0x00, ///< GPIO Use Select
	GP_IO_SEL = 0x04, ///< GPIO Input/Output Select
	// = 0x08, ///< reserved
	GP_LVL = 0x0C,	///< GPIO Level for Input or Output
	// = 0x10, ///< reserved
	// = 0x14, ///< reserved
	GPO_BLINK = 0x18, ///< GPIO Blink Enable
	// GP_SER_BLINK	= 0x1C,	///< GP Serial Blink [31:0]
	// GP_SB_CMDSTS	= 0x20,	///< GP Serial Blink Command Status [31:0]
	// GP_SB_DATA = 0x24,	///< GP Serial Blink Data [31:0]
	// = 0x28, ///< reserved
	// GPI_INV = 0x2C, ///< GPIO Signal Invert
	GPIO_USE_SEL2 = 0x30, ///< GPIO Use Select 2 [60:32]
	GP_IO_SEL2 = 0x34,	///< GPIO Input/Output Select 2 [60:32]
	GP_LVL2	= 0x38,	///< GPIO Level for INput or Output 2 [60:32]
	// = 0x3C, ///< reserved
};

//////////////////////////////////////////////////////////////////////////
//// bit mappings for HP EX48x EX49x LEDs
enum led_colors {
	OUT_BLUE0		= 22,
	OUT_BLUE1		= 21,
	OUT_BLUE2		= 13,
	OUT_BLUE3		= 57,

	OUT_RED0		= 4,
	OUT_RED1		= 5,
	OUT_RED2		= 38,
	OUT_RED3		= 39,

	OUT_USB_DEVICE	= 7,
	OUT_SYSTEM_BLUE	= 28,
	OUT_SYSTEM_RED	= 27,
};

enum other_led_colors {
	H340_BLUE0		= 0x56,	///< register 5, bit 6
	H340_BLUE1		= 0x52,	///< register 5, bit 2
	H340_BLUE2		= 0x50,	///< register 5, bit 0
	H340_BLUE3		= 0x14,	///< register 1, bit 4
		
	H340_RED0		= 0x57,	///< register 5, bit 7
	H340_RED1		= 0x53,	///< register 5, bit 3
	H340_RED2		= 0x51,	///< register 5, bit 1
	H340_RED3		= 0x11,	///< register 1, bit 1

	H340_USB_DEVICE	= 0x06,	///< bit 6
		
	H340_USB_LED		= 0x1B,	///< bit 27
	H340_POWER		= 0x19,	///< bit 25
	H340_SYSTEM_RED	= 0x18,	///< bit 24
	H340_SYSTEM_BLUE	= 0x14,	///< bit 20

	H341_BLUE0       = 0x4b,
    H341_BLUE1       = 0x4c,
    H341_BLUE2       = 0x52,
    H341_BLUE3       = 0x50,

    H341_RED0        = 0x59,
    H341_RED1        = 0x58,
    H341_RED2        = 0x4e,
    H341_RED3        = 0x51,

    H341_USB_DEVICE	= 0x06, ///< bit 6
    H341_USB_LED     = 0x12,
    H341_POWER       = 0x1b,
    H341_SYSTEM_RED  = 0x18,
    H341_SYSTEM_BLUE = 0x0A,

	ALTOS_BLUE0		= 0x14,	///< register 1, bit 4
	ALTOS_BLUE1		= 0x50,	///< register 5, bit 0
	ALTOS_BLUE2		= 0x52,	///< register 5, bit 2
	ALTOS_BLUE3		= 0x56,	///< register 5, bit 6
		
	ALTOS_RED0		= 0x11,	///< register 1, bit 1
	ALTOS_RED1		= 0x51,	///< register 5, bit 1
	ALTOS_RED2		= 0x53,	///< register 5, bit 3
	ALTOS_RED3		= 0x57,	///< register 5, bit 7
		
	ALTOS_USB_DEVICE	= 0x06,	///< bit 6
	ALTOS_USB_LED		= 0x1B,	///< bit 27
	ALTOS_POWER			= 0x19,	///< bit 25
	ALTOS_SYSTEM_RED	= 0x18,	///< bit 24
	ALTOS_SYSTEM_BLUE	= 0x14,	///< bit 20
};

#endif //INCLUDED_HPEX49XLED_LED