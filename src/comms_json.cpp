#include "comms_json.h"

namespace COMMS {
namespace JSON {

// Traemos las funciones C-like del builder a este TU

static inline void _begin(Builder* jb, char* out, size_t cap) {
  // Tu API de builder. Ajusta a tus nombres si difieren.
  ::JSON::begin(*jb,out, cap);
}

static inline void _stamp_common(Builder* jb, const char* status, const char* msg) {
  ::JSON::add_str(*jb, "status", status ? status : "OK");
  if (msg && *msg) ::JSON::add_str(*jb, "msg", msg);
  ::JSON::add_num(*jb, "time", (int32_t)Time.local());
}

static inline bool _end_publish(Builder* jb, const char* topic) {
  ::JSON::end(*jb);
  const char* payload = ::JSON::c_str(*jb);          // o json_get_cstr(jb)
  if (!payload) return false;
  return COMMS::mqtt_publish(topic, payload);
}

// ---------- API pública ----------
bool publish_status_ok(const char* topic, const char* msg) {
  ::JSON::Builder jb;
  _begin(&jb, g_mqtt_out, sizeof(g_mqtt_out));
  _stamp_common(&jb, "OK", msg);
  return _end_publish(&jb, topic);
}

bool publish_status_err(const char* topic, const char* msg) {
  ::JSON::Builder jb;
  _begin(&jb, g_mqtt_out, sizeof(g_mqtt_out));
  _stamp_common(&jb, "ERR", msg);
  return _end_publish(&jb, topic);
}

// Flujo manual
bool begin_reply(Builder* jb, char* out_buf, size_t out_cap,
                 const char* status, const char* msg) {
  _begin(jb, out_buf, out_cap);
  _stamp_common(jb, status, msg);
  return true;
}

bool kv_str(Builder* jb, const char* k, const char* v)  {
  if (!jb) return false;
  return ::JSON::add_str(*jb, k, v);
}
bool kv_i32(Builder* jb, const char* k, int32_t v)      {
  if (!jb) return false;
  return ::JSON::add_num(*jb, k, v);
}
bool kv_u32(Builder* jb, const char* k, uint32_t v)     {
  if (!jb) return false;
  return ::JSON::add_num(*jb, k, (long)v);
}
bool kv_f32(Builder* jb, const char* k, float v, int d) {
  if (!jb) return false;
  return ::JSON::add_float(*jb, k, v, d);
}

bool end_and_publish(Builder* jb, const char* topic) {
  return _end_publish(jb, topic);
}

} // namespace JSON
} // namespace COMMS
