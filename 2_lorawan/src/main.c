/*
 * Class A LoRaWAN sample application
 *
 * Copyright (c) 2020 Manivannan Sadhasivam <mani@kernel.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/lorawan/lorawan.h>
#include <zephyr/kernel.h>

#include "secrets.h"

#define DELAY K_MSEC(10000)

/* Request a link check on every n-th uplink */
#define LINK_CHECK_PERIOD 5

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lorawan_class_a);

static uint8_t data[] = "{\"name\":\"Team 21\",\"team\":\"21\",\"board\":\"WL55JC\"}";

static void dl_callback(uint8_t port, uint8_t flags, int16_t rssi, int8_t snr, uint8_t len,
			const uint8_t *hex_data)
{
	LOG_INF("Port %d, RSSI %ddB, SNR %ddBm", port, rssi, snr);

	if (flags & LORAWAN_TIME_UPDATED) {
		/* The stack has synced the network time from a DeviceTimeAns. */
		LOG_INF("Network time updated");
	}

	if (flags & LORAWAN_DATA_PENDING) {
		/* The network server has more downlinks queued.  An application
		 * that wants to drain them promptly can schedule another uplink
		 * (e.g. an empty one on any port) here.
		 */
		LOG_INF("Network reports more data pending (FPending set)");
	}

	if (hex_data) {
		LOG_HEXDUMP_INF(hex_data, len, "Payload: ");
	}
}

static void lorwan_datarate_changed(enum lorawan_datarate dr)
{
	uint8_t unused, max_size;

	lorawan_get_payload_sizes(&unused, &max_size);
	LOG_INF("New Datarate: DR_%d, Max Payload %d", dr, max_size);
}

static void link_check_ans_cb(uint8_t demod_margin, uint8_t nb_gateways)
{
	LOG_INF("Link check: margin %u dB, %u gateway(s)",
		demod_margin, nb_gateways);
}

int main(void)
{
	const struct device *lora_dev;
	struct lorawan_join_config join_cfg;
	uint8_t dev_eui[] = LORAWAN_DEV_EUI;
	uint8_t app_skey[] = LORAWAN_APP_SKEY;
	uint8_t nwk_skey[] = LORAWAN_NWK_SKEY;
	int ret;

	struct lorawan_downlink_cb downlink_cb = {
		.port = LW_RECV_PORT_ANY,
		.cb = dl_callback
	};

	lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
	if (!device_is_ready(lora_dev)) {
		LOG_ERR("%s: device not ready.", lora_dev->name);
		return 0;
	}

#if defined(CONFIG_LORAWAN_REGION_EU868)
	/* If more than one region Kconfig is selected, app should set region
	 * before calling lorawan_start()
	 */
	ret = lorawan_set_region(LORAWAN_REGION_EU868);
	if (ret < 0) {
		LOG_ERR("lorawan_set_region failed: %d", ret);
		return 0;
	}
#endif

	ret = lorawan_start();
	if (ret < 0) {
		LOG_ERR("lorawan_start failed: %d", ret);
		return 0;
	}

	/* Restrict to US915 FSB2 (channels 8-15 + channel 65),
	 * matching the gateway's frequency plan */
	uint16_t channel_mask[6] = { 0xFF00, 0x0000, 0x0000, 0x0000, 0x0002, 0x0000 };

	ret = lorawan_set_channels_mask(channel_mask, 6);
	if (ret < 0) {
		LOG_ERR("lorawan_set_channels_mask failed: %d", ret);
		return 0;
	}

	/* DR0 (the default) caps payloads at 11 bytes on US915.
	 * DR3 (SF7BW125) allows up to 242 bytes. */
	ret = lorawan_set_datarate(LORAWAN_DR_3);
	if (ret < 0) {
		LOG_ERR("lorawan_set_datarate failed: %d", ret);
		return 0;
	}

	lorawan_register_downlink_callback(&downlink_cb);
	lorawan_register_dr_changed_callback(lorwan_datarate_changed);
	lorawan_register_link_check_ans_callback(link_check_ans_cb);

	join_cfg.mode = LORAWAN_ACT_ABP;
	join_cfg.dev_eui = dev_eui;
	join_cfg.abp.dev_addr = LORAWAN_DEV_ADDR;
	join_cfg.abp.app_skey = app_skey;
	join_cfg.abp.nwk_skey = nwk_skey;
	join_cfg.abp.app_eui = NULL; /* AppEUI is n/a for ABP */

	LOG_INF("Joining network over ABP");
	ret = lorawan_join(&join_cfg);
	if (ret < 0) {
		LOG_ERR("lorawan_join_network failed: %d", ret);
		return 0;
	}

	LOG_INF("Sending data...");
	for (uint32_t i = 0;; i++) {
		if ((i % LINK_CHECK_PERIOD) == 0) {
			/*
			 * The LinkCheckReq MAC command rides the next uplink;
			 * the answer is delivered via the registered callback.
			 */
			ret = lorawan_request_link_check(false);
			if (ret < 0) {
				LOG_ERR("lorawan_request_link_check failed: %d",
					ret);
			}
		}

		/* sizeof(data) - 1 drops the trailing '\0' */
		ret = lorawan_send(2, data, sizeof(data) - 1,
				   LORAWAN_MSG_UNCONFIRMED);

		/*
		 * Note: The stack may return -EAGAIN if the provided data
		 * length exceeds the maximum possible one for the region and
		 * datarate. But since we are just sending the same data here,
		 * we'll just continue.
		 */
		if (ret == -EAGAIN) {
			LOG_ERR("lorawan_send failed: %d. Continuing...", ret);
			k_sleep(DELAY);
			continue;
		}

		if (ret < 0) {
			LOG_ERR("lorawan_send failed: %d", ret);
			return 0;
		}

		LOG_INF("Data sent!");
		k_sleep(DELAY);
	}
}
