#include <cstdlib>
#include <cstring>
#include "comms.h" // system_leds
#include "comms_parse_json.h"
#include "comms_json.h"
#include "comms_router.h"
#include "utils/debug.h"
#ifdef ENABLE_NEOPIXEL
#include "libs/LedController.h"
#endif

/*
Ejemplos de entrada

{"effect":"scanner","dur_ms":1200,"c1":"ff0000"}
{"effect":"fade","dur_ms":800,"c1":"00ff00","c2":"0000ff"}

*/
namespace COMMS
{
  namespace HANDLERS
  {

    bool on_led_control(const char * /*topic*/, const char *payload)
    {
#ifdef ENABLE_NEOPIXEL
      using namespace COMMS::JSON::PARSER;

      /* -------------------------
         PARSE PAYLOAD
      ----------------------------*/
      if (!payload || !*payload)
      {
        return JSON::publish_status_err(MQTT_LED_STATUS_TOPIC, "empty payload");
      }

      Doc d;
      if (!parse(d, payload))
      {
        return JSON::publish_status_err(MQTT_LED_STATUS_TOPIC, "bad json");
      }

      /* -------------------------
         EVALUATE PARAMETERS
      ----------------------------*/
      char effect[16];
      if (!get_str(d, "effect", effect, sizeof(effect)))
      {
        return JSON::publish_status_err(MQTT_LED_STATUS_TOPIC, "effect missing");
      }

      // TODO: Añadir comprobación para no seguir despues de este punto si el effecto no existe en los que tenemos disponibles! 

      // TODO: Notificar warning, se ha fijado un valor por defecto difierente al solicitado
      const uint16_t dur_ms = get_u16_or(d, "dur_ms", 1000, 100, 30000);

      uint32_t c1 = 0;
      if (!get_hex_rgb(d, "c1", c1))
      {
        return JSON::publish_status_err(MQTT_LED_STATUS_TOPIC, "c1 missing/bad");
      }

      uint32_t c2 = 0;
      if (!get_hex_rgb_or(d, "c2", 0xFFFFFF, c2))
      {
        JSON::publish_status_err(MQTT_LED_STATUS_TOPIC, "c2 missing/bad, using black");
      }

      /* -------------------------
         HANDLE EFFECT
      ----------------------------*/
      if (strcmp(effect, "scanner") == 0)
      {
        system_leds->scanner(c1, dur_ms);
        return JSON::publish_status_ok(MQTT_LED_STATUS_TOPIC, "scanner applied");
      }
      else if (strcmp(effect, "fade") == 0)
      {
        system_leds->fade(c1, c2, dur_ms);
        return JSON::publish_status_ok(MQTT_LED_STATUS_TOPIC, "fade applied");
      }

      return JSON::publish_status_err(MQTT_LED_STATUS_TOPIC, "unknown effect");
    #else
      return false;
    #endif
    }
  }
} // namespace