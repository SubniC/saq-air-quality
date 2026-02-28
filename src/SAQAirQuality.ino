// SAQAirQuality.ino — Scheduler basado en periodos

// =============================== Modo del sistema =============================
SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

// ============================ Includes del proyecto ============================
#include "config/config.h"
#include "sys.h"
#include "sound.h"
#include "sensors.h"
#include "persistence.h"
#include "comms.h"
#include "comms_router.h"
#include "comms_json.h"
#include "utils/debug.h"
#include "utils/profile.h"
#include "utils/time_helpers.h"

#ifdef ENABLE_LCD
#include "screen.h"
#endif

#ifdef WDT_ENABLE
#include "libs/photon-wdgs.h"
#endif

#ifdef ENABLE_BUZZER
#include "buzzer.h"
#endif

#ifdef ENABLE_NEOPIXEL
#include "libs/LedController.h"
LedController *system_leds;
#endif

// ================================ Declaraciones ===============================
// Cada tarea es una función “rápida”, sin bloqueos largos.

#ifdef WDT_ENABLE
static void task_wdt();
#endif

static void task_sound_loop();	 // Bucle de procesamiento de audio
static void task_gas();			 // Lectura del sensor de gas (CCS811)
static void task_mqtt_loop();	 // Conexión / loop principal de MQTT
static void task_mqtt_sensors(); // Envio de datos de sensores por MQTT
static void task_time_sync();	 // Sincronización de hora
static void task_health();		 // Metricas / housekeeping (FreeMem, Uptime)
static void task_temperature();	 // Lectura del sensor de temperatura

#ifdef ENABLE_LUX_SENSOR
static void task_lux(); // Lectura del sensor de luz (Lux)
#endif

#ifdef ENABLE_BUZZER
static void task_buzzer_loop(); // Bucle de control del zumbador
#endif

#ifdef ENABLE_NEOPIXEL
static void task_leds_loop(); // Bucle de control de los LEDs (Neopixel)
#endif

#ifdef HAS_PARTICLE_DATA
static void task_particles(); // Lectura del sensor de partículas (PMS)
static bool g_pms_publish_pending = false; // Flag: publicar sensores al dormir PMS
#endif

#ifdef ENABLE_LCD
static void task_screen(); // Refresco de pantalla
#endif

// ================================ Scheduler ===================================
struct Task
{
	const char *name;
	uint32_t period_ms;	  // 0 => deshabilitada
	uint32_t last_run;	  // interno
	void (*fn)();		  // no bloquea
	bool catchup;		  // re-planifica en base al periodo exacto
	bool run_if_flashing; // Permite ejecutar la tarea si estamos flasheando?
};

static Task *g_tasks = nullptr;
static size_t g_tasks_count = 0;

static void scheduler_init(Task *tasks, size_t count)
{
	g_tasks = tasks;
	g_tasks_count = count;
	const uint32_t now = now_ms();
	for (size_t i = 0; i < count; ++i)
		tasks[i].last_run = now;
}

static void scheduler_run_once(uint32_t budget_ms = 0)
{
	const uint32_t now = now_ms();

	for (size_t i = 0; i < g_tasks_count; ++i)
	{
		Task &t = g_tasks[i];
		if (t.period_ms == 0)
			continue;

		if (is_flashing && !t.run_if_flashing)
			continue;

		if (elapsed_since(t.last_run, t.period_ms))
		{
			t.last_run = t.catchup ? (t.last_run + t.period_ms) : now;
			t.fn();
		}

		if (budget_ms)
		{
			if ((now_ms() - now) >= budget_ms)
				break;
		}
	}
}

static Task g_task_table[] = {

#ifdef WDT_ENABLE
	{"wdt", WDT_TICK_PACE, 0, task_wdt, true, true},
#endif

	{"sound_loop", 10, 0, task_sound_loop, true, false},

#ifdef ENABLE_BUZZER
	{"buzzer_loop", 5, 0, task_buzzer_loop, true, false},
#endif

#ifdef ENABLE_NEOPIXEL
	{"leds_loop", 5, 0, task_leds_loop, true, false},
#endif

	{"mqtt_loop", 50, 0, task_mqtt_loop, true, false},

#ifdef ENABLE_LCD
	{"screen", DISPLAY_TASK_INTERVAL, 0, task_screen, false, false},
#endif

	{"temperature", BME280_POLLING_INTERVAL, 0, task_temperature, false, false},

#ifdef ENABLE_LUX_SENSOR
	{"lux", BH1750_POLLING_INTERVAL, 0, task_lux, false, false},
#endif

	{"gas", 1, 0, task_gas, false, false},

#ifdef HAS_PARTICLE_DATA
	{"particles", 1, 0, task_particles, false, false},
#endif

	{"mqtt_sensors", MQTT_OUTPUT_INTERVAL, 0, task_mqtt_sensors, false, false},
	// noise: ya no necesita task periódica, SoundMeter publica adaptativamente via callback
	{"sync_time", SYNC_TIME_INTERVAL, 0, task_time_sync, false, false},
	{"health", HEALTH_TASK_INTERVAL, 0, task_health, false, false},
	{"persist", 10000, 0, task_persist_save, false, false},
};

// ================================ Setup/Loop ==================================
void setup()
{

	// Inicializa hardware base
	SYS::begin();
#ifdef ENABLE_LCD
	SCREEN::begin();
#endif
#ifdef ENABLE_NEOPIXEL
	// Configuramos el objeto que controla los neopixel
	system_leds = new LedController(NEOPIXEL_DATA_PIN, NEOPIXEL_LED_COUNT, NEOPIXEL_LED_BRIGHTNESS);
#endif
	delay(3000);

	splash();

	if (!PERSISTENCE::begin())
	{
		LOG_WARN("Persistence init failed");
	}
	else
	{
		LOG_INFO("Persistence loaded");
	}
	SENSORS::begin(); // inicializa sensores disponibles por flags

#ifdef HAS_PARTICLE_DATA
	// Configura duty-cycle del PMS desde EEPROM
	pms.configureDutyCycle(
		PERSISTENCE::cfg().pms_sleep_min,
		PERSISTENCE::cfg().pms_wake_sec
	);
	pms.setStateCallback([](const char* s){
		COMMS::mqtt_publish_pms_state(s);
		if (s[0] == 's') g_pms_publish_pending = true;  // "sleeping" → publicar datos frescos
	});
#endif

	// Configuramos el sistema de procesamiento de audio
	SOUND::begin();

	// Aplica calibración de micrófono almacenada en EEPROM (si existe)
	{
		float nf = PERSISTENCE::cfg().audio.mic_noise_floor;
		if (nf < -10.0f && nf > -90.0f && sound_meter) {
			sound_meter->applyNoiseFloor(nf);
			LOG_INFO("Mic noise floor loaded from EEPROM: %.1f dBFS", nf);
		}
	}

#ifdef ENABLE_BUZZER
	BUZZER::begin();
#endif

	COMMS::mqtt_connect();

	// Vinculamos los eventos del sistema, por ejemplo el
	// inicio de actualizacion del firmware o el reset
	SYS::set_event_handlers();

#ifdef WDT_ENABLE
	// Iniciamos el WDT
	PhotonWdgs::begin(USE_WWDT, USE_IWDT, WDT_TIMEOUT, TIMER7);
#endif

	scheduler_init(g_task_table, sizeof(g_task_table) / sizeof(g_task_table[0]));

	// Iniciamos el sampleo de audio :)
	SOUND::start_sampling();

	LOG_INFO("Setup done.");
}

void loop()
{
	PROF_SCOPE("main_loop");
	scheduler_run_once(/*budget_ms*/ 100);
}

// ============================== Tareas concretas ==============================
static void task_persist_save()
{
	PERSISTENCE::save_if_dirty();
}
static void task_sound_loop()
{
	PROF_SCOPE("task_sound_loop");
	SOUND::loop();
}

static void task_gas()
{
	PROF_SCOPE("read_gas_sensor");
	SENSORS::read_gas_sensor();
}

static void task_mqtt_loop()
{
	PROF_SCOPE("task_mqtt_loop");

	COMMS::mqtt_loop();
}

static void task_mqtt_sensors()
{
	PROF_SCOPE("task_mqtt_sensors");

	COMMS::mqtt_send_sensors();
}

static void task_time_sync()
{
	PROF_SCOPE("task_time_sync");
	SYS::sync_time();
}

static void task_health()
{
	PROF_SCOPE("task_health");
	static uint32_t last_log = 0;

	if (elapsed_since(last_log, 15000))
	{
		last_log = now_ms();
		LOG_INFO("Uptime=%lus FreeMem=%lu", (unsigned long)(last_log / 1000), (unsigned long)System.freeMemory());
		PROF_SNAPSHOT();
	}
}

static void task_temperature()
{
	PROF_SCOPE("task_temperature");
	SENSORS::read_temperature_sensor();
}

#ifdef ENABLE_LUX_SENSOR
static void task_lux()
{
	PROF_SCOPE("task_lux");

	SENSORS::read_lux_sensor();
}
#endif

#ifdef WDT_ENABLE
static void task_wdt()
{
	PROF_SCOPE("task_wdt");

	PhotonWdgs::tickle();
}
#endif

#ifdef ENABLE_BUZZER
static void task_buzzer_loop()
{
	PROF_SCOPE("task_buzzer_loop");

	BUZZER::loop();
}
#endif

#ifdef ENABLE_NEOPIXEL
static void task_leds_loop()
{
	PROF_SCOPE("task_leds_loop");

	system_leds->loop();
}
#endif

#ifdef HAS_PARTICLE_DATA
static void task_particles()
{
	PROF_SCOPE("task_particles");

	SENSORS::read_pms_sensor();

	// Publicar sensores justo cuando el PMS va a dormir (datos más frescos)
	if (g_pms_publish_pending) {
		g_pms_publish_pending = false;
		COMMS::mqtt_send_sensors();
		// Resetear timer periódico para evitar doble publicación
		for (size_t i = 0; i < g_tasks_count; ++i) {
			if (g_tasks[i].fn == task_mqtt_sensors) {
				g_tasks[i].last_run = millis();
				break;
			}
		}
		LOG_INFO("PMS sensor publish on sleep transition");
	}
}
#endif

#ifdef ENABLE_LCD
static void task_screen()
{
	PROF_SCOPE("task_screen");

	static uint32_t last_top = 0;
	static uint32_t last_mid = 0;
	static uint32_t last_bot = 0;
	static uint32_t last_flush = 0;
	static uint32_t last_any_paint = 0;

	const uint32_t now = millis();

	// Pinta por bandas: sólo tocan el framebuffer y marcan sucio
	if ((now - last_top) >= DISPLAY_TASK_TOP_EVERY_MS)
	{
		SCREEN::paint_top();
		last_top = now;
		last_any_paint = now;
	}

	if ((now - last_mid) >= DISPLAY_TASK_MID_EVERY_MS)
	{
		SCREEN::paint_mid();
		last_mid = now;
		last_any_paint = now;
	}

	if ((now - last_bot) >= DISPLAY_TASK_BOT_EVERY_MS)
	{
		SCREEN::paint_bottom();
		last_bot = now;
		last_any_paint = now;
	}

	if ((now - last_flush) >= DISPLAY_TASK_FLUSH_MIN_GAP_MS)
	{
		bool flushed = SCREEN::flush_if_dirty();
		if (flushed)
		{
			last_flush = now;
		}
		else if ((now - last_any_paint) >= DISPLAY_TASK_FLUSH_MAX_WAIT_MS)
		{
			// Si hay riesgo de acumular suciedad demasiado tiempo, fuerza flush
			if (SCREEN::flush_if_dirty())
			{
				last_flush = now;
			}
		}
	}
}
#endif

void splash()
{
#ifdef ENABLE_SERIAL_DEBUG
	LOG_INFO("SAQAirQuality started...");
	LOG_INFO("DEVICE_ID: %d", AIR_QUALITY_ID);
	LOG_INFO("Free memory: %lu", (unsigned long)System.freeMemory());
	LOG_INFO(
		"EMA alphas - TMP[%.3f] HUM[%.3f] PRE[%.3f] LUX[%.3f] CO2[%.3f] TVOC[%.3f] PMS_sz[%u]",
		temperature_avg.alpha, humidity_avg.alpha,
		pressure_avg.alpha, lux_avg.alpha,
		co2_avg.alpha, tvoc_avg.alpha,
		(unsigned)pms.getSize());

#ifdef TEMP_SENSOR_BMP280
	LOG_INFO("Temeprature sensor BMP280");
#endif
#ifdef PROFILE_TIMES
	LOG_INFO("Loop time profile - active");
#endif
#ifdef ENABLE_MEGUNO_TIMEPLOT_DEBUG
	LOG_INFO("Serial data out - active");
#endif
#ifdef ENABLE_SERIAL_MQTT_DEBUG
	LOG_INFO("Serial MQTT data out - active");
#endif
#ifdef ENABLE_MEGUNO_DEBUG
	LOG_INFO("Serial MEGUNOLINK data out - active");
#endif
#endif
}
