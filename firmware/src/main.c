#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "modules/button_handler.h"
#include "modules/led_controller.h"
#include "modules/sd_card.h"
#include "audio/wav_file.h"
#include "audio/audio_stream.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/audio/dmic.h>
#include "macros.h"
#include "event_handler.h"
#include "ble/ble_manager.h"

LOG_MODULE_REGISTER(main);

int main(void)
{
    int ret;
	LOG_INF("Turning on");

	ret = button_handler_init();
	if(ret!=0) LOG_ERR("Buttons failed to init");
    ret = led_controller_init();
	if(ret!=0) LOG_ERR("LEDs failed to init");

    LOG_INF("Configured Buttons & Interrupts");

	static struct fs_file_t wav_file;

	WavConfig wav_config = {
			.wav_file = &wav_file,
			.file_name = "testfile.wav",
			.length = WAV_LENGTH_BLOCKS * MAX_BLOCK_SIZE,
			.sample_rate = MAX_SAMPLE_RATE,
			.bytes_per_sample = BYTES_PER_SAMPLE,
			.num_channels = NUM_CHANNELS,
	};

	AudioStream audio_stream = {
		.wav_config = wav_config,
		.dmic_ctx = DEVICE_DT_GET(DT_NODELABEL(pdm0)),
		.pdm_gain = NRF_PDM_GAIN_MAXIMUM
	};

    LOG_INF("BLOCK SIZE: %d\n", MAX_BLOCK_SIZE);

	ret = pdm_init(&audio_stream);
	if (ret != 0) LOG_ERR("pdm config failed");
	
    ret = sd_card_init();
	if(ret!=0) LOG_ERR("SD Failed to init");

	ret = ble_init();
	if(ret!=0) LOG_ERR("BLE Failed to init");

    event_handler_set_audio_stream(&audio_stream);

    //Start up the application
    AppEvent ev = { .type = EVENT_START_UP};
    event_handler_post(ev);
	
    LOG_INF("Entering main event loop…");
    event_handler_run();
}

