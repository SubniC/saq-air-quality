#pragma once

#include "application.h"
#include "config/config.h"
#include "utils/debug.h"
#include "libs/json.h"      
#include "comms.h"   
#include <stdint.h>


namespace COMMS {
namespace JSON {

using ::JSON::Builder;

// ---------- Helpers de “sobre” ----------
bool publish_status_ok   (const char* topic, const char* msg = nullptr);
bool publish_status_err  (const char* topic, const char* msg = nullptr);

// ---------- Flujo “manual” para respuestas con varios campos ----------
bool begin_reply(Builder* jb, char* out_buf, size_t out_cap,
                 const char* status, const char* msg = nullptr);

bool kv_str (Builder* jb, const char* k, const char* v);
bool kv_i32 (Builder* jb, const char* k, int32_t v);
bool kv_u32 (Builder* jb, const char* k, uint32_t v);
bool kv_f32 (Builder* jb, const char* k, float v, int decimals = 2);

bool end_and_publish(Builder* jb, const char* topic);

} // namespace JSON
} // namespace COMMS
