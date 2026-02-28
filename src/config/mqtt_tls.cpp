#include "mqtt_tls.h"

static const char _ca_pem[] = "-----BEGIN CERTIFICATE-----\r\n"                                       \
        "-----END CERTIFICATE----- ";

namespace TLS_CONFIG {
  const char* ca_bundle_data() {
    return reinterpret_cast<const char*>(_ca_pem);
  }

  size_t ca_bundle_size() {
    return sizeof(_ca_pem);
  }
} // namespace TLS