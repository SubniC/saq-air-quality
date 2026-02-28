#include "comms.h"
#include "comms_router.h"
#include "comms_parse_json.h"
#include "comms_json.h"
#include "ha_discovery.h"
#include "utils/debug.h"

namespace COMMS {
namespace HANDLERS {

bool on_ha_command(const char* topic, const char* payload) {
  using namespace COMMS::JSON::PARSER;
  using namespace HA;

  Doc d{};
  if (!parse(d, payload)) {
    return COMMS::JSON::publish_status_err(MQTT_HA_STATUS_TOPIC, "bad-json");
  }

  char action[16] = {0};
  if (!get_str(d, "action", action, sizeof(action))) {
    return COMMS::JSON::publish_status_err(MQTT_HA_STATUS_TOPIC, "missing-action");
  }

  if (strcmp(action, "announce") == 0) {
    const bool ok = HA::publish_discovery_all();
    return ok
      ? COMMS::JSON::publish_status_ok(MQTT_HA_STATUS_TOPIC, "announce")
      : COMMS::JSON::publish_status_err(MQTT_HA_STATUS_TOPIC, "announce-failed");
  }

  if (strcmp(action, "cleanup") == 0) {
    const bool ok = HA::clear_discovery_all();
    return ok
      ? COMMS::JSON::publish_status_ok(MQTT_HA_STATUS_TOPIC, "cleanup")
      : COMMS::JSON::publish_status_err(MQTT_HA_STATUS_TOPIC, "cleanup-failed");
  }

  // accion desconocida
  return COMMS::JSON::publish_status_err(MQTT_HA_STATUS_TOPIC, "unknown-action");
}

}} // namespaces
