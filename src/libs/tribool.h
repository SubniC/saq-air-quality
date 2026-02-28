#pragma once
#include <stdint.h>

namespace jb {
namespace logic {

// Tipo "tag" para el tercer estado (evita conversiones desde int/bool)
struct unknown_keyword_t { };
constexpr unknown_keyword_t unknown{};

class tribool {
  enum value_t : uint8_t { false_value = 0, true_value = 1, unknown_value = 2 } value;

public:
  // Constructores
  constexpr tribool() : value(false_value) {}
  constexpr tribool(bool v) : value(v ? true_value : false_value) {}
  constexpr tribool(unknown_keyword_t) : value(unknown_value) {}

  // Consultas
  constexpr bool is_true() const     { return value == true_value; }
  constexpr bool is_false() const    { return value == false_value; }
  constexpr bool is_unknown() const  { return value == unknown_value; }

  // Conversión a bool: true solo cuando es verdaderamente true
  explicit constexpr operator bool() const { return value == true_value; }

  // NOT
  constexpr tribool operator!() const {
    return value == true_value   ? tribool(false)
         : value == false_value  ? tribool(true)
                                 : tribool(unknown);
  }

  // AND
  friend constexpr tribool operator&&(tribool lhs, tribool rhs) {
    return (lhs.value == false_value || rhs.value == false_value) ? tribool(false)
         : (lhs.value == true_value  && rhs.value == true_value ) ? tribool(true)
                                                                  : tribool(unknown);
  }
  friend constexpr tribool operator&&(tribool lhs, bool rhs) { return rhs ? lhs : tribool(false); }
  friend constexpr tribool operator&&(bool lhs, tribool rhs) { return lhs ? rhs : tribool(false); }
  friend constexpr tribool operator&&(unknown_keyword_t, tribool lhs) { return !lhs ? tribool(false) : tribool(unknown); }
  friend constexpr tribool operator&&(tribool lhs, unknown_keyword_t) { return !lhs ? tribool(false) : tribool(unknown); }

  // OR
  friend constexpr tribool operator||(tribool lhs, tribool rhs) {
    return (lhs.value == true_value || rhs.value == true_value) ? tribool(true)
         : (lhs.value == false_value && rhs.value == false_value) ? tribool(false)
                                                                  : tribool(unknown);
  }
  friend constexpr tribool operator||(tribool lhs, bool rhs) { return rhs ? tribool(true) : lhs; }
  friend constexpr tribool operator||(bool lhs, tribool rhs) { return lhs ? tribool(true) : rhs; }
  friend constexpr tribool operator||(unknown_keyword_t, tribool rhs) { return rhs ? tribool(true) : tribool(unknown); }
  friend constexpr tribool operator||(tribool lhs, unknown_keyword_t) { return lhs ? tribool(true) : tribool(unknown); }
};

} // namespace logic
} // namespace jb