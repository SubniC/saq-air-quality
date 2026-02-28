#include "config/config.h"
#ifdef ENABLE_BUZZER

#include "comms_router.h"
#include "comms_json.h"
#include "comms_parse_json.h"
#include "utils/debug.h"
#include "buzzer.h"

/*
{ "melody": "C5:2,D5:2,G4:4" }   // reproduce
// ó
{ "stop": true }                  // detiene
*/

namespace COMMS
{
  namespace HANDLERS
  {

    namespace JP = COMMS::JSON::PARSER;

    bool on_buzzer_control(const char * /*topic*/, const char *payload)
    {
      JP::Doc d;
      if (!JP::parse(d, payload))
      {
        return COMMS::JSON::publish_status_err(MQTT_BUZZER_STATUS_TOPIC, "invalid json");
      }

      bool enabled = false;
      if (!JP::get_bool(d, "enabled", enabled))
      {
        return COMMS::JSON::publish_status_err(MQTT_BUZZER_STATUS_TOPIC, "missing 'enabled' (bool)");
      }

      if (enabled)
      {
        BUZZER::enable();
        LOG_INFO("BUZZER ENABLED");
        return COMMS::JSON::publish_status_ok(MQTT_BUZZER_STATUS_TOPIC, "enabled");
      }
      else
      {
        BUZZER::disable();
        LOG_INFO("BUZZER DISABLED");
        return COMMS::JSON::publish_status_ok(MQTT_BUZZER_STATUS_TOPIC, "disabled");
      }
    }

    bool on_buzzer_play(const char * /*topic*/, const char *payload)
    {
      JP::Doc d;
      if (!JP::parse(d, payload))
      {
        return COMMS::JSON::publish_status_err(MQTT_BUZZER_STATUS_TOPIC, "invalid json");
      }

      // Prioridad: melody -> stop
      char melody[192];
      if (JP::get_str(d, "melody", melody, sizeof(melody)) && melody[0])
      {
        BUZZER::play(melody);
        LOG_INFO("BUZZER PLAY: %s", melody);
        return COMMS::JSON::publish_status_ok(MQTT_BUZZER_STATUS_TOPIC, "playing");
      }

      bool stop = false;
      if (JP::get_bool(d, "stop", stop) && stop)
      {
        BUZZER::silence();
        LOG_INFO("BUZZER STOP");
        return COMMS::JSON::publish_status_ok(MQTT_BUZZER_STATUS_TOPIC, "stopped");
      }

      return COMMS::JSON::publish_status_err(MQTT_BUZZER_STATUS_TOPIC, "missing 'melody' or 'stop'");
    }

  }
} // namespace COMMS::HANDLERS

#endif // ENABLE_BUZZER
