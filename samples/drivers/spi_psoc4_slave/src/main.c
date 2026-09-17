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
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define SPI_NODE DT_NODELABEL(spi1)

/* 3-byte framed protocol: SOP, command, EOP. Only act on the command byte
 * when both frame markers are intact.
 */
#define PACKET_SOP_POS 0
#define PACKET_CMD_POS 1
#define PACKET_EOP_POS 2
#define PACKET_SOP     0x01U
#define PACKET_EOP     0x17U

#define LED_STATE_ON 0x01U

static const struct device *const spi_dev = DEVICE_DT_GET(SPI_NODE);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
/* LED1 toggles on every completed SPI transfer, valid or not -- this tells
 * a stuck/dark LED0 (bad SOP/EOP) apart from a truly hung spi_read().
 */
static const struct gpio_dt_spec activity_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct spi_config spi_cfg = {
	.frequency = 1000000,
	.operation = SPI_OP_MODE_PERIPHERAL | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
	.slave = 0,
};

int main(void)
{
	if (!device_is_ready(spi_dev) || !gpio_is_ready_dt(&led) ||
	    !gpio_is_ready_dt(&activity_led)) {
		return 0;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&activity_led, GPIO_OUTPUT_INACTIVE);

	uint8_t rx_buf[3];
	const struct spi_buf rx_spi_buf = {.buf = rx_buf, .len = sizeof(rx_buf)};
	const struct spi_buf_set rx_set = {.buffers = &rx_spi_buf, .count = 1};
	/* Only print repeated identical failures periodically, not every loop. */
	uint32_t fail_streak = 0;

	while (1) {
		int ret = spi_read(spi_dev, &spi_cfg, &rx_set);
		/* In slave mode a successful spi_read() returns the number of frames
		 * received, not 0; a full packet is sizeof(rx_buf) frames.
		 */
		bool ok = (ret == (int)sizeof(rx_buf));

		gpio_pin_toggle_dt(&activity_led);
		if (ok) {
			fail_streak = 0;
			printk("rx: %02x %02x %02x\n", rx_buf[0], rx_buf[1], rx_buf[2]);
		} else {
			fail_streak++;
			if (fail_streak == 1 || (fail_streak % 100) == 0) {
				printk("spi_read failed: %d (x%u so far)\n", ret, fail_streak);
			}
		}
		if (ok && rx_buf[PACKET_SOP_POS] == PACKET_SOP &&
		    rx_buf[PACKET_EOP_POS] == PACKET_EOP) {
			gpio_pin_set_dt(&led, rx_buf[PACKET_CMD_POS] == LED_STATE_ON);
		}
	}

	return 0;
}
