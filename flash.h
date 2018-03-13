/*
 * OhmniLabs flash/settings persistence library
 */
#ifndef __FLASH_H__
#define __FLASH_H__


/* ---- Global ---- */

/* 
 * Flash data structure.  Don't change the order without
 * leaving appropriate gaps or old data will become invalid!
 * Everything here is aligned to 4 bytes, though some fields
 * may need less.
 */
struct flashdata {

	// item 1: Bus id 0 - 253
	uint32_t sid;		

	// Item 2: Number of pixels to drive
	uint32_t npixels;

	// Item 3: startup hue
	uint8_t boot_hue;


} __attribute__((__packed__));

extern struct flashdata _flash;


/* ---- Methods ---- */

void flash_load_to_ram();
void flash_save_to_flash();

__attribute__((used, long_call, section(".ramfunc"))) void flash_wait();
__attribute__((used, long_call, section(".ramfunc"))) void flash_launch();

#endif

