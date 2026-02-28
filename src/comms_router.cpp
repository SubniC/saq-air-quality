#include "comms_router.h"
#include "utils/debug.h" // para LOG_*
#include <cstring>

// ===== Helpers de matching =====
static inline bool cstr_ends_with(const char *s, const char *suffix)
{
  if (!s || !suffix)
    return false;
  const size_t ns = strlen(s), nx = strlen(suffix);
  if (nx > ns)
    return false;
  return memcmp(s + (ns - nx), suffix, nx) == 0;
}

// ===== Tabla de rutas =====
static const COMMS::MqttRoute s_routes[] = {
#ifdef ENABLE_NEOPIXEL
    {MQTT_LED_CONTROL_SUBTOPIC, COMMS::HANDLERS::on_led_control, true},
#endif
#ifdef ENABLE_BUZZER
    {MQTT_BUZZER_CONTROL_SUBTOPIC, COMMS::HANDLERS::on_buzzer_control, true},
    {MQTT_BUZZER_PLAY_SUBTOPIC, COMMS::HANDLERS::on_buzzer_play, true},
#endif
    {MQTT_SYSTEM_REBOOT_SUBTOPIC, COMMS::HANDLERS::on_system_reboot, true},

    {MQTT_OFFSETS_SET_SUBTOPIC, COMMS::HANDLERS::on_offsets_set, true},
    {MQTT_OFFSETS_GET_SUBTOPIC, COMMS::HANDLERS::on_offsets_get, true},

    {MQTT_AUDIO_CAL_CMD_SUBTOPIC, COMMS::HANDLERS::on_audio_cal, true},

#ifdef HAS_PARTICLE_DATA
    {MQTT_PMS_CFG_CMD_SUBTOPIC, COMMS::HANDLERS::on_pms_cfg, true},
#endif

#ifdef HA_ENABLE_DISCOVERY
    {MQTT_HA_CMD_SUBTOPIC, COMMS::HANDLERS::on_ha_command, true},
#endif
};
static const size_t s_routes_count = sizeof(s_routes) / sizeof(s_routes[0]);

// ===== Callback MQTT (declarado en comms.h) =====
void COMMS::on_mqtt_message_callback(char *topic, byte *payload, unsigned int length)
{
  // NUL-terminate seguro
  const unsigned int n = (length < (MQTT_IN_BUFFER - 1)) ? length : (MQTT_IN_BUFFER - 1);
  if (length >= MQTT_IN_BUFFER) {
    LOG_WARN("MQTT RX payload truncated: %u > %u", length, (unsigned)(MQTT_IN_BUFFER - 1));
  }
  memcpy(g_mqtt_in, payload, n);
  g_mqtt_in[n] = '\0';

  LOG_MQTT_RECEIVE(topic, g_mqtt_in);

  // Router: intenta cada ruta
  for (size_t i = 0; i < s_routes_count; ++i)
  {
    const auto &r = s_routes[i];
    const bool match = r.suffix ? cstr_ends_with(topic, r.topic)
                                : (strcmp(topic, r.topic) == 0);
    if (match && r.handler(topic, g_mqtt_in))
      return; // consumido
  }

  LOG_WARN("MQTT RX topic sin handler: %s", topic);
}
