/*
 * OhmniLabs SPI apa102 interface
 *
 * Jared
 */

/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"

/* Libraries */
#include "SIM_PDD.h"
#include "PORT_PDD.h"
#include "SPI_PDD.h"

/* Our source */
#include "systime.h"
#include "spi_apa102.h"
#include "flash.h"


/* ---- Global ---- */

struct spi_data _spi = {0};



/* ---- Methods ---- */

void spi_setup()
{
	/* Build our ouput data now */
	spi_build_data();

	/* Enable SPI clocks */
	SIM_PDD_SetClockGate(SIM_BASE_PTR, SIM_PDD_CLOCK_GATE_SPI0, PDD_ENABLE);

	/* Directly set up C1 and C2 registers */
	SPI_PDD_WriteControl1Reg(SPI0_BASE_PTR, 
		(0 << 0) | /* MSB first */
		(1 << 1) | /* Automatic SS */
		(1 << 2) | /* Phase */
		(1 << 3) | /* CPOL idles high */
		(1 << 4) | /* Master mode */
		(1 << 6) | /* SPI enable */
		(0 << 7)   /* No RX interrupts */
	);
	SPI_PDD_WriteControl2Reg(SPI0_BASE_PTR, 
		(1 << 4)
	);
	SPI_PDD_WriteBaudRateReg(SPI0_BASE_PTR, 
		//(3 << 0) /* 24 MHz / 16 = 1.5 MHz - uart seems to stop working at this rate, too much CPU usage */
		(5 << 0) /* 24 MHz / 64 = 375 kHz */
	);

	/* Clear and then enable interrupts for SPI, IRQ 10 */
	/* See sec 3.3.2.3 of http://cache.freescale.com/files/32bit/doc/ref_manual/KL04P48M48SF1RM.pdf */
	NVIC_ICPR = (1 << 10);
	NVIC_ISER = (1 << 10);

	/* Start in global color mode and off */
	_spi.use_global_color = 1;

	_spi.global_hsv[0] = 0x00;
	_spi.global_hsv[1] = 0x00;
	_spi.global_hsv[2] = 0x00;

	_spi.global_rgb[0] = 0xff;
	_spi.global_rgb[1] = 0x00;
	_spi.global_rgb[2] = 0x00;
	_spi.global_rgb[3] = 0x00;

	/* Bound npixels here for safety, for now set lower for our purposes */
	if (_flash.npixels > 32) _flash.npixels = 32;
	if (_flash.npixels < 1) _flash.npixels = 1;
}


/* HSV to RGB conversion function with only integer
 * math */
void
hsvtorgb(
	uint8_t h, uint8_t s, uint8_t v,
	uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t fpart, p, q, t;
    
    if(s == 0) {
        /* color is grayscale */
        *r = *g = *b = v;
        return;
    }
    
    /* make hue 0-5 */    
    uint8_t region = (uint8_t)(((uint32_t)h * 48771)/(1<<21)); //region = h / 43;    

    /* find remainder part, make it from 0-255 */
    fpart = (h - (region * 43)) * 6;
    
    /* calculate temp vars, doing integer multiplication */
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * fpart) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - fpart)) >> 8))) >> 8;
        
    /* assign temp vars based on color cone region */
    switch(region) {
        case 0:
            *r = v; *g = t; *b = p; break;
        case 1:
            *r = q; *g = v; *b = p; break;
        case 2:
            *r = p; *g = v; *b = t; break;
        case 3:
            *r = p; *g = q; *b = v; break;
        case 4:
            *r = t; *g = p; *b = v; break;
        default:
            *r = v; *g = p; *b = q; break;
    }
}


void spi_build_data()
{
	// Helper
	uint8_t *ledbuf = _spi.ledbuf;

	/* Generate all LEDs now */
	uint8_t r, g, b;
	uint8_t hue = 0;
	int sat = 255;
	int satdir = -3;
	for (int i = 0; i < sizeof(_spi.ledbuf); ) {

		// Convert here
		hsvtorgb(hue, sat, 16, &r, &g, &b);
		hue += 2;

		// Write here
		ledbuf[i++] = 0xff;
		ledbuf[i++] = r;
		ledbuf[i++] = g;
		ledbuf[i++] = b;

	}

}


void spi_set_global_hsv(uint8_t h, uint8_t s, uint8_t v)
{
	// For now force on global mode if it's not on
	_spi.use_global_color = 1;

	// Bound for max current
	if (v > 120) v = 120;

	// Save this
	_spi.global_hsv[0] = h;
	_spi.global_hsv[1] = s;
	_spi.global_hsv[2] = v;

	// Translate now for sending out
	uint8_t r, g, b;
	hsvtorgb(h, s, v, &r, &g, &b);
	_spi.global_rgb[1] = b;
	_spi.global_rgb[2] = g;
	_spi.global_rgb[3] = r;
}


void spi_poll()
{

	/* Check if can start a new cycle */
	if (_spi.txbyte == 0) {
		 if (SPI_PDD_ReadStatusReg(SPI0_BASE_PTR) & SPI_S_SPTEF_MASK) {

		 	/* Enable TX interrupts */
		 	SPI_C1_REG(SPI0_BASE_PTR) |= (1 << 5);		 	

		 	/* Increment the byte written first */
		 	SPI_PDD_WriteData8Bit(SPI0_BASE_PTR, spi_nextbyte());
		 	_spi.txbyte++;
		 }
	}

}


uint8_t spi_nextbyte()
{
	// Handle global case
	if (_spi.use_global_color) {
		return _spi.txbyte < PREAMBLE_LENGTH ? 
			0x00 : _spi.global_rgb[(_spi.txbyte - PREAMBLE_LENGTH) & (0x3)];
	}

	// Handle writing from buffer, with buffer offset
	return _spi.txbyte < PREAMBLE_LENGTH ? 
		0x00 : _spi.ledbuf[(_spi.txbyte - PREAMBLE_LENGTH + _spi.txoffset) & (LEDBUF_SIZE-1)];
}


/* Interrupt handler for SPI */
PE_ISR(spi_interrupt)
{
	/* Read status reg */
	uint32_t reg = SPI_PDD_ReadStatusReg(SPI0_BASE_PTR);

	/* Handle write completion */
	if (reg & SPI_S_SPTEF_MASK) {
		if (_spi.txbyte < PREAMBLE_LENGTH + (_flash.npixels * 4)) {

			/* Send the next byte */			
			SPI_PDD_WriteData8Bit(SPI0_BASE_PTR,  spi_nextbyte());
			_spi.txbyte++;

		} else {

			/* Disable tx interrupts now that we are done */
			SPI_C1_REG(SPI0_BASE_PTR) &= ~(1 << 5);

			/* Stamp the tx completion here, poll will trigger next */
			_spi.txbyte = 0;	
			_spi.nwritten++;		
			//_spi.txoffset = (_spi.txoffset + 4) & (LEDBUF_SIZE-1); 
		}
	}

	/* Handle reads */
	if (reg & SPI_S_SPRF_MASK) {

		/* Read in the byte */
		_spi.rbuf[0] = SPI_PDD_ReadData8bit(SPI0_BASE_PTR);

	}
}
