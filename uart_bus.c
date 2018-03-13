/*
 * OhmniLabs new common UART/api
 */

#include "Cpu.h"
#include "Events.h"
#include "PE_Types.h"

#include "SIM_PDD.h"
#include "PORT_PDD.h"
#include "UART_PDD.h"

#include "systime.h"
#include "flash.h"
#include "uart_bus.h"
#include "spi_apa102.h"
#include "anim_engine.h"


/* ---- Defines ---- */

// Set garbage detection timeout
#define UART_TIMEOUT (1500 * 5)

// Commands
#define CMD_EEP_WRITE	0x01
#define CMD_EEP_READ	0x02
#define CMD_RAM_WRITE	0x03
#define CMD_RAM_READ	0x04
#define CMD_I_JOG		0x05
#define CMD_S_JOG		0x06
#define CMD_STAT 		0x07
#define CMD_ROLLBACK 	0x08
#define CMD_REBOOT		0x09

// Port to use, currently UART1
#define CLOCK_GATE SIM_PDD_CLOCK_GATE_UART1
#define BASE_PTR UART1_BASE_PTR


/* ---- Global ---- */

struct uart_data _uart = {0};


/* ---- Methods ---- */

/* UART setup routine */
void uart_setup()
{
	/* Enable clock to the uart */
	SIM_PDD_SetClockGate(SIM_BASE_PTR, CLOCK_GATE, PDD_ENABLE);

	/* Disable both at start */	
	UART_PDD_EnableTransmitter(BASE_PTR, PDD_DISABLE);
	UART_PDD_EnableReceiver(BASE_PTR, PDD_DISABLE);

	/* TODO: Reset flags? */
	UART_C1_REG(BASE_PTR) = 0x00U;
	UART_C3_REG(BASE_PTR) = 0x00U;
	UART_S2_REG(BASE_PTR) = 0x00U;	

	/* Set up for 115200 baud */	
	UART_PDD_SetBaudRate(BASE_PTR, 11U);	

	/* Turn it on now */
	/* NOTE: until we switch to real open-drain, transmitting is a risk, keep off */
	//UART_PDD_EnableTransmitter(BASE_PTR, PDD_ENABLE);
	UART_PDD_EnableReceiver(BASE_PTR, PDD_ENABLE);
}


/* Payload validation for read/write commands */
int uart_validate_payload_read()
{
	// Make sure we have both addr and size in the payload
	if (_uart.cbufreq < 6)
		return 0;

	// Okay
	return 1;
}

int uart_validate_payload_write()
{
	// Make sure we have both addr and size for payload
	if (_uart.cbufreq < 6)
		return 0;

	// Check payload length versus packet length
	// to make sure our bytes are readable
	uint8_t plen = _uart.cbuf[5];
	if (_uart.cbufreq < 6 + plen) {
		return 0;
	}

	// Okay
	return 1;
}

/* Helper for read commands to build reply buffer */
void uart_build_reply(void *rdata, int rlen)
{
	uint8_t *req = _uart.cbuf;
	uint8_t *b = _uart.obuf;

	// Preamble
	*b++ = 0xff;
	*b++ = 0xff;
	
	// Size, SID, and message type with reply bit
	int msglen = 9 + rlen + 2;
	*b++ = msglen;
	*b++ = req[0];
	*b++ = req[1] | 0x40;

	// Zero checksums for now
	*b++ = 0;
	*b++ = 0;

	// Address copied over, use our reply len
	*b++ = req[4];
	*b++ = rlen;

	// Copy over reply data
	for (int i = 0; i < rlen; i++) {
		*b++ = ((uint8_t*)rdata)[i];
	}

	// Status error and detail
	*b++ = 0x00;
	*b++ = 0x00;

	// Compute checksum now
	uint8_t cs = 0x00;
	for (int i = 2; i < msglen; i++) {
		cs ^= _uart.obuf[i];
	}

	// Write the checksums here
	_uart.obuf[5] = cs & 0xfe;
	_uart.obuf[6] = ~cs & 0xfe;

	// Start the reply now
	_uart.obufpos = 0;
	_uart.obuflen = msglen;

}

/* Split message handlers */
void uart_handle_cmd_eep_write()
{
	uint8_t *b = _uart.cbuf;
	uint8_t addr = b[4];
	uint8_t plen = b[5];
	uint8_t *data = b + 6;

	switch(addr) {

		// Handle writing servo ID
		case 6:
			if (plen == 1 && data[0] <= 0xfd) {
				_flash.sid = data[0];
				flash_save_to_flash();
			}
			break;

		// Special light control, set number of
		// pixels to drive and bake into flash.  We only
		// allow up to 255 for now but we use 16-bit for extensibility
		case 0xc0:
			if (plen == 2) {

				// Grab and bound
				int val = data[0] | (data[1] << 8);
				if (val < 1) val = 1;
				if (val > 255) val = 255;

				// Save here
				_flash.npixels = val;
				flash_save_to_flash();
			}
			break;

		// Set up the startup hue
		case 0xc4:
			if (plen == 1) {
				_flash.boot_hue = data[0];
				flash_save_to_flash();
			}
			break;

	}
}

void uart_handle_cmd_eep_read()
{
	uint8_t *b = _uart.cbuf;
	uint8_t addr = b[4];
	uint8_t plen = b[5];
	uint8_t *data = b + 6;
}

void uart_handle_cmd_ram_write()
{
	uint8_t *b = _uart.cbuf;
	uint8_t addr = b[4];
	uint8_t plen = b[5];
	uint8_t *data = b + 6;

	switch(addr) {

		// Handle writing LED as test param
		case 53:
			if (plen == 1) {
				_uart.ledval = data[0];				
			}
			break;

		// Special light control, set global HSV directly
		case 0xc1:
			if (plen == 3) {
				// Set HSV here
				spi_set_global_hsv(data[0], data[1], data[2]);
			}
			break;

		// Special light control, trigger a stored animation state
		case 0xc2:
			if (plen == 1) {				
				_anim.state = data[0];
				anim_state_begin();
			}
			break;

		// Special light control set up custom animation and play
		case 0xc3:
		{
			if (plen == 8) {				
				_anim.state = ANIM_STATE_ONESHOT;
				struct hsv start = { data[0], data[1], data[2] };
				struct hsv end = { data[3], data[4], data[5] };
				int frames = data[6] | (data[7] << 8);
				anim_begin(&start, &end, frames);
			}
			break;
		}

	}
}

void uart_handle_cmd_ram_read()
{
	uint8_t *b = _uart.cbuf;
	uint8_t addr = b[4];
	uint8_t plen = b[5];
	uint8_t *data = b + 6;

	switch(addr) {

		// Handle reading LED as a test param to set and get
		case 53:
			if (plen == 1) {				
				uart_build_reply(&_uart.ledval, 1);	
			}
			break;

	}
}


void uart_handle_cmd_reboot()
{
	// TODO: reset chip?
	//SCB_AIRCR = SCB_AIRCR_VECTKEY(0x05FA) | SCB_AIRCR_SYSRESETREQ(1) ;
	//while(1);
 
}



/* Handle completion of a variable read of bytes */
void uart_handle_read_complete()
{
	// Pointer for convenience
	uint8_t *b = _uart.cbuf;

	// Validate checksum
	if ((_uart.currcsum & 0xfe) != b[2] ||
		(~_uart.currcsum & 0xfe) != b[3]) {
		_uart.badsumcount++;
		return;
	}

	// Check if this message is for us
	if (b[0] != _flash.sid && b[0] != 0xfe) {
		return;
	}

	// Okay, message for us, handle here
	switch(b[1]) {

		case CMD_EEP_WRITE:
			if (!uart_validate_payload_write()) return;
			uart_handle_cmd_eep_write();
			break;		

		case CMD_EEP_READ:
			if (!uart_validate_payload_read()) return;
			uart_handle_cmd_eep_read();
			break;

		case CMD_RAM_WRITE:
			if (!uart_validate_payload_write()) return;
			uart_handle_cmd_ram_write();
			break;

		case CMD_RAM_READ:
			if (!uart_validate_payload_read()) return;
			uart_handle_cmd_ram_read();
			break;

		case CMD_REBOOT:
			uart_handle_cmd_reboot();
			break;
	}

}


/* Byte by byte handler */
void uart_handle_byte(uint8_t byte)
{
	// For now, pretty much every byte is a state transition
	// so we stamp the state_ts var anyways.  Instead of doing this
	// case by case just do it on every byte.
	_uart.state_ts = _systime.ticks;

	// Now handle state machine
	switch(_uart.state) {

		case UART_STATE_SEEK1: 
		{
			_uart.state = byte == 0xff ? UART_STATE_SEEK2 : UART_STATE_SEEK1;			
			break;
		}

		case UART_STATE_SEEK2:
		{
			_uart.state = byte == 0xff ? UART_STATE_GET_LENGTH : UART_STATE_SEEK1;
			break;
		}

		case UART_STATE_GET_LENGTH:
		{
			/* Check reasonable bounds on desired length */
			if (byte < 7 || byte > 64) {
				_uart.state = UART_STATE_SEEK1;
				return;
			}

			/* Set this as readn */
			_uart.cbufpos = 0;
			_uart.cbufreq = byte-3;			
			_uart.state = UART_STATE_READN;			

			/* Start running checksum */
			_uart.currcsum = byte;

			break;
		}

		case UART_STATE_READN:
		{			
			// Update checksum EXCEPT for csum bytes
			if (_uart.cbufpos != 2 && _uart.cbufpos != 3) 
				_uart.currcsum ^= byte;

			// Store the new byte
			_uart.cbuf[_uart.cbufpos++] = byte;

			// Check for finish
			if (_uart.cbufpos >= _uart.cbufreq) {

				// Call the completion here
				uart_handle_read_complete();

				// Go back to root
				_uart.state = UART_STATE_SEEK1;			
			}

			break;
		}
	}
}

/* Do uart polling work */
void uart_poll()
{
	/* Check if we have a char to read */
	uint8_t s1 = UART_PDD_ReadStatus1Reg(BASE_PTR);
	_uart.last_status = s1;

	if (s1 & UART_PDD_RX_DATA_FULL_FLAG) {
		uint8_t data = UART_PDD_ReadDataReg(BASE_PTR);

		// Set it now		
		_uart.readcount += 1;
		uart_handle_byte(data);
	}

#if 0
	// Trigger writes for testing resistor trim
	if (_uart.obuflen == 0) {
		_uart.obufpos = 0;
		_uart.obuflen = 4;
		_uart.obuf[0] = 0x55;
		_uart.obuf[1] = 0x55;		
		_uart.obuf[2] = 0x55;		
		_uart.obuf[3] = 0x00;		
	}
#endif

	/* See if we have anything to write */
	if (_uart.obuflen != 0 && (s1 & UART_PDD_TX_DATA_EMPTY_FLAG)) {

		/* Write the char now and advance */
		UART_PDD_WriteDataReg(BASE_PTR, _uart.obuf[_uart.obufpos++]);

		/* Check for finishing */
		if (_uart.obufpos >= _uart.obuflen) {
			_uart.obufpos = 0;
			_uart.obuflen = 0;
		}
	}

	/* Check for timeout, give it 5ms */
	if (_systime.ticks - _uart.state_ts > UART_TIMEOUT ) {

		// Restamp now
		_uart.state_ts = _systime.ticks;

		// Log timeouts and reset state but only
		// when not already in state 1
		if (_uart.state != UART_STATE_SEEK1) {
			_uart.state = UART_STATE_SEEK1;
			_uart.timocount++;
		}

	}
}


int uart_can_write()
{
	return _uart.obuflen == 0;
}

uint8_t *uart_get_outbuf()
{
	return _uart.obuf;
}

void uart_start_write(int len)
{
	_uart.obufpos = 0;
	_uart.obuflen = len;
}

