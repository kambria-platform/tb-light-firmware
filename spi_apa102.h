/*
 * OhmniLabs SPI apa102 interface
 *
 * Jared
 */
#ifndef __SPI_APA102_H__
#define __SPI_APA102_H__

#define PREAMBLE_LENGTH 8
#define LEDCOUNT 256
#define LEDBUF_SIZE (LEDCOUNT*4)

struct spi_data
{
	/* Transmission index */
	int txbyte;
	int rxbyte;

	/* Transmission offset */
	int txoffset;

	/* Individual bytes read */
	uint8_t rbuf[2];

	/* Number of values read */
	uint32_t nwritten;
	uint32_t nread;

	/* Global color or buffer */
	int use_global_color;

	/* Global color frame as hsv and cached as rgb for sending */
	uint8_t global_hsv[3];
	uint8_t global_rgb[4];

	/* Support up to N LEDs in buffered mode */
	uint8_t ledbuf[LEDBUF_SIZE];
};


/* Global structure */

extern struct spi_data _spi;


/* Methods */

void spi_setup();
void spi_build_data();
void spi_poll();
uint8_t spi_nextbyte();
void spi_interrupt(void);

void spi_set_global_hsv(uint8_t h, uint8_t s, uint8_t v);

#endif

