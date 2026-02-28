#pragma once
#include <stddef.h>

namespace JSON {

struct Builder {
  char*  buf;        // buffer destino
  size_t cap;        // capacidad total
  size_t len;        // longitud usada (sin contar '\0')
  bool   started;    // se ha llamado begin()
  bool   first;      // próxima pareja necesita coma?
  bool   truncated;  // true si en algún append no cupo todo

  size_t tail_reserve;      // bytes reservados para poder cerrar/etiquetar
  bool   trunc_emitted;     // ya se añadió "_trunc":true al JSON

  static constexpr int MAX_NEST = 4;
  struct Frame { bool first_saved; char closing; };
  Frame  stack[MAX_NEST];
  int    sp = 0; // profundidad actual (0 = raíz)
};

// Inicializa el builder con un buffer externo (no reserva memoria)
void begin(Builder& jb, char* buffer, size_t capacity);

// (Opcional) ajustar la reserva de cola (por defecto ya viene razonable)
inline void set_tail_reserve(Builder& jb, size_t bytes) { jb.tail_reserve = bytes; }

// Pares clave:valor
bool add_num  (Builder& jb, const char* key, long v);
bool add_float(Builder& jb, const char* key, float v, int prec);
bool add_bool (Builder& jb, const char* key, bool b);
bool add_str  (Builder& jb, const char* key, const char* s); // escapa básico

bool obj_begin(Builder& jb, const char* key); // emite "key":{  y entra
bool obj_end  (Builder& jb);                  // emite }       y sale

// Cierra el objeto ("}"). Garantiza JSON válido.
bool end(Builder& jb);

// Acceso al resultado
inline const char* c_str(const Builder& jb) { return jb.buf; }
inline size_t      size (const Builder& jb) { return jb.len; }

// Opción: añadir un fragmento ya formateado (sin comillas alrededor del valor)
bool add_raw(Builder& jb, const char* key, const char* raw_no_quotes);

} // namespace JSON
