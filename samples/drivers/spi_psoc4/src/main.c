/*
 * SPDX-FileCopyrightText: <text>Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG. All rights reserved.</text>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>

#define SPI_NODE DT_NODELABEL(spi1)

/* 3-byte framed protocol: SOP, command, EOP. The slave validates the frame
 * markers before acting on the command byte.
 */
#define PACKET_SOP_POS 0
#define PACKET_CMD_POS 1
#define PACKET_EOP_POS 2
#define PACKET_SOP     0x01U
#define PACKET_EOP     0x17U

#define LED_STATE_ON  0x01U
#define LED_STATE_OFF 0x00U

static const struct device *const spi_dev = DEVICE_DT_GET(SPI_NODE);

/* CS is driven by the SCB's hardware slave-select (SS0), so it is asserted
 * automatically for exactly the transfer duration -- no GPIO cs entry.
 */
static const struct spi_config spi_cfg = {
	.frequency = 1000000,
	.operation = SPI_OP_MODE_CONTROLLER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
	.slave = 0,
};

int main(void)
{
	if (!device_is_ready(spi_dev)) {
		return 0;
	}

	uint8_t led_cmd = LED_STATE_OFF;

	while (1) {
		led_cmd = (led_cmd == LED_STATE_OFF) ? LED_STATE_ON : LED_STATE_OFF;

		uint8_t tx_data[3];

		tx_data[PACKET_SOP_POS] = PACKET_SOP;
		tx_data[PACKET_CMD_POS] = led_cmd;
		tx_data[PACKET_EOP_POS] = PACKET_EOP;

		const struct spi_buf tx_buf = {.buf = tx_data, .len = sizeof(tx_data)};
		const struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};

		spi_write(spi_dev, &spi_cfg, &tx_set);

		k_sleep(K_MSEC(1000));
	}

	return 0;
}
