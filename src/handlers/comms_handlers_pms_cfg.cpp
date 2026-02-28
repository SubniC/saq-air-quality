#include "config/config.h"
#ifdef HAS_PARTICLE_DATA

#include "comms.h"
#include "comms_json.h"
#include "comms_parse_json.h"
#include "persistence.h"
#include "sensors.h"
#include "utils/debug.h"
#include "config/config.h"

namespace COMMS
{
    namespace HANDLERS
    {
        using COMMS::JSON::PARSER::Doc;
        using COMMS::JSON::PARSER::get_str;
        using COMMS::JSON::PARSER::get_u32;
        using COMMS::JSON::PARSER::parse;

        static const char *duty_state_str(SaqPMS::DutyState s)
        {
            switch (s) {
                case SaqPMS::DutyState::ACTIVE:   return "active";
                case SaqPMS::DutyState::WARMUP:   return "warmup";
                case SaqPMS::DutyState::SLEEPING: return "sleeping";
                default:                            return "unknown";
            }
        }

        static bool _publish_pms_cfg()
        {
            JSON::Builder jb;
            if (!COMMS::JSON::begin_reply(&jb, g_mqtt_out, sizeof(g_mqtt_out), "OK", "pms_cfg"))
                return false;

            COMMS::JSON::kv_u32(&jb, "sleep_min", (uint32_t)pms.getSleepMin());
            COMMS::JSON::kv_u32(&jb, "wake_sec",  (uint32_t)pms.getWakeSec());
            COMMS::JSON::kv_str(&jb, "state",     duty_state_str(pms.getDutyState()));

            return COMMS::JSON::end_and_publish(&jb, MQTT_PMS_CFG_STATUS_TOPIC);
        }

        bool on_pms_cfg(const char * /*topic*/, const char *payload)
        {
            Doc d{};
            if (!parse(d, payload))
                return COMMS::JSON::publish_status_err(MQTT_PMS_CFG_STATUS_TOPIC, "bad-json");

            char action[16] = {0};
            if (!get_str(d, "action", action, sizeof(action)))
                return COMMS::JSON::publish_status_err(MQTT_PMS_CFG_STATUS_TOPIC, "missing-action");

            // --- GET ---
            if (strcmp(action, "get") == 0)
                return _publish_pms_cfg();

            // --- SET ---
            if (strcmp(action, "set") == 0)
            {
                uint32_t sleep_min = pms.getSleepMin();
                uint32_t wake_sec  = pms.getWakeSec();

                get_u32(d, "sleep_min", sleep_min);
                get_u32(d, "wake_sec",  wake_sec);

                // Validar rangos
                if (sleep_min > 60)
                    return COMMS::JSON::publish_status_err(MQTT_PMS_CFG_STATUS_TOPIC, "sleep_min-max-60");
                if (wake_sec > 120)
                    return COMMS::JSON::publish_status_err(MQTT_PMS_CFG_STATUS_TOPIC, "wake_sec-max-120");

                // Aplicar en caliente
                pms.configureDutyCycle((uint8_t)sleep_min, (uint8_t)wake_sec);

                // Persistir
                PERSISTENCE::cfg().pms_sleep_min = (uint8_t)sleep_min;
                PERSISTENCE::cfg().pms_wake_sec  = (uint8_t)wake_sec;
                PERSISTENCE::mark_dirty();
                PERSISTENCE::save_if_dirty(0);

                return _publish_pms_cfg();
            }

            return COMMS::JSON::publish_status_err(MQTT_PMS_CFG_STATUS_TOPIC, "unknown-action");
        }

    } // namespace HANDLERS
} // namespace COMMS

#endif // HAS_PARTICLE_DATA
