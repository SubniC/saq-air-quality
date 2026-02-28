#pragma once

#include "application.h"
#include "config/config.h"

#ifndef MQTT_OUT_BUFFER
#define MQTT_OUT_BUFFER 256 // buffer de salida JSON
#endif
#ifndef MQTT_IN_BUFFER
#define MQTT_IN_BUFFER 128 // buffer de entrada json
#endif
#ifndef MQTT_TOPIC_BUFFER
#define MQTT_TOPIC_BUFFER 96 // buffer de entrada json
#endif


// Forward declarations
class LedController;
namespace AQI
{
  struct Event;
}

extern char g_mqtt_out[MQTT_OUT_BUFFER]; // PARA SALIDA (builder)
extern char g_mqtt_in[MQTT_IN_BUFFER];   // PARA ENTRADA (NUL-terminar RX si hace falta)
// TODO: Habilitar este tambien, ya hay varios sitios con char topic[128]
extern char g_mqtt_topic[MQTT_TOPIC_BUFFER]; // PARA Topics

#ifdef ENABLE_NEOPIXEL
extern LedController *system_leds;
#endif

// Estado de conexión MQTT (backoff + guard)
enum class MqttState : uint8_t {
  DISCONNECTED,
  CONNECTED,
  BACKOFF
};

struct MqttConnCtl {
  MqttState state = MqttState::DISCONNECTED;
  unsigned long backoff_ms = 0;         // backoff actual
  unsigned long last_attempt_ms = 0;    // millis() del último intento
};

enum class MqttConnectResult : uint8_t {
  Ok = 0,
  FailEnsureClient,
  FailConnect,
  SoftFail_Subscribe,     // conectado, pero algún subscribe falló
  SoftFail_LwtOnline,     // conectado, pero publicar LWT online falló
  SoftFail_Discovery      // conectado, pero discovery falló
};

extern MqttConnCtl g_mqttc;

namespace COMMS
{
  
  // void _mqtt_setup_client();
  bool mqtt_loop();
  bool mqtt_connect();
  bool mqtt_send_sensors();
  bool mqtt_send_noise(float dbfs, float dbfs_slow);

  bool mqtt_publish(const char *, const char *, bool=false);
  bool mqtt_publish_info();
#ifdef HAS_PARTICLE_DATA
  bool mqtt_publish_pms_state(const char *state);
#endif
  void mqtt_connection_success(void);
  void mqtt_connection_error(void);
  void on_mqtt_message_callback(char *, byte *, unsigned int);
  bool network_ready();

#ifdef AUDIO_ENABLE_DETECT_CLAPS
  bool mqtt_send_claps(uint8_t count, unsigned long period_ms, float peak_dbfs, float ambient_dbfs, float snr_db);
#endif

#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
  bool mqtt_send_whistle(unsigned long dur_ms, uint16_t freq_hz, float tonality, float db);
#endif

#ifdef ENABLE_AQI
  void publish_aqi_event(const AQI::Event &ev);
#endif
} // namespace MQTT
