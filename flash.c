/*
 * OhmniLabs flash/settings persistence library
 */

#include "Cpu.h"
#include "Events.h"
#include "PE_Types.h"

#include "SIM_PDD.h"
#include "FTMRH_PDD.h"

#include "flash.h"


/* ---- Helpers for interrupt control ---- */

/* Macro to enable all interrupts. */
#define EnableInterrupts asm ("CPSIE  i")
 
/* Macro to disable all interrupts. */
#define DisableInterrupts asm ("CPSID  i")


/* ---- Global flash memory copy ---- */

struct flashdata _flash = {0};


/* ---- Globals ---- */

// Status checks for flash write.  For some reason putting these
// in seems to make the flashing much more reliable.  Not 100% sure
// why yet but let's leave it for now.
struct flashstat {
	int s1, s2, s3;
};

struct flashstat _fs = {
	0xf00d0000,
	0xf00d0000,
	0xf00d0000
};


/* ---- Methods ---- */

void flash_load_to_ram()
{
	// Enable clock
	SIM_PDD_SetClockGate(SIM_BASE_PTR, SIM_PDD_CLOCK_GATE_FTMRH, PDD_ENABLE);

	// Wait till we are ready
	flash_wait();

	// Set up FCLKDIV to result in 1MHz, see 18.3.4.1
	FTMRH_PDD_WriteClockDividerReg(FTMRH_BASE_PTR, 0x13);

	// Now read in 4 byte chunks
	uint32_t* maddr = (uint32_t*)&_flash;
	int faddr = 0x7c00;
	for (int i = 0; i < sizeof(_flash); i += 4) {
		*(maddr++) = *(uint32_t*)(faddr);
		faddr += 4;
	}
}

// Launch and wait
__attribute__((used, long_call, section(".ramfunc"))) void flash_wait()
{
	while (!(FTMRH_PDD_ReadStatusReg(FTMRH_BASE_PTR) & FTMRH_FSTAT_CCIF_MASK)) {}
}

// Launch and wait
__attribute__((used, long_call, section(".ramfunc"))) void flash_launch()
{	
	DisableInterrupts;
	FTMRH_PDD_LaunchCommand(FTMRH_BASE_PTR);
	while (!(FTMRH_PDD_ReadStatusReg(FTMRH_BASE_PTR) & FTMRH_FSTAT_CCIF_MASK)) {}
	EnableInterrupts;
}

/* Note: blocking.  writing flash fail if you read anything */
void flash_save_to_flash()
{
	// Check if we are ready to write
	flash_wait();

	// See if we need to clear either of the error bits
	if (FTMRH_PDD_ReadStatusReg(FTMRH_BASE_PTR) & 
		(FTMRH_FSTAT_FPVIOL_MASK | FTMRH_FSTAT_ACCERR_MASK) ) {
		FTMRH_PDD_WriteStatusReg(FTMRH_BASE_PTR, 
			(FTMRH_FSTAT_FPVIOL_MASK | FTMRH_FSTAT_ACCERR_MASK));
	}

	// Erase sectors 0x7c00 - 0x7fff with this, the last 1k
	FTMRH_PDD_SetCommandAndAddress(FTMRH_BASE_PTR, 0x0a, 0x7c00);
	flash_launch();
	FTMRH_PDD_SetCommandAndAddress(FTMRH_BASE_PTR, 0x0a, 0x7e00);
	flash_launch();
	
	// Now write the bytes, this automatically handles non 4-byte sized struct
	uint8_t* raddr = (uint8_t*)&_flash;
	int waddr = 0x7c00;
	int step = 4;
	for (int i = 0; i < sizeof(_flash); i += step) {

		// Now write value here
		FTMRH_PDD_SetCommandAndAddress(FTMRH_BASE_PTR, 0x06, waddr);
		FTMRH_PDD_SetProgrammedWord(FTMRH_BASE_PTR, 2, raddr[1], raddr[0]);
		FTMRH_PDD_SetProgrammedWord(FTMRH_BASE_PTR, 3, raddr[3], raddr[2]);		
		flash_launch();

		// Increase writeaddr and our read ptr
		waddr += step;	
		raddr += step;
	}
	
}
