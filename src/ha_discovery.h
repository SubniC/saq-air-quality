#pragma once
#include "config/config.h"

namespace HA {

// Publica todos los config (retained)
bool publish_discovery_all();

// Opcional: limpiar discovery (retained vacío)
bool clear_discovery_all();

} // namespace HA