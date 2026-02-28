#pragma once
#include <stddef.h>
#include "config/config.h"

namespace TLS_CONFIG {
  // Devuelve puntero y longitud del bundle CA
  const char* ca_bundle_data();
  size_t               ca_bundle_size();
} // namespace TLS_CONFIG