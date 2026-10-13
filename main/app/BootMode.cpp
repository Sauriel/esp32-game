#include "BootMode.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
namespace {
constexpr uint32_t ticketBase = 0x4d4f4400; // Versioniertes MOD-Ticket.
boot_mode_t selected = BOOT_MODE_GAME;
bool initialized = false, nvsReady = false;
const char* error = nullptr;
bool fail(esp_err_t result, const char* message) {
    if (result == ESP_OK) return false;
    error = message;
    ESP_LOGE("BootMode", "%s: %s", message, esp_err_to_name(result));
    return true;
}
}
extern "C" void boot_mode_select() {
    if (initialized) return;
    initialized = true;
    if (fail(nvs_flash_init(), "NVS initialisieren fehlgeschlagen")) return;
    nvsReady = true;
    nvs_handle_t handle;
    if (fail(nvs_open("boot_mode", NVS_READWRITE, &handle), "NVS oeffnen fehlgeschlagen")) return;
    uint32_t ticket = 0;
    const esp_err_t read = nvs_get_u32(handle, "ticket", &ticket);
    if (read != ESP_ERR_NVS_NOT_FOUND) {
        // Auch falschen Typ/ungueltigen Inhalt entfernen. Ohne bestaetigte
        // Loeschung bleibt Game aktiv; keine wiederkehrenden Transfer-Boots.
        const esp_err_t erased = nvs_erase_key(handle, "ticket");
        const esp_err_t committed = erased == ESP_OK ? nvs_commit(handle) : erased;
        if (!fail(committed, "Modusticket loeschen fehlgeschlagen")) {
            if (read == ESP_OK && esp_reset_reason() == ESP_RST_SW &&
                (ticket == ticketBase || ticket == (ticketBase | 1))) {
                selected = static_cast<boot_mode_t>(ticket & 1);
            } else if (read != ESP_OK) {
                fail(read, "Modusticket lesen fehlgeschlagen");
            }
        }
    }
    nvs_close(handle);
}
extern "C" boot_mode_t boot_mode_get() { return selected; }
extern "C" bool boot_mode_nvs_ready() { return nvsReady; }
extern "C" const char* boot_mode_error() { return error; }
extern "C" bool boot_mode_save(boot_mode_t target) {
    if (target != BOOT_MODE_GAME && target != BOOT_MODE_TRANSFER) {
        error = "Ungueltiger Zielmodus"; return false;
    }
    if (!nvsReady) { error = "NVS nicht verfuegbar"; return false; }
    nvs_handle_t handle;
    if (fail(nvs_open("boot_mode", NVS_READWRITE, &handle), "NVS oeffnen fehlgeschlagen")) return false;
    esp_err_t result = nvs_set_u32(handle, "ticket", ticketBase | target);
    if (result == ESP_OK) result = nvs_commit(handle);
    const bool success = !fail(result, "Modusticket speichern fehlgeschlagen");
    nvs_close(handle);
    return success;
}
extern "C" void app_log_heap(const char* phase) {
    ESP_LOGI("Heap", "%s: internal free=%u largest=%u minimum=%u; heap free=%u minimum=%u", phase,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
}
