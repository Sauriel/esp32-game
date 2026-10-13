#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { BOOT_MODE_GAME = 0, BOOT_MODE_TRANSFER = 1 } boot_mode_t;
void boot_mode_select(void); // Genau einmal, vor BT/Arduino.
boot_mode_t boot_mode_get(void);
bool boot_mode_nvs_ready(void);
const char* boot_mode_error(void);
bool boot_mode_save(boot_mode_t target);
void app_log_heap(const char* phase);
#ifdef __cplusplus
}
#endif
