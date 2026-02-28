#pragma once
#include "application.h" // Trae JSONValue, JSONObjectIterator, JSONString

namespace COMMS
{
  namespace JSON
  {
    namespace PARSER
    {

      // Documento parseado (raíz + flag)
      struct Doc
      {
        JSONValue root;
        bool ok;
      };

      // Tokeniza el payload. Devuelve true si hay un objeto raíz válido.
      bool parse(Doc &d, const char *payload);

      // Helpers de búsqueda/lectura (json plano: claves en root)
      bool has_key(const Doc &d, const char *key);

      // Lee string en 'out' (capacidad cap). Devuelve true si existe y cabe.
      bool get_str(const Doc &d, const char *key, char *out, size_t cap);

      // Lee bool: admite true/false (literales) o "true"/"false" (como string)
      bool get_bool(const Doc &d, const char *key, bool &out);

      // Lee enteros (base auto con strtol/strtoul)
      bool get_i32(const Doc &d, const char *key, int32_t &out);
      bool get_u32(const Doc &d, const char *key, uint32_t &out);

      // Lee float (número JSON o string numérico si activas JSON_PARSER_COERCE_TYPES)
     bool get_f32(const Doc& d, const char* key, float& out);

      // Lee color "RRGGBB" o "#RRGGBB" -> 0xRRGGBB
      bool get_hex_rgb(const Doc &d, const char *key, uint32_t &rgb);

      // --- Azúcar con defaults/clamps ---

      inline uint16_t get_u16_or(const Doc &d, const char *k, uint16_t def,
                                 uint16_t lo, uint16_t hi)
      {
        uint32_t v;
        if (!get_u32(d, k, v))
          return def;
        if (v < lo)
          return lo;
        if (v > hi)
          return hi;
        return (uint16_t)v;
      }

      inline uint32_t get_u32_or(const Doc &d, const char *k, uint32_t def,
                                 uint32_t lo, uint32_t hi)
      {
        uint32_t v;
        if (!get_u32(d, k, v))
          return def;
        if (v < lo)
          return lo;
        if (v > hi)
          return hi;
        return v;
      }

            // --- Azúcar con defaults/clamps ---
      inline float get_f32_or(const Doc& d, const char* k, float def,
                              float lo, float hi)
      {
        float v;
        if (!get_f32(d, k, v)) return def;
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
      }

      inline bool get_hex_rgb_or(const Doc &d, const char *k, uint32_t def, uint32_t &out)
      {
        uint32_t tmp;
        if (!get_hex_rgb(d, k, tmp))
        {
          out = def;
          return false;
        }
        out = tmp;
        return true;
      }



    } // namespace PARSER
  } // namespace JSON
} // namespace COMMS
