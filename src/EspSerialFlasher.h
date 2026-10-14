/*
 * SPDX-FileCopyrightText: 2026 Jaroslav Burian
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// Arduino port for esp-serial-flasher.
//
// Everything else in this directory is copied verbatim from upstream by
// update.sh; the API is the plain C one from esp_loader.h.
//
//   Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
//
//   ArduinoFlasherPort port;
//   port.serial    = &Serial1;
//   port.reset_pin = TARGET_EN;
//   port.boot_pin  = TARGET_IO0;
//
//   esp_loader_t loader;
//   esp_loader_init_serial(&loader, &port.port);
//   esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
//   esp_loader_connect(&loader, &args);

#pragma once

#include <Arduino.h>

#include "esp_loader.h"
#include "esp_loader_io.h"

extern const esp_loader_port_ops_t arduino_port_ops;

struct ArduinoFlasherPort {
    esp_loader_port_t port = { &arduino_port_ops };

    // The UART must already be started with begin() at the baud rate the
    // target ROM expects (115200 by default).
    HardwareSerial *serial = nullptr;

    // -1 = not connected. Pins are driven active-low unless *_invert is set.
    int reset_pin = -1;
    int boot_pin = -1;
    bool reset_invert = false;
    bool boot_invert = false;
    uint32_t reset_hold_ms = 100;
    uint32_t boot_hold_ms = 50;

    // Library log output. Only used when compiled with
    // -DSERIAL_FLASHER_LOG_LEVEL=1..4; otherwise logging is compiled out.
    Print *log = nullptr;

    uint32_t _time_end = 0;
};

const char *flasher_error_string(esp_loader_error_t err);
