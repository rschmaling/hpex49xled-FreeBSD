#ifndef INCLUDED_HPLED
#define INCLUDED_HPLED
/////////////////////////////////////////////////////////////////////////////
/////// @file hpled.h
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
struct hpled
{
	u_int64_t b_read;
	u_int64_t b_write;
	u_int64_t n_read; /* I don't like having to do this but I don't want a race condition */
	u_int64_t n_write; 
	size_t target_id;
	size_t path_id;
	size_t blue;
	size_t red;
	size_t dev_index;
	int HDD;
	char path[12];
};

#define LED_DELAY 50000000 // for nanosleep() struct timespec - delay for turning off LEDs in nanoseconds
#define BLINK_DELAY 8500000 // for nanosleep() struct timespec - blink delay to indicate activity
#define MAX_HDD_LEDS 4 // Maximum number of Drives to work on - four bays in the HPEX49x and HPEX48x

/////////////////////////////////////////////////////////////////////////
// LED definitions
enum ledcolor {
	LED_BLUE	= 1 << 0,
	LED_RED		= 1 << 1,
};

enum ledstate {
	LED_OFF		= 1 << 0,
	LED_ON		= 1 << 1,
	LED_BLINK	= 1 << 2,
};

enum bstate { 
	OFF = 0,
	ON = 1,
};

#endif //INCLUDED_HPLED