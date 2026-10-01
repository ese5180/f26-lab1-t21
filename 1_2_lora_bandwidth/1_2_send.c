/*
 * Copyright (c) 2019 Manivannan Sadhasivam
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/drivers/gpio.h>
#include <errno.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DEFAULT_RADIO_NODE),
	     "No default LoRa radio specified in DT");

#define MAX_DATA_LEN 12

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lora_send);

/* Explicit node label bindings for Nucleo-WL55JC discrete LEDs */
static const struct gpio_dt_spec led_blue  = GPIO_DT_SPEC_GET(DT_NODELABEL(blue_led_1), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_NODELABEL(green_led_2), gpios);
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(DT_NODELABEL(red_led_3), gpios);

char data[MAX_DATA_LEN] = {'e', 's', 'e', '5', '1', '8', '0', 't', '2', '1', '-', '0'};

int main(void)
{
	const struct device *const lora_dev = DEVICE_DT_GET(DEFAULT_RADIO_NODE);
	struct lora_modem_config config = {0};
	uint32_t airtime_ms;
	uint32_t sleep_ms;
	int ret;

	if (!device_is_ready(lora_dev)) {
		LOG_ERR("%s Device not ready", lora_dev->name);
		return 0;
	}

	/* Configure discrete LEDs */
	if (gpio_is_ready_dt(&led_blue))  gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
	if (gpio_is_ready_dt(&led_green)) gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	if (gpio_is_ready_dt(&led_red))   gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);

	/* --- FCC Part 15.231 Micro-Power Settings --- */
	config.frequency = 433920000;      /* 433.92 MHz */
	config.bandwidth = BW_500_KHZ;     /* 500 kHz, widest the SX126x supports */
	config.datarate = SF_5;            /* SF5, fastest */
	config.preamble_len = 12;          /* SX126x minimum at SF5/SF6 */
	config.coding_rate = CR_4_5;
	config.packet_crc_disable = false; /* CRC costs ~0.3 ms here */
	config.iq_inverted = false;
	config.public_network = false;
	config.tx_power = -10;             /* Minimum output (-10 dBm) */
	config.tx = true;

	ret = lora_config(lora_dev, &config);
	if (ret < 0) {
		LOG_ERR("LoRa config failed");
		return 0;
	}

	airtime_ms = lora_airtime(lora_dev, MAX_DATA_LEN);
	LOG_INF("Packet airtime: %u ms", airtime_ms);

	if (airtime_ms >= 1000) {
		LOG_ERR("Airtime (%u ms) exceeds 1s FCC Part 15 limit!", airtime_ms);
		return 0;
	}

	sleep_ms = MAX(airtime_ms * 30, 10000);
	LOG_INF("FCC Part 15 compliant quiet period: %u ms", sleep_ms);

	while (1) {
		/* 1. RF TX ACTIVE: Turn ON Red LED while transmitting */
		if (gpio_is_ready_dt(&led_red)) {
			gpio_pin_set_dt(&led_red, 1);
		}

		ret = lora_send(lora_dev, data, MAX_DATA_LEN);

		/* Turn OFF Red LED immediately after transmission finishes */
		if (gpio_is_ready_dt(&led_red)) {
			gpio_pin_set_dt(&led_red, 0);
		}

		if (ret < 0) {
			LOG_ERR("LoRa send failed");
			return 0;
		}

		LOG_INF("Data sent %c! Entering required %u ms quiet period...",
			data[MAX_DATA_LEN - 1], sleep_ms);

		/* 2. TX COMPLETE STROBE: Rapid double-flash on Blue + Green LEDs */
		for (int i = 0; i < 2; i++) {
			if (gpio_is_ready_dt(&led_blue))  gpio_pin_set_dt(&led_blue, 1);
			if (gpio_is_ready_dt(&led_green)) gpio_pin_set_dt(&led_green, 1);
			k_msleep(75);
			if (gpio_is_ready_dt(&led_blue))  gpio_pin_set_dt(&led_blue, 0);
			if (gpio_is_ready_dt(&led_green)) gpio_pin_set_dt(&led_green, 0);
			k_msleep(75);
		}

		/* 3. FCC QUIET PERIOD: All LEDs remain OFF for the 10-second sleep */
		k_sleep(K_MSEC(sleep_ms - 300)); /* Subtract strobe delay */

		/* Increment final character */
		if (data[MAX_DATA_LEN - 1] == '9') {
			data[MAX_DATA_LEN - 1] = '0';
		} else {
			data[MAX_DATA_LEN - 1] += 1;
		}
	}

	return 0;
}
