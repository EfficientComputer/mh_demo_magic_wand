#include <stdint.h>
#include <stdio.h>

#include <eff.h>
#include <eff/drivers/pinmux.h>
#include <eff/drivers/uart.h>

// MKR Header
// Bank 5, Pin 4(TX) Pin 5(RX)
#define HOST_UART UART_5
#define HOST_PINMUX PINMUX_5

// UNO Header
// Bank 2, Pin 4(TX) Pin 5(RX)
/* #define HOST_UART UART_2 */
/* #define HOST_PINMUX PINMUX_2 */

int main(void)
{
	printf("=====================================\n");
	printf("Printing any byte seen on HOST_UART as it arrives\n");
	printf("=====================================\n");

	eff_pinmux_set(HOST_PINMUX, PINMUX_UART);
	eff_uart_cfg_t cfg = EFF_UART_DEFAULTS;
	cfg.baud = 9600;

	if (eff_uart_init(HOST_UART, cfg)) {
		printf("Initialization failed!\n");
		return -1;
	}
	printf("HOST_UART Initialized\n");

	eff_uart_rx_flush(HOST_UART);
	eff_uart_tx_flush(HOST_UART);

	while (1) {
		char b = 0;

		// Block until at least one byte is available on HOST_UART
		while (eff_uart_rx_empty(HOST_UART)) {
			// busy-wait
		}

		eff_uart_getc(HOST_UART, &b);

		// Dump raw value as hex and signed decimal on STDIO
		printf("RX: 0x%02X (%d)\n", (unsigned char)b, (int8_t)b);
		eff_uart_rx_flush(STDIO_UART);
	}

	return 0;
}
