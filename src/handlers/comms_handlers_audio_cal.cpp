#include "comms.h"
#include "comms_json.h"
#include "comms_parse_json.h"
#include "persistence.h"
#include "sound.h"
#include "utils/debug.h"
#include "config/config.h"

namespace COMMS
{
    namespace HANDLERS
    {
        using COMMS::JSON::PARSER::Doc;
        using COMMS::JSON::PARSER::get_f32;
        using COMMS::JSON::PARSER::get_str;
        using COMMS::JSON::PARSER::has_key;
        using COMMS::JSON::PARSER::parse;

        // Callback invocado cuando la medición automática termina
        static void on_measurement_done(float noise_floor_db)
        {
            // Guardar en EEPROM
            PERSISTENCE::cfg().audio.mic_noise_floor = noise_floor_db;
            PERSISTENCE::mark_dirty();
            PERSISTENCE::save_if_dirty(0); // forzar guardado inmediato

            // Publicar resultado
            JSON::Builder jb;
            if (COMMS::JSON::begin_reply(&jb, g_mqtt_out, sizeof(g_mqtt_out), "OK", "measured"))
            {
                COMMS::JSON::kv_f32(&jb, "noise_floor", noise_floor_db, 1);
                COMMS::JSON::end_and_publish(&jb, MQTT_AUDIO_CAL_STATUS_TOPIC);
            }
        }

        // /cmd/AUDIO_CAL handler
        // Payloads:
        //   {"action":"measure"}               - Inicia medición de noise floor (5s silencio)
        //   {"action":"measure","seconds":10}   - Medición con duración custom
        //   {"action":"set","noise_floor":-45}  - Fija el noise floor manualmente
        //   {"action":"get"}                    - Devuelve el valor actual
        bool on_audio_cal(const char * /*topic*/, const char *payload)
        {
            Doc d{};
            if (!parse(d, payload))
            {
                return COMMS::JSON::publish_status_err(MQTT_AUDIO_CAL_STATUS_TOPIC, "bad-json");
            }

            char action[16] = {0};
            if (!get_str(d, "action", action, sizeof(action)))
            {
                return COMMS::JSON::publish_status_err(MQTT_AUDIO_CAL_STATUS_TOPIC, "missing-action");
            }

            // --- GET: devuelve noise floor actual ---
            if (strcmp(action, "get") == 0)
            {
                float stored = PERSISTENCE::cfg().audio.mic_noise_floor;
                JSON::Builder jb;
                if (!COMMS::JSON::begin_reply(&jb, g_mqtt_out, sizeof(g_mqtt_out), "OK", "audio_cal"))
                    return false;
                COMMS::JSON::kv_f32(&jb, "noise_floor", stored, 1);
                COMMS::JSON::kv_str(&jb, "calibrated", (stored < -10.0f) ? "yes" : "no");
                return COMMS::JSON::end_and_publish(&jb, MQTT_AUDIO_CAL_STATUS_TOPIC);
            }

            // --- SET: fija noise floor manualmente ---
            if (strcmp(action, "set") == 0)
            {
                float nf = 0.0f;
                if (!get_f32(d, "noise_floor", nf))
                {
                    return COMMS::JSON::publish_status_err(MQTT_AUDIO_CAL_STATUS_TOPIC, "missing-noise_floor");
                }
                if (nf > -5.0f || nf < -90.0f)
                {
                    return COMMS::JSON::publish_status_err(MQTT_AUDIO_CAL_STATUS_TOPIC, "noise_floor-out-of-range");
                }

                PERSISTENCE::cfg().audio.mic_noise_floor = nf;
                PERSISTENCE::mark_dirty();
                PERSISTENCE::save_if_dirty(0);

                if (sound_meter) sound_meter->applyNoiseFloor(nf);

                JSON::Builder jb;
                if (!COMMS::JSON::begin_reply(&jb, g_mqtt_out, sizeof(g_mqtt_out), "OK", "set"))
                    return false;
                COMMS::JSON::kv_f32(&jb, "noise_floor", nf, 1);
                return COMMS::JSON::end_and_publish(&jb, MQTT_AUDIO_CAL_STATUS_TOPIC);
            }

            // --- MEASURE: inicia medición automática ---
            if (strcmp(action, "measure") == 0)
            {
                if (!sound_meter)
                {
                    return COMMS::JSON::publish_status_err(MQTT_AUDIO_CAL_STATUS_TOPIC, "no-sound-meter");
                }
                if (sound_meter->isNoiseFloorMeasuring())
                {
                    return COMMS::JSON::publish_status_err(MQTT_AUDIO_CAL_STATUS_TOPIC, "already-measuring");
                }

                // Duración: default 5s, rango [2..30]
                uint32_t secs = 5;
                COMMS::JSON::PARSER::get_u32(d, "seconds", secs);
                if (secs < 2) secs = 2;
                if (secs > 30) secs = 30;

                sound_meter->setCalibrationCallback(on_measurement_done);
                sound_meter->startNoiseFloorMeasurement((uint16_t)secs);

                JSON::Builder jb;
                if (!COMMS::JSON::begin_reply(&jb, g_mqtt_out, sizeof(g_mqtt_out), "OK", "measuring"))
                    return false;
                COMMS::JSON::kv_u32(&jb, "seconds", secs);
                return COMMS::JSON::end_and_publish(&jb, MQTT_AUDIO_CAL_STATUS_TOPIC);
            }

            return COMMS::JSON::publish_status_err(MQTT_AUDIO_CAL_STATUS_TOPIC, "unknown-action");
        }

    } // namespace HANDLERS
} // namespace COMMS
