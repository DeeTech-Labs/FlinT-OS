#include "display.h"
#include "esp_log.h"

void displayBegin() {}
void displayScene(flint::Core &, const FlintRender::Scene &) {}
void displayRecovery(const char *message) {
    ESP_LOGE("recovery", "%s", message ? message : "Recovery requested");
}
void displayDiagnostics(flint::Values &values) {
    values["display_available"] = "false";
}
