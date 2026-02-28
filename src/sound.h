#ifndef _MODULESOUND_H_
#define _MODULESOUND_H_

#include "application.h"
#include "config/config.h"
#include "SparkIntervalTimer.h"
#include "libs/SoundMeter.h"
#ifdef ENABLE_NEOPIXEL
#include "libs/LedController.h"
#endif
#include "comms.h"

extern bool mqtt_publish(const char*, const char*);

const size_t SAMPLE_BUF_SIZE = AUDIO_SAMPLE_BUFFER_SIZE;
const size_t AUDIO_SAMPLE_BUFFERS = AUDIO_SAMPLE_NUM_BUFFERS;
const long SAMPLE_RATE = AUDIO_SAMPLE_RATE_HZ;

typedef struct
{
	volatile bool free;
	volatile size_t index;
	uint16_t data[SAMPLE_BUF_SIZE];
	#ifdef SYSTEM_COMPENSATE_ADC
		uint16_t data_ref[SAMPLE_BUF_SIZE];
	#endif
} SoundSampleBuffer_t;

#ifdef ENABLE_NEOPIXEL
extern LedController *system_leds;
#endif

extern SoundMeter *sound_meter;
extern IntervalTimer soundSampligTimer;
extern SoundSampleBuffer_t audio_sample_buffer[AUDIO_SAMPLE_BUFFERS];
extern volatile size_t sampleIndex;
extern volatile size_t sendIndex;
// Contador de bloques perdidos por el ISR cuando main loop no procesa a tiempo.
// Se incrementa en el ISR y se lee/resetea en SOUND::loop().
extern volatile uint32_t g_audio_dropped_blocks;
/*
NOTA: La idea inicial de esta infraestrucutra partio del codigo encontrado en el sigueinte enlace,
algunos nombres de variables y comentarios siguen siendo heredados, pero el codigo poco tiene que ver ya
con el original https://github.com/rickkas7/photonAudio
*/

namespace SOUND{

	void begin();
	void loop();
	void start_sampling();
	void stop_sampling();
	void on_sound_sampling_isr();
	void on_sound_level_callback(float dbfs_fast, float dbfs_slow);
	#ifdef AUDIO_ENABLE_DETECT_CLAPS
	void on_clap_callback(uint8_t, unsigned long, float, float, float);
	#endif
	#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
	void on_whistle_callback(unsigned long dur_ms, uint16_t freq_hz, float tonality, float db);
	#endif
}

#endif
