#pragma once
#include "comms.h"   // asegura visibilidad de s_mqtt, topics y LOG_*

namespace COMMS {

// ===== Forward decl de handlers (los implementa cada .cpp en handlers/) =====
  namespace HANDLERS
  {
#ifdef ENABLE_NEOPIXEL
    bool on_led_control(const char *topic, const char *payload);
#endif
#ifdef ENABLE_BUZZER
    bool on_buzzer_control(const char *topic, const char *payload);
    bool on_buzzer_play(const char *topic, const char *payload);
#endif
    bool on_system_reboot(const char *topic, const char *payload);

    bool on_offsets_set(const char *topic, const char *payload);
    bool on_offsets_get(const char *topic, const char *payload);
    bool on_audio_cal(const char *topic, const char *payload);
#ifdef HAS_PARTICLE_DATA
    bool on_pms_cfg(const char *topic, const char *payload);
#endif
#ifdef HA_ENABLE_DISCOVERY
    bool on_ha_command(const char *topic, const char *payload);
#endif
  }


  // Firma de handler: true => consumido
  using MqttHandler = bool (*)(const char* topic, const char* payload);

  struct MqttRoute {
    const char* topic;   // subtopic (p.ej. MQTT_LED_CONTROL_SUBTOPIC) o topic completo
    MqttHandler handler;
    bool suffix;         // true: match por sufijo (…endsWith)
  };

} // namespace COMMS
