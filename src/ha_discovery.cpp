#include "ha_discovery.h"
#include "comms.h"
#include "libs/json.h"
#include "utils/debug.h"

extern char g_mqtt_topic[MQTT_TOPIC_BUFFER];
extern char g_mqtt_out[MQTT_OUT_BUFFER];

namespace
{

      // Helpers de nombres/ids
      const char *client_id() { return MQTT_CLIENT_ID; } // ya lo usas en connect
      const char *lwt_topic() { return MQTT_LWT_TOPIC; }
      const char *lwt_online() { return MQTT_LWT_MESSAGE_ON; }
      const char *lwt_offline() { return MQTT_LWT_MESSAGE_OFF; }

      // Construye: homeassistant/sensor/<node>/<obj>/config
      void build_config_topic(char *out, size_t cap, const char *component, const char *object_id)
      {
            snprintf(out, cap, "%s/%s/%s/%s/config", MQTT_HA_DISCOVERY_PREFIX, component, client_id(), object_id);
      }

      // Publica un MQTT Discovery de tipo sensor, leyendo de un JSON en state_topic + value_template
      bool publish_sensor_config_jsonpath(
          const char *object_id,      // p.ej. "temperature"
          const char *name,           // Nombre visible
          const char *state_topic,    // p.ej. MQTT_SENSORS_TOPIC
          const char *value_template, // p.ej. "{{ value_json.temp }}"
          const char *unit,           // "°C", "%", "hPa", "ppm", "µg/m³", "lx", ...
          const char *device_class,   // null si no seguro
          const char *icon,           // null si no usas
          const char *state_class     // "measurement", "total_increasing", ...
      )
      {
            // topic config
            // extern char g_mqtt_topic[MQTT_TOPIC_BUFFER];
            build_config_topic(g_mqtt_topic, sizeof(g_mqtt_topic), "sensor", object_id);

            // payload discovery
            // extern char g_mqtt_out[MQTT_OUT_BUFFER];
            JSON::Builder jb;

            JSON::begin(jb, g_mqtt_out, sizeof(g_mqtt_out));
            JSON::add_str(jb, "name", name);
            char uid[64];
            snprintf(uid, sizeof(uid), "%s_%s", client_id(), object_id);
            JSON::add_str(jb, "unique_id", uid);
            JSON::add_str(jb, "state_topic", state_topic);
            JSON::add_str(jb, "value_template", value_template);

            if (unit)
                  JSON::add_str(jb, "unit_of_measurement", unit);
            if (device_class)
                  JSON::add_str(jb, "device_class", device_class);
            if (state_class)
                  JSON::add_str(jb, "state_class", state_class);
            if (icon)
                  JSON::add_str(jb, "icon", icon);

            // availability (LWT)
            JSON::add_str(jb, "availability_topic", lwt_topic());
            JSON::add_str(jb, "payload_available", lwt_online());
            JSON::add_str(jb, "payload_not_available", lwt_offline());

            // bloque "device" para agrupar entidades en HA
            JSON::obj_begin(jb, "device");
            JSON::add_raw(jb, "identifiers", "[\"" MQTT_CLIENT_ID "\"]");
            JSON::add_str(jb, "name", MQTT_CLIENT_ID);
            JSON::add_str(jb, "manufacturer", "SAQ");
            JSON::add_str(jb, "model", "AirQuality");
            JSON::add_str(jb, "sw_version", DEVICE_SOFTWARE_VERSION " (" DEVICE_BUILD_TIMESTAMP ")");
            JSON::obj_end(jb);

            JSON::end(jb);

            if (jb.truncated)
            {
                  LOG_ERROR("HA trunc '%s' %u/%u",
                            object_id, (unsigned)JSON::size(jb), (unsigned)sizeof(g_mqtt_out));
                  return false;
            }

            return COMMS::mqtt_publish(g_mqtt_topic, JSON::c_str(jb), true);
      }

      // Limpia un config (retained vacío)
      bool clear_config(const char *component, const char *object_id)
      {
            build_config_topic(g_mqtt_topic, sizeof(g_mqtt_topic), component, object_id);
            return COMMS::mqtt_publish(g_mqtt_topic, "", true); // payload vacío (retained) borra discovery
      }

} // anon

namespace HA
{

      bool publish_discovery_all()
      {
            bool ok = true;

            // ---- Métricas que ya publicas en MQTT_SENSORS_TOPIC como JSON ----
            // T, H, P, Lux, CO2, TVOC
            ok &= publish_sensor_config_jsonpath("temperature", "Temperature",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.temp_c }}",
                                                 "°C", "temperature", nullptr, "measurement");

            

#ifdef HAS_HUMIDITY_DATA
            ok &= publish_sensor_config_jsonpath("humidity", "Humidity",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.hum_percent }}",
                                                 "%", "humidity", nullptr, "measurement");
            
#endif

            ok &= publish_sensor_config_jsonpath("pressure", "Pressure",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.press_mb }}",
                                                 "mbar", "atmospheric_pressure", nullptr, "measurement");
            

#ifdef HAS_LUX_DATA                                                 
            ok &= publish_sensor_config_jsonpath("illuminance", "Illuminance",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.lux }}",
                                                 "lx", "illuminance", nullptr, "measurement");
            
#endif
            ok &= publish_sensor_config_jsonpath("co2", "Carbon Dioxide",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.co2_ppm }}",
                                                 "ppm", "carbon_dioxide", nullptr, "measurement");
            

            ok &= publish_sensor_config_jsonpath("tvoc", "TVOC",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.tvoc_ppm }}",
                                                 "ppb", "volatile_organic_compounds_parts", nullptr, "measurement");
            

#ifdef HAS_PARTICLE_DATA
            ok &= publish_sensor_config_jsonpath("pm1", "PM1",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.pm1_ugm3 }}",
                                                 "µg/m³", "pm1", nullptr, "measurement");

            ok &= publish_sensor_config_jsonpath("pm25", "PM2.5",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.pm25_ugm3 }}",
                                                 "µg/m³", "pm25", nullptr, "measurement");

            ok &= publish_sensor_config_jsonpath("pm10", "PM10",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.pm10_ugm3 }}",
                                                 "µg/m³", "pm10", nullptr, "measurement");

	#ifdef PARTICLE_SENSOR_MODEL_PMS5003ST
            ok &= publish_sensor_config_jsonpath("hcho", "Formaldehyde",
                                                 MQTT_SENSORS_TOPIC, "{{ value_json.hcho }}",
                                                 "µg/m³", nullptr, nullptr, "measurement");
	#endif
#endif

#ifdef ENABLE_AQI
            ok &= publish_sensor_config_jsonpath("aqi25", "AQI2.5",
                                                 MQTT_AQI_BASE_TOPIC "pm25/minute", "{{ value_json.aqi }}",
                                                 nullptr, "aqi", nullptr, "measurement");

            ok &= publish_sensor_config_jsonpath("aqi10", "AQI10",
                                                 MQTT_AQI_BASE_TOPIC "pm10/minute", "{{ value_json.aqi }}",
                                                 nullptr, "aqi", nullptr, "measurement");
#endif
            // ---- Ruido (publicación adaptativa: IIR rápido ~2s + lento ~30s) ----
            ok &= publish_sensor_config_jsonpath("noise_dbfs", "Noise (dBFS)",
                                                 MQTT_NOISE_TOPIC, "{{ value_json.dbfs }}",
                                                 "dB", "sound_pressure", "mdi:volume-high", "measurement");

            ok &= publish_sensor_config_jsonpath("noise_ambient", "Noise Ambient (dBFS)",
                                                 MQTT_NOISE_TOPIC, "{{ value_json.dbfs_slow }}",
                                                 "dB", "sound_pressure", "mdi:volume-low", "measurement");

            return ok;
      }

      bool clear_discovery_all()
      {
            static const char* const ids[] = {
                  "temperature", "humidity", "pressure", "illuminance",
                  "co2", "tvoc", "pm1", "pm25", "pm10", "hcho",
                  "aqi25", "aqi10", "noise_dbfs", "noise_ambient"
            };
            bool ok = true;
            for (size_t i = 0; i < sizeof(ids)/sizeof(ids[0]); ++i)
                  ok &= clear_config("sensor", ids[i]);
            return ok;
      }

} // namespace HA