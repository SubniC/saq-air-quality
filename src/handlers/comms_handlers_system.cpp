#include "buzzer.h"
#include "comms_router.h"
#include "comms_json.h"
#include "comms_parse_json.h"
#include "utils/debug.h"

/*
{ "now": true, "delay_ms": 500 }  // delay_ms opcional
*/

namespace COMMS
{
  namespace HANDLERS
  {

    namespace JP = COMMS::JSON::PARSER;

    bool on_system_reboot(const char * /*topic*/, const char *payload)
    {
      JP::Doc d;
      if (!JP::parse(d, payload))
      {
        return COMMS::JSON::publish_status_err(MQTT_SYSTEM_STATUS_TOPIC, "invalid json");
      }

      bool now = false;
      if (!JP::get_bool(d, "now", now) || !now)
      {
        return COMMS::JSON::publish_status_err(MQTT_SYSTEM_STATUS_TOPIC, "missing 'now': true");
      }

      uint32_t delay_ms = 0;
      (void)JP::get_u32(d, "delay_ms", delay_ms); // opcional

#if defined(ENABLE_BUZZER)
      BUZZER::reboot(true);
#endif

      // Publica estado y da tiempo a que salga por MQTT antes del reset
      COMMS::JSON::publish_status_ok(MQTT_SYSTEM_STATUS_TOPIC, "rebooting");
      delay(delay_ms ? delay_ms : 500);

      LOG_INFO("SYSTEM RESET (delay %lu ms)", (unsigned long)delay_ms);
      System.reset();
      return true; // no llega
    }

  }
} // namespace COMMS::HANDLERS
