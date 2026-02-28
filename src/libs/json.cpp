#include "json.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace JSON {

static inline void init_buf(Builder& jb) {
  if (jb.cap == 0) return;
  jb.buf[0] = '\0';
  jb.len = 0;
}

// Capacidad útil teniendo en cuenta la reserva de cola
static inline size_t headroom(const Builder& jb) {
  if (jb.cap <= jb.len) return 0;
  size_t usable = jb.cap - jb.len;
  if (jb.tail_reserve >= usable) return 0;
  return usable - jb.tail_reserve;
}

static bool append_char(Builder& jb, char c) {
  if (headroom(jb) < 1) { jb.truncated = true; return false; }
  jb.buf[jb.len++] = c;
  jb.buf[jb.len] = '\0';
  return true;
}

static bool vappendf(Builder& jb, const char* fmt, va_list ap) {
  size_t hr = headroom(jb);
  if (hr == 0) { jb.truncated = true; return false; }
  int n = vsnprintf(jb.buf + jb.len, hr, fmt, ap);
  if (n < 0) { jb.truncated = true; return false; }

  if ((size_t)n >= hr) {
    // Se ha truncado por capacidad
    jb.len = jb.cap - jb.tail_reserve - 1; // deja sitio para cierre/fin de cadena
    jb.buf[jb.len] = '\0';
    jb.truncated = true;
    return false;
  }

  jb.len += (size_t)n;
  return true;
}

static bool appendf(Builder& jb, const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  bool ok = vappendf(jb, fmt, ap);
  va_end(ap);
  return ok;
}

static bool maybe_comma(Builder& jb) {
  if (!jb.started) return false;
  if (jb.first) { jb.first = false; return true; }
  return append_char(jb, ',');
}

// Intenta emitir el marcador global de truncado: "_trunc":true
// Idempotente (sólo lo añade una vez).
static bool emit_trunc_marker(Builder& jb) {
  if (jb.trunc_emitted) return true;
  size_t save_len = jb.len;
  bool   save_first = jb.first;

  if (!maybe_comma(jb)) { jb.len = save_len; jb.first = save_first; return false; }
  if (!appendf(jb, "\"_trunc\":true")) {
    // no hubo espacio: revierte
    jb.len = save_len; jb.first = save_first;
    return false;
  }
  jb.trunc_emitted = true;
  return true;
}

static bool push_object(Builder& jb) {
  if (jb.sp >= Builder::MAX_NEST) { jb.truncated = true; return false; }
  // Guardar estado actual
  jb.stack[jb.sp].first_saved = jb.first;
  jb.stack[jb.sp].closing     = '}';   // por si en el futuro añadimos arrays
  jb.sp++;

  // Entramos a un nuevo objeto: empezamos con '{' y 'first = true'
  if (!append_char(jb, '{')) { jb.sp--; return false; }
  jb.first = true;
  return true;
}

static bool pop_container(Builder& jb) {
  if (jb.sp <= 0) return false; // nada que cerrar (error de uso)
  // Cerrar el objeto actual
  if (!append_char(jb, jb.stack[jb.sp - 1].closing)) { jb.truncated = true; return false; }
  // Restaurar estado del nivel anterior
  jb.first = jb.stack[jb.sp - 1].first_saved;
  jb.sp--;
  return true;
}

// Helpers de campo atómico (si falla, revertimos y añadimos marcador si cabe)
struct SavePoint { size_t len; bool first; };
static inline SavePoint start_field(Builder& jb) {
  SavePoint sp{jb.len, jb.first};
  (void)maybe_comma(jb); // si falla, igualmente revertiremos
  return sp;
}
static inline void rollback(Builder& jb, const SavePoint& sp) {
  jb.len = sp.len; jb.first = sp.first;
}

// --- API pública ---

void begin(Builder& jb, char* buffer, size_t capacity) {
  jb.buf = buffer; jb.cap = capacity; jb.len = 0;
  jb.started = true; jb.first = true; jb.truncated = false;
  jb.trunc_emitted = false;
  // Reserva por defecto: 20B ~ coma + "_trunc":true + '}' + '\0'
  jb.tail_reserve = 20;

  init_buf(jb);
  append_char(jb, '{');
}

bool end(Builder& jb) {

    while (jb.sp > 0) {
    if (!pop_container(jb)) break; // si no cabe, forzaremos cierre al final
  }

  // Si hubo truncado pero aún no emitimos la marca, inténtalo ahora
  if (jb.truncated && !jb.trunc_emitted) {
    (void)emit_trunc_marker(jb); // si no cabe, no pasa nada; cerraremos igual
  }

  // Cierra *siempre* con '}'
  if (jb.cap == 0) return false;
  if (jb.len < jb.cap - 1) {
    jb.buf[jb.len++] = '}';
    jb.buf[jb.len] = '\0';
    return true;
  }
  // Forzar cierre last-minute (muy improbable gracias a tail_reserve)
  jb.buf[jb.cap - 2] = '}';
  jb.buf[jb.cap - 1] = '\0';
  return false;
}

bool obj_begin(Builder& jb, const char* key) {
  SavePoint sp = start_field(jb);
  // "key":
  if (!appendf(jb, "\"%s\":", key)) {
    rollback(jb, sp);
    jb.truncated = true; (void)emit_trunc_marker(jb);
    return false;
  }
  // '{' y entrar a nuevo nivel
  if (!push_object(jb)) {
    rollback(jb, sp);
    jb.truncated = true; (void)emit_trunc_marker(jb);
    return false;
  }
  return true;
}

bool obj_end(Builder& jb) {
  if (!pop_container(jb)) { jb.truncated = true; return false; }
  return true;
}

bool add_num(Builder& jb, const char* key, long v) {
  SavePoint sp = start_field(jb);
  if (appendf(jb, "\"%s\":%ld", key, v)) return true;
  rollback(jb, sp);
  jb.truncated = true; (void)emit_trunc_marker(jb);
  return false;
}

bool add_float(Builder& jb, const char* key, float v, int prec) {
  SavePoint sp = start_field(jb);
  if (appendf(jb, "\"%s\":%.*f", key, prec, v)) return true;
  rollback(jb, sp);
  jb.truncated = true; (void)emit_trunc_marker(jb);
  return false;
}

bool add_bool(Builder& jb, const char* key, bool b) {
  SavePoint sp = start_field(jb);
  if (appendf(jb, "\"%s\":%s", key, b ? "true" : "false")) return true;
  rollback(jb, sp);
  jb.truncated = true; (void)emit_trunc_marker(jb);
  return false;
}

static bool append_escaped_str(Builder& jb, const char* s) {
  if (!append_char(jb, '"')) return false;
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    switch (*p) {
      case '\"': if (!appendf(jb, "\\\"")) return false; break;
      case '\\': if (!appendf(jb, "\\\\")) return false; break;
      case '\n': if (!appendf(jb, "\\n"))  return false; break;
      case '\r': if (!appendf(jb, "\\r"))  return false; break;
      case '\t': if (!appendf(jb, "\\t"))  return false; break;
      default:
        if (!append_char(jb, (char)*p)) return false;
    }
  }
  return append_char(jb, '"');
}

bool add_str(Builder& jb, const char* key, const char* s) {
  SavePoint sp = start_field(jb);
  if (appendf(jb, "\"%s\":", key) && append_escaped_str(jb, s ? s : "")) return true;
  rollback(jb, sp);
  jb.truncated = true; (void)emit_trunc_marker(jb);
  return false;
}

bool add_raw(Builder& jb, const char* key, const char* raw_no_quotes) {
  SavePoint sp = start_field(jb);
  if (appendf(jb, "\"%s\":%s", key, raw_no_quotes ? raw_no_quotes : "null")) return true;
  rollback(jb, sp);
  jb.truncated = true; (void)emit_trunc_marker(jb);
  return false;
}

} // namespace JSON
