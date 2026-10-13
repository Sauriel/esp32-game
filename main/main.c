// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "sdkconfig.h"
#include "app/BootMode.h"
#include "arduino_bootstrap.h"
#include <esp_bt.h>
#include <esp_log.h>

#if CONFIG_AUTOSTART_ARDUINO
#error "Moduswahl erfordert CONFIG_AUTOSTART_ARDUINO=n"
#endif

#include <stddef.h>

// BTstack related
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>

// Bluepad32 related
#include <arduino_platform.h>
#include <uni.h>

//
// Autostart
//
#if CONFIG_AUTOSTART_ARDUINO
void initBluepad32() {
#else
int app_main(void) {
#endif  // !CONFIG_AUTOSTART_ARDUINO
    boot_mode_select();
    app_log_heap("Bootentscheidung vor Bluetooth");
    if (!boot_mode_nvs_ready()) {
        // Arduino und Bluepad32 haben ansonsten eine automatische NVS-Loeschung.
        ESP_LOGE("BootMode", "%s; Start gestoppt, NVS bleibt erhalten", boot_mode_error());
        return 0;
    }
    if (boot_mode_get() == BOOT_MODE_TRANSFER) {
        esp_err_t result = ESP_ERR_INVALID_STATE;
        if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
            result = esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
        }
        if (result != ESP_OK) {
            ESP_LOGW("BootMode", "BT-Speicherfreigabe: %s", esp_err_to_name(result));
        }
        app_log_heap("Transfer nach BT-Controller-Freigabe");
        arduino_bootstrap();
        return 0;
    }
    // hci_dump_open(NULL, HCI_DUMP_STDOUT);

// Don't use BTstack buffered UART. It conflicts with the console.
#ifndef CONFIG_ESP_CONSOLE_UART_NONE
#ifndef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
    btstack_stdio_init();
#endif  // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
#endif  // CONFIG_ESP_CONSOLE_UART_NONE

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // Must be called before uni_init()
    uni_platform_set_custom(get_arduino_platform());

    // Init Bluepad32.
    uni_init(0 /* argc */, NULL /* argv */);

    // Does not return.
    btstack_run_loop_execute();
#if !CONFIG_AUTOSTART_ARDUINO
    return 0;
#endif  // !CONFIG_AUTOSTART_ARDUINO
}
