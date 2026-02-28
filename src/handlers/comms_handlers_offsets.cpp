#include "comms.h"
#include "comms_json.h"       // helpers de construcción JSON
#include "comms_parse_json.h" // parser basado en Particle JSON
#include "persistence.h"
#include "utils/debug.h"
#include <cmath>
#include "config/config.h"

namespace COMMS
{
    namespace HANDLERS
    {

        using COMMS::JSON::PARSER::Doc;
        using COMMS::JSON::PARSER::get_f32;
        using COMMS::JSON::PARSER::has_key;
        using COMMS::JSON::PARSER::parse;

        static bool _publish_all_offsets(const char *topic)
        {
            JSON::Builder jb;
            if (!COMMS::JSON::begin_reply(&jb, g_mqtt_out, sizeof(g_mqtt_out), "OK", "offsets"))
                return false;

            COMMS::JSON::kv_f32(&jb, "temp", PERSISTENCE::cfg().offsets.temp, 2);
            COMMS::JSON::kv_f32(&jb, "hum", PERSISTENCE::cfg().offsets.hum, 2);
            COMMS::JSON::kv_f32(&jb, "lux", PERSISTENCE::cfg().offsets.lux, 2);
            COMMS::JSON::kv_f32(&jb, "press", PERSISTENCE::cfg().offsets.press, 2);

            return COMMS::JSON::end_and_publish(&jb, topic);
        }

        bool on_offsets_get(const char * /*topic*/, const char * /*payload*/)
        {
            return _publish_all_offsets(MQTT_OFFSETS_STATUS_TOPIC);
        }

        // TODO: Adaptar a nuevos offsets con struct CalibOffsets y PERSISTENCE::cfg::
        bool on_offsets_set(const char * /*topic*/, const char *payload)
        {
            Doc d{};
            if (!parse(d, payload))
            {
                return COMMS::JSON::publish_status_err(MQTT_OFFSETS_STATUS_TOPIC, "bad-json");
            }

            // Mapa clave -> puntero al campo del struct de offsets
            struct Entry
            {
                const char *key;
                float &slot;
            };

            // TODO: Modificar en persistence.h y que el objeto CalibOffsets ya sea la lista esta de floats y keys
            auto &cfg = PERSISTENCE::cfg();
            Entry entries[] = {
                {"temp", cfg.offsets.temp},
                {"hum", cfg.offsets.hum},
                {"lux", cfg.offsets.lux},
                {"press", cfg.offsets.press},
            };

            int updated = 0;
            for (auto &e : entries)
            {
                if (has_key(d, e.key))
                {
                    float v = 0.f;
                    // Usa el helper que tengas: get_f32(...) o get_float(...)
                    if (!get_f32(d, e.key, v))
                    {
                        // Campo presente pero inválido → respondemos con error específico
                        JSON::Builder jb;
                        if (COMMS::JSON::begin_reply(&jb, g_mqtt_out, sizeof(g_mqtt_out), "ERR", "invalid"))
                        {
                            COMMS::JSON::kv_str(&jb, "field", e.key);
                            COMMS::JSON::end_and_publish(&jb, MQTT_OFFSETS_STATUS_TOPIC);
                        }
                        return true;
                    }
                    e.slot = v; // Actualizamos el offset en RAM
                    ++updated;
                }
            }

            if (!updated)
            {
                return COMMS::JSON::publish_status_err(MQTT_OFFSETS_STATUS_TOPIC, "no-fields");
            }

            PERSISTENCE::mark_dirty();
            PERSISTENCE::save_if_dirty();
            return _publish_all_offsets(MQTT_OFFSETS_STATUS_TOPIC);
        }
    }
} // namespace
