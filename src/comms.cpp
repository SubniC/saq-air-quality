#include "comms.h"
#include "comms_router.h"

#include "config/config.h"

#include "sys.h"
#include "sensors.h"

#include "utils/debug.h"
#include "libs/json.h"

#include "utils/backoff_helpers.h"

#include "MQTT-TLS.h"
#ifdef MQTT_USE_TLS
#include "config/mqtt_tls.h"
#endif

#ifdef HAS_PARTICLE_DATA
#include "libs/SaqPms.h"
#endif
#ifdef ENABLE_AQI
#include "libs/aqi/AqiEngine.h"
#endif

#ifdef ENABLE_NEOPIXEL
#include "libs/LedController.h"
#endif

#ifdef HA_ENABLE_DISCOVERY
#include "ha_discovery.h"
#endif

#ifdef ENABLE_BUZZER
#include "buzzer.h"
#endif

namespace
{
	MQTT *s_mqtt = nullptr;

	// buffer estático para la IP (evitamos temporales)
	uint8_t s_broker_ip[4] = {0, 0, 0, 0};

	MqttConnectResult s_last_connect_result = MqttConnectResult::Ok;
#ifdef HA_ENABLE_DISCOVERY
	bool s_ha_announced = false;
#endif

	// helper: ¿es "a.b.c.d"? Si sí, rellena s_broker_ip
	bool parseIPv4(const char *s, uint8_t out[4])
	{
		unsigned v = 0;
		int parts = 0;
		while (*s)
		{
			if (*s == '.')
			{
				if (parts >= 3 || v > 255)
					return false;
				out[parts++] = (uint8_t)v;
				v = 0;
				++s;
				continue;
			}
			if (*s < '0' || *s > '9')
				return false;
			v = v * 10 + (unsigned)(*s - '0');
			if (v > 255)
				return false;
			++s;
		}
		if (parts != 3 || v > 255)
			return false;
		out[3] = (uint8_t)v;
		return true;
	}

#ifdef ENABLE_AQI
	static const char *scopeToStr(AQI::Scope s)
	{
		switch (s)
		{
		case AQI::Scope::Minute:
			return "minute";
		case AQI::Scope::Hour:
			return "hour";
		case AQI::Scope::Day:
			return "day";
		case AQI::Scope::NowCast:
			return "nowcast";
		}
		return "unknown";
	}

	static const char *polToStr(AQI::Pollutant p)
	{
		return (p == AQI::Pollutant::PM25) ? "pm25" : "pm10";
	}
#endif

	// Crea el cliente si no existe
	bool ensure_client()
	{
		if (s_mqtt)
			return true;
		// El literal viene de config.h
		static char mqttHost[] = MQTT_HOST;
		if (parseIPv4(mqttHost, s_broker_ip))
		{
			s_mqtt = new MQTT(s_broker_ip, MQTT_PORT, COMMS::on_mqtt_message_callback, MQTT_LIB_MAX_PACKET_SIZE);
		}
		else
		{
			s_mqtt = new MQTT(mqttHost, MQTT_PORT, COMMS::on_mqtt_message_callback, MQTT_LIB_MAX_PACKET_SIZE);
		}
		LOG_INFO("MQTT client OK");

#ifdef MQTT_USE_TLS
		s_mqtt->enableTls(TLS_CONFIG::ca_bundle_data(), TLS_CONFIG::ca_bundle_size());
		LOG_INFO("MQTT: TLS Enabled");
#endif
		return (s_mqtt != nullptr);
	}

	// Desconecta de forma segura si está conectado
	void disconnect_if_connected()
	{
		if (s_mqtt && s_mqtt->isConnected())
		{
			s_mqtt->disconnect();
		}
	}

	// Suscripciones centralizadas con control de retorno
	bool subscribe_all()
	{
		struct Sub
		{
			const char *topic;
			bool enabled;
		};

		// Construimos la lista en compile-time según #defines
		const Sub subs[] = {
#ifdef ENABLE_NEOPIXEL
			{MQTT_LED_CONTROL_TOPIC, true},
#endif
#ifdef ENABLE_BUZZER
			{MQTT_BUZZER_CONTROL_TOPIC, true},
			{MQTT_BUZZER_PLAY_TOPIC, true},
#endif
			{MQTT_SYSTEM_REBOOT_TOPIC, true},
			{MQTT_OFFSETS_SET_TOPIC, true},
			{MQTT_OFFSETS_GET_TOPIC, true},
			{MQTT_AUDIO_CAL_CMD_TOPIC, true},
#ifdef HAS_PARTICLE_DATA
			{MQTT_PMS_CFG_CMD_TOPIC, true},
#endif
#ifdef HA_ENABLE_DISCOVERY
			{MQTT_HA_CMD_TOPIC, true},
#endif
#ifdef MQTT_LISTEN_TO_GLOBAL
#ifdef ENABLE_NEOPIXEL
			{MQTT_GLOBAL_LED_CONTROL_TOPIC, true},
#endif
#ifdef ENABLE_BUZZER
			{MQTT_GLOBAL_BUZZER_PLAY_TOPIC, true},
#endif
			{MQTT_GLOBAL_REBOOT_TOPIC, true},
#ifdef HA_ENABLE_DISCOVERY
			{MQTT_GLOBAL_HA_CMD_TOPIC, true},
#endif
#endif
		};

		bool all_ok = true;
		for (const auto &s : subs)
		{
			if (!s.enabled)
				continue;
			const bool ok = s_mqtt->subscribe(s.topic);
			LOG_INFO("%s %s", ok ? "SUB OK" : "SUB FAIL", s.topic);
			if (!ok)
				all_ok = false;
		}
		return all_ok;
	}

	// Publica el LWT "ONLINE" (retained) y loguea
	bool publish_lwt_online()
	{
		const bool ok = s_mqtt->publish(MQTT_LWT_TOPIC,
										(const uint8_t *)MQTT_LWT_MESSAGE_ON,
										strlen(MQTT_LWT_MESSAGE_ON),
										MQTT_LWT_RETAIN);
		LOG_INFO("%s LWT online -> %s", ok ? "PUB OK" : "PUB FAIL", MQTT_LWT_TOPIC);
		return ok;
	}

	// Lanza el discovery sólo una vez por arranque (opcional)
	// Si quieres re-publicarlo en cada reconexión, elimina el guard.
	bool announce_discovery_once()
	{
#ifdef HA_ENABLE_DISCOVERY
		if (!s_ha_announced)
		{
			const bool ok = HA::publish_discovery_all(); // retained
			LOG_INFO("%s HA discovery", ok ? "DISCOVERY OK" : "DISCOVERY FAIL");
			if (ok)
				s_ha_announced = true;
			return ok;
		}
#endif
		return true;
	}

}

// ==============================
//  Implementaciones del modulo comms
// ==============================
char g_mqtt_out[MQTT_OUT_BUFFER];
char g_mqtt_in[MQTT_IN_BUFFER];
char g_mqtt_topic[MQTT_TOPIC_BUFFER];

MqttConnCtl g_mqttc; // estado global de conexión

bool COMMS::mqtt_publish(const char *mqtt_topic, const char *mqtt_message, bool retain)
{
	if (s_mqtt->isConnected())
	{
		s_mqtt->publish(mqtt_topic, (uint8_t *)mqtt_message, strlen(mqtt_message), retain);
		LOG_MQTT_PUBLISH(mqtt_topic, mqtt_message);
		return true;
	}
	return false;
}

bool COMMS::mqtt_connect()
{
	g_mqttc.last_attempt_ms = millis();
	s_last_connect_result = MqttConnectResult::Ok;
	LOG_INFO("MQTT connecting...");

	if (!ensure_client())
	{
		s_last_connect_result = MqttConnectResult::FailEnsureClient;
		mqtt_connection_error();
		return false;
	}

	// Asegura un estado limpio antes de conectar
	disconnect_if_connected();

	// Conexión
	const bool connected = s_mqtt->connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWD,
										   MQTT_LWT_TOPIC, MQTT::QOS0, /*retain=*/1,
										   MQTT_LWT_MESSAGE_OFF, /*cleanSession=*/true,
										   MQTT::MQTT_V311);
	if (!connected)
	{
		s_last_connect_result = MqttConnectResult::FailConnect;
		mqtt_connection_error();
		return false;
	}

	// Ya estamos conectados. Paso 1: suscripciones
	bool subs_ok = subscribe_all();
	if (!subs_ok)
	{
		// Conectado, pero con suscripciones fallidas: dejamos conexión viva y lo marcamos como "soft-fail".
		s_last_connect_result = MqttConnectResult::SoftFail_Subscribe;
	}

	// Paso 2: publicar LWT ONLINE (retained)
	bool lwt_ok = publish_lwt_online();
	if (!lwt_ok)
	{
		// De nuevo, soft-fail: seguimos conectados, pero lo registramos
		s_last_connect_result = MqttConnectResult::SoftFail_LwtOnline;
	}

	// Paso 3: anunciar discovery sólo una vez (opcional)
	bool disc_ok = announce_discovery_once();
	if (!disc_ok)
	{
		s_last_connect_result = MqttConnectResult::SoftFail_Discovery;
	}

	// Paso 4: publicar info del dispositivo (retained)
	mqtt_publish_info();

	// Señales visuales/acústicas de éxito
	mqtt_connection_success();
	return true;
}

void COMMS::mqtt_connection_success(void)
{
	g_mqttc.state = MqttState::CONNECTED;
	g_mqttc.backoff_ms = 0;

	LOG_INFO("MQTT OK %s:%d", MQTT_HOST, MQTT_PORT);

#ifdef ENABLE_BUZZER
	BUZZER::success();
#endif

#ifdef ENABLE_NEOPIXEL
	system_leds->fade(HTMLColorCode::Green, HTMLColorCode::Black, 750);
#endif
}

void COMMS::mqtt_connection_error(void)
{
	g_mqttc.state = MqttState::BACKOFF;
	g_mqttc.backoff_ms = next_backoff(g_mqttc.backoff_ms, MQTT_RECONNECT_INTERVAL, MQTT_RECONNECT_INTERVAL * 12, 2, 15);
	g_mqttc.last_attempt_ms = millis();
	LOG_ERROR("MQTT fail, backoff=%lums", g_mqttc.backoff_ms);

#ifdef ENABLE_BUZZER
	BUZZER::error();
#endif

#ifdef ENABLE_NEOPIXEL
	system_leds->scanner(HTMLColorCode::Red, 1500);
#endif
}

bool COMMS::network_ready()
{
	return (WiFi.ready() && WiFi.localIP()[0] != 0);
}

bool COMMS::mqtt_loop()
{
	// 1) Si estamos conectados, bombear y salir
	if (g_mqttc.state == MqttState::CONNECTED)
	{
		s_mqtt->loop();
		if (s_mqtt->isConnected())
		{
			return true; // seguimos OK
		}
		mqtt_connection_error();
		return false;
	}

	const unsigned long now = millis();

	// 2) Sin red -> no intentamos nada; resetea a DISCONNECTED
	if (!network_ready())
	{
		if (g_mqttc.state != MqttState::DISCONNECTED)
		{
			LOG_ERROR("NET DOWN st=%u", (uint8_t)g_mqttc.state);
			g_mqttc.state = MqttState::DISCONNECTED;
			g_mqttc.backoff_ms = 0;
		}
		return false;
	}

	// 3) Respetar backoff si estamos en BACKOFF
	if (g_mqttc.state == MqttState::BACKOFF)
	{
		if ((now - g_mqttc.last_attempt_ms) < g_mqttc.backoff_ms)
		{
			return false; // todavía no
		}
		// listo para reintentar
		g_mqttc.state = MqttState::DISCONNECTED;
	}

	// 4) Intento de conexión cuando toca
	if (g_mqttc.state == MqttState::DISCONNECTED)
	{
		return mqtt_connect();
	}

	return false;
}

#ifdef ENABLE_AQI
void COMMS::publish_aqi_event(const AQI::Event &ev)
{
	// Topic: base/aqi/pm25/minute  (ajusta BASE_TOPIC si lo tienes en config)
	snprintf(g_mqtt_topic, sizeof(g_mqtt_topic), "%s%s/%s", MQTT_AQI_BASE_TOPIC, polToStr(ev.pollutant), scopeToStr(ev.scope));

	JSON::Builder jb;
	JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));

	JSON::add_str(jb, "pollutant", polToStr(ev.pollutant));
	JSON::add_str(jb, "scope", scopeToStr(ev.scope));
	JSON::add_float(jb, "conc_ugm3", ev.conc, 1);
	JSON::add_num(jb, "aqi", (long)ev.aqi);
	JSON::add_num(jb, "time", (long)ev.ts); // cierre del bucket (epoch s, local-aligned)

	JSON::end(jb);

	// Aviso si hubo truncado (el JSON sigue siendo válido gracias al builder)
	if (jb.truncated)
	{
		LOG_WARN("JSON trunc aqi %u/%u",
				 (unsigned)JSON::size(jb), (unsigned)sizeof(g_mqtt_out));
	}

	COMMS::mqtt_publish(g_mqtt_topic, JSON::c_str(jb));
}
#endif

bool COMMS::mqtt_send_sensors()
{
	JSON::Builder jb;
	JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));

#ifdef HAS_PARTICLE_DATA
	// Partículas (sin decimales)
	JSON::add_float(jb, "pm1_ugm3", pms.getCurrentValue(SaqPmsDataField_t::PM1dot0), 0);
	JSON::add_float(jb, "pm25_ugm3", pms.getCurrentValue(SaqPmsDataField_t::PM2dot5), 0);
	JSON::add_float(jb, "pm10_ugm3", pms.getCurrentValue(SaqPmsDataField_t::PM10dot0), 0);

#ifdef PARTICLE_SENSOR_MODEL_PMS5003ST
	JSON::add_float(jb, "hcho", pms.getCurrentValue(SaqPmsDataField_t::HCHO), 0);
#endif
#endif

#ifdef ENABLE_LUX_SENSOR
	JSON::add_float(jb, "lux", lux_avg.get(), 0);
#endif

	// Métricas ambientales
	JSON::add_float(jb, "temp_c", SENSORS::get_current_temperature(), 2);
#ifdef HAS_HUMIDITY_DATA
	JSON::add_float(jb, "hum_percent", SENSORS::get_current_humidity(), 2);
#endif
	// Presión se enviaba como entero ya dividido entre 100
	JSON::add_float(jb, "press_mb", pressure_avg.get() / 100.0f, 0);

	// Gas (enteros)
	JSON::add_float(jb, "co2_ppm", co2_avg.get(), 0);
	JSON::add_float(jb, "tvoc_ppm", tvoc_avg.get(), 0);

	// Epoch local (segundos)
	JSON::add_num(jb, "time", (long)Time.local());

	JSON::end(jb);

	// Aviso si hubo truncado (el JSON sigue siendo válido gracias al builder)
	if (jb.truncated)
	{
		LOG_WARN("JSON trunc sensors %u/%u",
				 (unsigned)JSON::size(jb), (unsigned)sizeof(g_mqtt_out));
	}

	return COMMS::mqtt_publish(MQTT_SENSORS_TOPIC, JSON::c_str(jb));
}

bool COMMS::mqtt_send_noise(float dbfs, float dbfs_slow)
{
	JSON::Builder jb;
	JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));

	JSON::add_float(jb, "dbfs", dbfs, 1);           // nivel actual (IIR ~2s)
	JSON::add_float(jb, "dbfs_slow", dbfs_slow, 1); // nivel ambiente (IIR ~30s)
	JSON::add_num(jb, "time", (long)Time.local());
	JSON::end(jb);

	if (jb.truncated)
	{
		LOG_WARN("JSON trunc noise %u/%u",
				 (unsigned)JSON::size(jb), (unsigned)sizeof(g_mqtt_out));
	}

	return COMMS::mqtt_publish(MQTT_NOISE_TOPIC, JSON::c_str(jb));
}

bool COMMS::mqtt_publish_info()
{
	JSON::Builder jb;
	JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));
	JSON::add_str(jb, "fw", DEVICE_SOFTWARE_VERSION);
	JSON::add_str(jb, "build", DEVICE_BUILD_TIMESTAMP);
	JSON::add_num(jb, "device", (long)AIR_QUALITY_ID);
	JSON::add_num(jb, "uptime_s", (long)(millis() / 1000));
	JSON::add_str(jb, "ip", WiFi.localIP().toString().c_str());
	JSON::end(jb);
	return COMMS::mqtt_publish(MQTT_INFO_TOPIC, JSON::c_str(jb), /*retained=*/true);
}

#ifdef HAS_PARTICLE_DATA
bool COMMS::mqtt_publish_pms_state(const char *state)
{
	JSON::Builder jb;
	JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));
	JSON::add_str(jb, "state", state);
	JSON::add_num(jb, "time", (long)Time.local());
	JSON::end(jb);
	return COMMS::mqtt_publish(MQTT_PMS_STATE_TOPIC, JSON::c_str(jb), /*retained=*/true);
}
#endif

#ifdef AUDIO_ENABLE_DETECT_CLAPS
// count -> numero de palmas detectadas
bool COMMS::mqtt_send_claps(uint8_t count, unsigned long period_ms, float peak_dbfs, float ambient_dbfs, float snr_db)
{
	JSON::Builder jb;
	JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));

	// Si prefieres topic unificado, también puedes añadir: JSON::add_str(jb, "type", "clap");
	JSON::add_num(jb, "count", (long)count);
	JSON::add_num(jb, "period", (long)period_ms); // ms (asumo)
	JSON::add_float(jb, "peak_dbfs", peak_dbfs, 1);
	JSON::add_float(jb, "ambient_dbfs", ambient_dbfs, 1);
	JSON::add_float(jb, "snr_db", snr_db, 1);

	// hora local; para UTC usa Time.now()
	JSON::add_num(jb, "time", (long)Time.local());

	JSON::end(jb);

	// Aviso si hubo truncado (el JSON sigue siendo válido gracias al builder)
	if (jb.truncated)
	{
		LOG_WARN("JSON trunc claps %u/%u",
				 (unsigned)JSON::size(jb), (unsigned)sizeof(g_mqtt_out));
	}

	return COMMS::mqtt_publish(MQTT_CLAP_TOPIC, JSON::c_str(jb));
}
#endif

#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
// count -> numero de silvidos detectados
bool COMMS::mqtt_send_whistle(unsigned long dur_ms, uint16_t freq_hz, float tonality, float db)
{
	JSON::Builder jb;
	JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));
	JSON::add_str(jb, "type", "whistle");
	JSON::add_num(jb, "duration_ms", (long)dur_ms);
	JSON::add_num(jb, "freq_hz", (long)freq_hz);
	JSON::add_float(jb, "tonality", tonality, 2);
	JSON::add_float(jb, "level_dbfs", db, 1);
	JSON::end(jb);

	// Aviso si hubo truncado (el JSON sigue siendo válido gracias al builder)
	if (jb.truncated)
	{
		LOG_WARN("JSON trunc whistle %u/%u",
				 (unsigned)JSON::size(jb), (unsigned)sizeof(g_mqtt_out));
	}

	return COMMS::mqtt_publish(MQTT_WHISTLE_TOPIC, JSON::c_str(jb));
}
#endif