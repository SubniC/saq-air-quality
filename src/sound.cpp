#include "sound.h"
#include "libs/json.h"
#include "utils/debug.h"
#include "utils/placement_new.h"

volatile size_t sampleIndex = 0;
volatile size_t sendIndex = 0;
volatile uint32_t g_audio_dropped_blocks = 0;
static InPlace<SoundMeter> s_sound_meter_storage;
SoundMeter *sound_meter = nullptr;
IntervalTimer soundSampligTimer;
SoundSampleBuffer_t audio_sample_buffer[AUDIO_SAMPLE_BUFFERS];

void SOUND::begin()
{
	/*
	Configuramos el ADC
	https://docs.particle.io/reference/firmware/photon/#setadcsampletime-
	ADC_SampleTime_3Cycles: Sample time equal to 3 cycles, 100ns
	ADC_SampleTime_15Cycles: Sample time equal to 15 cycles, 500ns
	ADC_SampleTime_28Cycles: Sample time equal to 28 cycles, 933ns
	ADC_SampleTime_56Cycles: Sample time equal to 56 cycles, 1.87us
	ADC_SampleTime_84Cycles: Sample time equal to 84 cycles, 2.80us
	ADC_SampleTime_112Cycles: Sample time equal to 112 cycles, 3.73us
	ADC_SampleTime_144Cycles: Sample time equal to 144 cycles, 4.80us
	ADC_SampleTime_480Cycles: Sample time equal to 480 cycles, 16.0us (default)
	The default is ADC_SampleTime_480Cycles. This means that the ADC is sampled for 16 us which can provide a more accurate reading, at the expense of taking longer than using a shorter ADC sample time. If you are measuring a high frequency signal, such as audio, you will almost certainly want to reduce the ADC sample time.

	Furthermore, 5 consecutive samples at the sample time are averaged in analogRead(), so the time to convert is closer to 80 us, not 16 us, at 480 cycles.
	*/
	// Teoricamente sampleando a 16khz tardamos 62.5uS entre medida y medida, con lo que dice la documentacion ahora me parece muy poco 3 ciclos.
	// usando 84 ciclos del ADC consumimos 2.8us * 5 = 14uS
	// usando 112 ciclos del ADC consumimos 3.73us * 5 = 18.65uS
	// usando 144 ciclos del ADC consumimos 4.8us * 5 = 24uS
	setADCSampleTime(ADC_SampleTime_112Cycles);
	// Medidas dummy para configurar los canales de ADC
	analogRead(MIC_PIN);
#ifdef SYSTEM_COMPENSATE_ADC
	analogRead(VREF_PIN);
#endif

	// Iniciamos el objeto SoundMeter (en almacenamiento estático, sin heap)
	sound_meter = s_sound_meter_storage.construct(SAMPLE_RATE, SAMPLE_BUF_SIZE);
	sound_meter->addNoiseLevelCallback(on_sound_level_callback);
#ifdef AUDIO_ENABLE_DETECT_CLAPS
	sound_meter->enableClapDetector();
	sound_meter->addClapCallback(on_clap_callback); // Notificacion del detector de palmas
#endif

#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
	sound_meter->enableWhistleDetector();
	sound_meter->addWhistleCallback(on_whistle_callback);
#endif
}

void SOUND::loop()
{
	// Esta funcion debe ejecutarse mas rapido que el llenado de 1 buffer (32 ms a 16kHz/512).
	// Con AUDIO_SAMPLE_NUM_BUFFERS=4 tenemos tolerancia de ~96 ms antes de perder datos.

	// Diagnostico: loguear drops acumulados por el ISR (cada 10s maximo)
	#if defined(ENABLE_SERIAL_DEBUG) || defined(ENABLE_SERIAL_MQTT_DEBUG)
	if (g_audio_dropped_blocks > 0) {
		__disable_irq();
		uint32_t dropped = g_audio_dropped_blocks;
		g_audio_dropped_blocks = 0;
		__enable_irq();
		LOG_WARN("{SOUND} ISR dropped %lu sample blocks (main loop too slow)", (unsigned long)dropped);
		(void)dropped; // evita warning si logging está deshabilitado
	}
	#endif

	if (sendIndex < sampleIndex)
	{
		// Obtenemos el buffer que toca enviar :)
		SoundSampleBuffer_t *sb = &audio_sample_buffer[sendIndex % AUDIO_SAMPLE_BUFFERS];

#ifdef SYSTEM_COMPENSATE_ADC
		for (size_t jj = 0; jj < SAMPLE_BUF_SIZE; jj++)
		{
			sb->data[jj] = (uint16_t)((sb->data[jj] * sb->data_ref[jj]) / VERF_ADC_TICKS_33);
		}
#endif
		// Llamamos a la libreria y le pasamos el buffer para que procese los datos y calcule los dB del periodo
		sound_meter->parseBucket(sb->data, SAMPLE_BUF_SIZE);
		// Ahora marcamos el buffer como vacio, para que el timer con la interrupcion pueda seguir procesando datos
		
		sendIndex++; // Primero incrementamos
		sb->free = true; // Luego liberamos la isr
		// Procesamos el bucket
		sound_meter->processBucket();
	}
}

void SOUND::start_sampling()
{
	// Inicializamos los buffers
	for (size_t i = 0; i < AUDIO_SAMPLE_BUFFERS; i++)
	{
		audio_sample_buffer[i].free = true;
		audio_sample_buffer[i].index = 0;
	}
	sampleIndex = 0;
	sendIndex = 0;

	soundSampligTimer.begin(on_sound_sampling_isr, 1000000 / SAMPLE_RATE, uSec, TIMER4);
}

void SOUND::stop_sampling()
{
	soundSampligTimer.end();
}

// Callback adaptativo: se llama cuando el nivel cambia >= NOISE_PUBLISH_DELTA_DB
// o cada NOISE_MAX_INTERVAL_MS como heartbeat.
void SOUND::on_sound_level_callback(float dbfs_fast, float dbfs_slow)
{
	COMMS::mqtt_send_noise(dbfs_fast, dbfs_slow);
}

#ifdef AUDIO_ENABLE_DETECT_CLAPS
// count -> numero de palmas detectadas
void SOUND::on_clap_callback(uint8_t count, unsigned long period_ms, float peak_dbfs, float ambient_dbfs, float snr_db)
{
	COMMS::mqtt_send_claps(count, period_ms, peak_dbfs, ambient_dbfs, snr_db);

#ifdef ENABLE_NEOPIXEL
#ifdef AUDIO_CLAP_WHISLE_VISUAL_DEBUG
	// Parpadeamos tantas veces como palmadas y tan rapido como las mismas
	system_leds->blink(NEOPIXEL_LED_COUNT - 1, HTMLColorCode::Blue, HTMLColorCode::Black, 300, count);
#endif
#endif
}
#endif

#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
// count -> numero de silvidos detectados
void SOUND::on_whistle_callback(unsigned long dur_ms, uint16_t freq_hz, float tonality, float db)
{
	COMMS::mqtt_send_whistle(dur_ms, freq_hz, tonality, db);

#ifdef ENABLE_NEOPIXEL
#ifdef AUDIO_CLAP_WHISLE_VISUAL_DEBUG
	// Parpadeamos tantas veces como palmadas y tan rapido como las mismas
	system_leds->blink(NEOPIXEL_LED_COUNT - 2, HTMLColorCode::Green, HTMLColorCode::Black, dur_ms, 2);
#endif
#endif
}
#endif

void SOUND::on_sound_sampling_isr()
{
	SoundSampleBuffer_t *sb = &audio_sample_buffer[sampleIndex % AUDIO_SAMPLE_BUFFERS];
	if (!sb->free)
	{
		// Buffer lleno: el main loop no ha procesado a tiempo. Contabilizamos la perdida
		// para diagnostico (se loguea en SOUND::loop).
		g_audio_dropped_blocks++;
		return;
	}
	// Esto lo hacia en el codigo original
	//  uint16_t read = (uint8_t) (analogRead(MIC_PIN) >> 4);
	//  uint16_t read_ref = (uint8_t) (analogRead(A3) >> 4);
	sb->data[sb->index] = analogRead(MIC_PIN);
#ifdef SYSTEM_COMPENSATE_ADC
	sb->data_ref[sb->index] = analogRead(VREF_PIN);
#endif
	sb->index++;
	// Comprobamos si el buffer esta lleno
	if (sb->index >= SAMPLE_BUF_SIZE)
	{
		// marcamos como lleno
		sb->free = false;
		sb->index = 0;
		// Al incrementar sampleIndex rotamos al siguiente buffer circular.
		// A 16kHz con 512 muestras cada buffer se llena en ~32 ms.
		// Con N buffers, toleramos (N-1)*32 ms sin procesar antes de perder datos.
		sampleIndex++;
	}
}