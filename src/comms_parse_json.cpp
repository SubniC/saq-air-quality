#include "comms_parse_json.h"
#include <ctype.h>
#include <string.h>
#include "config/config.h"

namespace COMMS
{
  namespace JSON
  {
    namespace PARSER
    {

      // ---------------------------------------------------------------
      // Helpers
      // ---------------------------------------------------------------
      static inline bool key_equals(const JSONString &name, const char *key)
      {
        if (!key)
          return false;
        const size_t klen = strlen(key);
        const size_t nlen = name.size();
        if (klen != nlen)
          return false;
        return (strncmp(name.data(), key, nlen) == 0);
      }

      static inline bool find_value(const Doc &d, const char *key, JSONValue &out)
      {
        if (!d.ok || !d.root.isValid() || !d.root.isObject() || !key)
          return false;

        JSONObjectIterator it(d.root);
        while (it.next())
        {
          const JSONString nm = it.name();
          if (key_equals(nm, key))
          {
            out = it.value();
            return true;
          }
        }
        return false;
      }

      // Devuelve true si todos los chars son hex válidos
      static inline bool is_hex_str(const char *s, size_t n)
      {
        for (size_t i = 0; i < n; ++i)
        {
          const char c = s[i];
          if (!isxdigit((unsigned char)c))
            return false;
        }
        return true;
      }

      // Copia segura: sólo true si cabe completo (+nul)
      static inline bool copy_cstr_exact(char *out, size_t cap, const char *src, size_t n)
      {
        if (!out || cap == 0)
          return false;
        if (!src)
        {
          if (cap)
            out[0] = '\0';
          return false;
        }
        if (cap <= n)
          return false; // no cabe
        memcpy(out, src, n);
        out[n] = '\0';
        return true;
      }

      // ---------------------------------------------------------------
      // API pública
      // ---------------------------------------------------------------
      bool parse(Doc &d, const char *payload)
      {
        d.ok = false;
        d.root = JSONValue(); // invalida por claridad
        if (!payload)
          return false;

        // Copia interna del buffer
        JSONValue tmp = JSONValue::parseCopy(payload);
        if (!tmp.isValid() || !tmp.isObject())
        {
          return false;
        }
        d.root = tmp;
        d.ok = true;
        return true;
      }

      bool has_key(const Doc &d, const char *key)
      {
        JSONValue v;
        return find_value(d, key, v);
      }

      bool get_str(const Doc &d, const char *key, char *out, size_t cap)
      {
        if (!out || cap == 0)
          return false;

        JSONValue v;
        if (!find_value(d, key, v) || !v.isString())
          return false;

        const JSONString s = v.toString();
        return copy_cstr_exact(out, cap, s.data(), s.size());
      }

      bool get_bool(const Doc &d, const char *key, bool &out)
      {
        JSONValue v;
        if (!find_value(d, key, v))
          return false;

        if (v.isBool())
        {
          out = v.toBool();
          return true;
        }

#ifdef JSON_PARSER_COERCE_TYPES
        if (v.isString())
        {
          const JSONString s = v.toString();
          // Comparar case-insensitive con "true"/"false"
          bool all_lower = true;
          for (size_t i = 0; i < s.size(); ++i)
          {
            if (isupper((unsigned char)s.data()[i]))
            {
              all_lower = false;
              break;
            }
          }
          // Creamos copia a minúsculas si hace falta
          String lower;
          if (!all_lower)
          {
            lower.reserve(s.size() + 1);
            for (size_t i = 0; i < s.size(); ++i)
              lower += (char)tolower((unsigned char)s.data()[i]);
          }
          const char *p = all_lower ? s.data() : lower.c_str();
          if (strcmp(p, "true") == 0)
          {
            out = true;
            return true;
          }
          if (strcmp(p, "false") == 0)
          {
            out = false;
            return true;
          }
        }

        if (v.isNumber())
        {
          out = (v.toInt() != 0);
          return true;
        }
#endif
        return false;
      }

      bool get_i32(const Doc &d, const char *key, int32_t &out)
      {
        JSONValue v;
        if (!find_value(d, key, v))
          return false;

        if (v.isNumber())
        {
          out = (int32_t)v.toInt();
          return true;
        }
#ifdef JSON_PARSER_COERCE_TYPES
        if (v.isString())
        {
          const JSONString s = v.toString();
          char *end = nullptr;
          long val = strtol(s.data(), &end, 0);
          if (end && *end == '\0')
          {
            out = (int32_t)val;
            return true;
          }
        }
#endif
        return false;
      }

      bool get_u32(const Doc &d, const char *key, uint32_t &out)
      {
        JSONValue v;
        if (!find_value(d, key, v))
          return false;

        if (v.isNumber())
        {
          long long tmp = (long long)v.toInt();
          if (tmp < 0)
            return false;
          out = (uint32_t)tmp;
          return true;
        }
#ifdef JSON_PARSER_COERCE_TYPES
        if (v.isString())
        {
          const JSONString s = v.toString();
          char *end = nullptr;
          unsigned long val = strtoul(s.data(), &end, 0);
          if (end && *end == '\0')
          {
            out = (uint32_t)val;
            return true;
          }
        }
#endif
        return false;
      }

      bool get_f32(const Doc &d, const char *key, float &out)
      {
        JSONValue v;
        if (!find_value(d, key, v))
          return false;

        if (v.isNumber())
        {
          out = (float)v.toDouble();
          return true;
        }

#ifdef JSON_PARSER_COERCE_TYPES
        if (v.isString())
        {
          const JSONString s = v.toString();
          char *end = nullptr;
          double val = strtod(s.data(), &end);
          if (end && *end == '\0')
          {
            out = (float)val;
            return true;
          }
        }
#endif

        return false;
      }

      bool get_hex_rgb(const Doc &d, const char *key, uint32_t &rgb)
      {
        JSONValue v;
        if (!find_value(d, key, v) || !v.isString())
          return false;

        const JSONString s = v.toString();
        const char *p = s.data();
        size_t n = s.size();

        if (n == 0)
          return false;
        if (p[0] == '#')
        {
          p++;
          n--;
        } // admite "#RRGGBB"

        if (n != 6)
          return false; // sólo 6 dígitos hex
        if (!is_hex_str(p, n))
          return false;

        char buf[7]; // "RRGGBB"
        memcpy(buf, p, 6);
        buf[6] = '\0';

        char *end = nullptr;
        unsigned long val = strtoul(buf, &end, 16);
        if (!end || *end != '\0')
          return false;
        if (val > 0xFFFFFFul)
          return false;

        rgb = (uint32_t)val;
        return true;
      }

    } // namespace PARSER
  } // namespace JSON
} // namespace COMMS
