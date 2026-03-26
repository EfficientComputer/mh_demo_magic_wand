#include <stdint.h>
#include <stdio.h>

#include <eff.h>
#include <eff/drivers/pinmux.h>
#include <eff/drivers/uart.h>
#include "model_dh.h"

#define SENSOR_BYTES 384
#define SYNC_BYTE 0x80

#define HOST_UART UART_5
#define HOST_PINMUX PINMUX_5

int fabric_run_model(int8_t input[1][128][3][1], int8_t output[1][4])
{
	return run_model(input, output);
}

static int predict_gesture(const int8_t *prediction_scores)
{
	int max_prediction_index = -1;
	int8_t max_prediction_score = -128;

	for (int i = 0; i < 4; i++) {
		if ((max_prediction_index == -1) ||
			(prediction_scores[i] > max_prediction_score)) {
			max_prediction_score = prediction_scores[i];
			max_prediction_index = i;
		}
	}

	return max_prediction_index;
}

static void reshape_to_model_input(const int8_t *flat,
					 int8_t input[1][128][3][1])
{
	for (int i = 0; i < 128; i++) {
		for (int j = 0; j < 3; j++) {
			input[0][i][j][0] = flat[i * 3 + j];
		}
	}
}

int main(void)
{
	printf("=====================================\n");
	printf("Ready for payload, waiting for sync (0x80)\n");
	printf("=====================================\n");

	eff_uart_cfg_t cfg = EFF_UART_DEFAULTS;
	cfg.baud = 9600;

	eff_pinmux_set(HOST_PINMUX, PINMUX_UART);
	eff_uart_init(HOST_UART, cfg);

	static int8_t sensor_bytes[SENSOR_BYTES];
	static int8_t model_input[1][128][3][1];
	static int8_t model_output[1][4];

	/* Clear any stale bytes before starting protocol */
	eff_uart_rx_flush(HOST_UART);
	eff_uart_tx_flush(HOST_UART);

	while (1) {
		char c = 0;

		// Wait for sync byte 0x80
		for (;;) {
			while (eff_uart_rx_empty(HOST_UART)) {
			}

			eff_uart_getc(HOST_UART, &c);
			if ((uint8_t)c == SYNC_BYTE) {
				break;
			}
		}

		// Receive 384 bytes of sensor data
		for (int i = 0; i < SENSOR_BYTES; i++) {
			while (eff_uart_rx_empty(HOST_UART)) {
				// busy wait
			}
			char b = 0;
			eff_uart_getc(HOST_UART, &b);
			sensor_bytes[i] = (int8_t)b;
		}

		// Reshape to [1][128][3][1]
		reshape_to_model_input(sensor_bytes, model_input);

		// Run inference
		fabric_run_model(model_input, model_output);

		// Find Argmax gesture
		int gesture_id = predict_gesture(model_output[0]);

		// Print raw scores and chosen gesture on STDIO
		printf("EVK_SCORES,%d,%d,%d,%d\n",
			   (int)model_output[0][0],
			   (int)model_output[0][1],
			   (int)model_output[0][2],
			   (int)model_output[0][3]);
		printf("Chosen gesture_id=%d\n", gesture_id);

		// Flush TX FIFO and send response: [gesture_id][conf0][conf1][conf2][conf3]
		eff_uart_tx_flush(HOST_UART);

		// First byte: predicted gesture id
		eff_uart_putc(HOST_UART, (char)gesture_id);

		// Next 4 bytes: per-class confidences 0–3
		for (int i = 0; i < 4; i++) {
			int8_t score = model_output[0][i];
			uint8_t conf = (uint8_t)(((score + 128) * 100) / 255);
			eff_uart_putc(HOST_UART, (char)conf);
		}
	}

	return 0;
}
