/*
 * OhmniLabs new common UART/api
 */

#ifndef __UART_BUS_H__
#define __UART_BUS_H__


/* ---- UART definitions ---- */

enum uart_state
{
	UART_STATE_SEEK1,
	UART_STATE_SEEK2,
	UART_STATE_GET_LENGTH,
	UART_STATE_READN,
};

struct uart_data
{
	/* Parsing state */
	enum uart_state state;

	/* State timeout check */
	uint32_t state_ts;

	/* Last status */
	int last_status;

	/* Inbound stuff */  
	int cbufpos;
	int cbufreq;

	/* Inbound checksum in progress */
	uint8_t currcsum;

	/* Stats */
	int readcount;		// Total number of bytes read	
	int timocount;		// Number of timeouts triggered
	int badsumcount;	// Number of bad checksums	

	/* Outbound stuff */  
	int obuflen;
	int obufpos;

	/* Buffers */
	uint8_t cbuf[64];
	uint8_t obuf[32];

	/* Test param */
	uint8_t ledval;
};

extern struct uart_data _uart;


/* ---- Methods ---- */

void uart_setup();
void uart_poll();
void uart_handle_byte(uint8_t byte);
void uart_handle_read_complete();

void uart_handle_cmd_eep_write();
void uart_handle_cmd_eep_read();
void uart_handle_cmd_ram_write();
void uart_handle_cmd_ram_read();
void uart_handle_cmd_reboot();

int uart_can_write();
uint8_t *uart_get_outbuf();
void uart_start_write(int len);


#endif
