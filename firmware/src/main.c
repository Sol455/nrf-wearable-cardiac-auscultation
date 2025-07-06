#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "modules/button_handler.h"
#include "modules/led_controller.h"
#include "modules/sd_card.h"
#include "audio/wav_file.h"
#include "audio/audio_in.h"
#include "audio/audio_stream.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/audio/dmic.h>
#include "macros.h"
#include "event_handler.h"
#include "ble/ble_manager.h"
#include "audio/dsp/rt_peak_detector.h"
#include "audio/dsp/circular_block_buffer.h"
#include "audio/dsp/peak_processor.h"
#include "audio/dsp/window_analysis.h"

LOG_MODULE_REGISTER(main);

int main(void)
{
    int ret;
	LOG_INF("Turning on");

	static struct fs_file_t output_wav_file;

	WavConfig output_wav_config = {
			.wav_file = &output_wav_file,
			.file_name = "test.wav",
			.length = WAV_LENGTH_BLOCKS * MAX_BLOCK_SIZE,
			.sample_rate = MAX_SAMPLE_RATE,
			.bytes_per_sample = BYTES_PER_SAMPLE,
			.num_channels = NUM_CHANNELS,
	};

	static struct fs_file_t input_wav_file;

	WavConfig input_wav_config = {
			.wav_file = &input_wav_file,
			.file_name = "tri0.wav",
	};

	AudioInConfig audio_in_config = {
		.audio_input_type = AUDIO_INPUT_TYPE_WAV,
		.input_wav_config = input_wav_config,
		.output_wav_config = output_wav_config,
		.dmic_ctx = DEVICE_DT_GET(DT_NODELABEL(pdm0)),
		.pdm_gain = NRF_PDM_GAIN_MAXIMUM,
		.msgq = audio_stream_get_msgq(),
	};

	RTPeakConfig rt_peak_config = {
		.block_size = BLOCK_SIZE_SAMPLES,   
		.num_blocks = CB_NUM_BLOCKS,   
		.alpha = 0.0001,
		.threshold_scale = 2.0,
		.min_distance_samples = 2000,        
	};

	RTPeakValConfig rt_peak_val_config = {
		.close_r = 0.45,
		.far_r = 0.55,
		.margin = 0.05,
		.peak_msgq = audio_stream_get_peak_msgq()
	};

	PeakProcessorConfig peak_processor_config = {
		.pre_ratio = 0.25,
		.pre_max_samples = 2000,
		.pre_min_samples = 200,
	};

	WindowAnalysisConfig window_analysis_config = {
		.audio_hl_thresh = 1.0f / 3.0f,
		.ste_block_size_samples = STE_SAMPLES_PER_BLOCK,
		.ste_hl_thresh = 0.4,
		.peak_thresh_scale = 0.7,
		.peak_min_distance = 1,
		.de_cluster_window_r = 0.2,
		.ident_s1_reject_r = 0.3,
		.ident_s1_s2_gap_r = 0.29,
		.ident_s1_s2_gap_tol = 0.15,
	};

	AudioStreamConfig audio_stream_config = {
		.rt_peak_config = rt_peak_config,
		.rt_peak_val_config = rt_peak_val_config,
		.peak_processor_config = peak_processor_config,
		.window_analysis_config = window_analysis_config,
	};

    LOG_INF("BLOCK SIZE: %d\n", MAX_BLOCK_SIZE);

	ret = button_handler_init();
	if(ret!=0) LOG_ERR("Buttons failed to init");
    ret = led_controller_init();
	if(ret!=0) LOG_ERR("LEDs failed to init");

    LOG_INF("Configured Buttons & Interrupts");

	ret = audio_in_init(audio_in_config);
	if(ret!=0) LOG_ERR("Audio In Config Failed");
	
    ret = sd_card_init();
	if(ret!=0) LOG_ERR("SD Failed to init");

	ret = ble_init();
	if(ret!=0) LOG_ERR("BLE Failed to init");

	init_audio_stream(audio_stream_config);

    //Start up the application
    AppEvent ev = { .type = EVENT_START_UP};
    event_handler_post(ev);
	
    LOG_INF("Entering main event loop…");
    event_handler_run();
}

