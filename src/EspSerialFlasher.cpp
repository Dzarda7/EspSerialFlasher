/*
 * SPDX-FileCopyrightText: 2026 Jaroslav Burian
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>

#include "EspSerialFlasher.h"

static ArduinoFlasherPort *get_port(esp_loader_port_t *port)
{
    return container_of(port, ArduinoFlasherPort, port);
}

static void set_pin(int pin, bool active, bool invert)
{
    if (pin >= 0) {
        digitalWrite(pin, (active != invert) ? LOW : HIGH);
    }
}

static esp_loader_error_t arduino_init(esp_loader_port_t *port)
{
    ArduinoFlasherPort *p = get_port(port);
    if (!p->serial) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }
    if (p->reset_pin >= 0) {
        set_pin(p->reset_pin, false, p->reset_invert);
        pinMode(p->reset_pin, OUTPUT);
    }
    if (p->boot_pin >= 0) {
        set_pin(p->boot_pin, false, p->boot_invert);
        pinMode(p->boot_pin, OUTPUT);
    }
    return ESP_LOADER_SUCCESS;
}

static void arduino_enter_bootloader(esp_loader_port_t *port)
{
    ArduinoFlasherPort *p = get_port(port);
    set_pin(p->boot_pin, true, p->boot_invert);
    set_pin(p->reset_pin, true, p->reset_invert);
    delay(p->reset_hold_ms);
    set_pin(p->reset_pin, false, p->reset_invert);
    delay(p->boot_hold_ms);
    set_pin(p->boot_pin, false, p->boot_invert);
}

static void arduino_reset_target(esp_loader_port_t *port)
{
    ArduinoFlasherPort *p = get_port(port);
    set_pin(p->reset_pin, true, p->reset_invert);
    delay(p->reset_hold_ms);
    set_pin(p->reset_pin, false, p->reset_invert);
}

static void arduino_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    get_port(port)->_time_end = millis() + ms;
}

static uint32_t arduino_remaining_time(esp_loader_port_t *port)
{
    int32_t remaining = (int32_t)(get_port(port)->_time_end - millis());
    return remaining > 0 ? (uint32_t)remaining : 0;
}

static void arduino_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    delay(ms);
}

static void arduino_log(esp_loader_port_t *port, esp_loader_log_level_t level, const char *fmt, va_list args)
{
    ArduinoFlasherPort *p = get_port(port);
    if (!p->log) {
        return;
    }
    static const char prefix[] = "?EWID";
    char buf[192];
    vsnprintf(buf, sizeof(buf), fmt, args);
    p->log->print('[');
    p->log->print(prefix[level <= 4 ? level : 0]);
    p->log->print("] esf: ");
    p->log->println(buf);
}

static void arduino_log_hex(esp_loader_port_t *port,
                            esp_loader_log_level_t level,
                            const char *label,
                            const uint8_t *data,
                            size_t size)
{
    (void)level;
    ArduinoFlasherPort *p = get_port(port);
    if (!p->log) {
        return;
    }
    p->log->print(label ? label : "hex");
    p->log->print(':');
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            p->log->println();
        }
        char hex[4];
        snprintf(hex, sizeof(hex), "%02x ", data[i]);
        p->log->print(hex);
    }
    p->log->println();
}

static esp_loader_error_t arduino_change_rate(esp_loader_port_t *port, uint32_t rate)
{
    HardwareSerial *serial = get_port(port)->serial;
    serial->flush();
#if defined(ARDUINO_ARCH_ESP32)
    serial->updateBaudRate(rate);
#else
    serial->end();
    serial->begin(rate);
#endif
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t arduino_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    (void)timeout;
    size_t written = get_port(port)->serial->write(data, size);
    return written == size ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_FAIL;
}

static esp_loader_error_t arduino_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    HardwareSerial *serial = get_port(port)->serial;
    uint32_t start = millis();
    size_t pos = 0;
    while (pos < size) {
        if (millis() - start > timeout) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }
        int c = serial->read();
        if (c >= 0) {
            data[pos++] = (uint8_t)c;
        }
    }
    return ESP_LOADER_SUCCESS;
}

const esp_loader_port_ops_t arduino_port_ops = {
    .init = arduino_init,
    .deinit = nullptr,
    .enter_bootloader = arduino_enter_bootloader,
    .reset_target = arduino_reset_target,
    .start_timer = arduino_start_timer,
    .remaining_time = arduino_remaining_time,
    .delay_ms = arduino_delay_ms,
    .log = arduino_log,
    .log_hex = arduino_log_hex,
    .change_transmission_rate = arduino_change_rate,
    .write = arduino_write,
    .read = arduino_read,
    .spi_set_cs = nullptr,
    .sdio_write = nullptr,
    .sdio_read = nullptr,
    .sdio_card_init = nullptr,
};

const char *flasher_error_string(esp_loader_error_t err)
{
    switch (err) {
    case ESP_LOADER_SUCCESS:
        return "success";
    case ESP_LOADER_ERROR_FAIL:
        return "fail";
    case ESP_LOADER_ERROR_TIMEOUT:
        return "timeout";
    case ESP_LOADER_ERROR_IMAGE_SIZE:
        return "image size";
    case ESP_LOADER_ERROR_INVALID_MD5:
        return "invalid MD5";
    case ESP_LOADER_ERROR_INVALID_PARAM:
        return "invalid param";
    case ESP_LOADER_ERROR_INVALID_TARGET:
        return "invalid target";
    case ESP_LOADER_ERROR_UNSUPPORTED_CHIP:
        return "unsupported chip";
    case ESP_LOADER_ERROR_UNSUPPORTED_FUNC:
        return "unsupported function";
    case ESP_LOADER_ERROR_INVALID_RESPONSE:
        return "invalid response";
    case ESP_LOADER_ERROR_EFUSE_BLOCK_IN_USE:
        return "efuse block in use";
    case ESP_LOADER_ERROR_EFUSE_BURN_FAILED:
        return "efuse burn failed";
    }
    return "unknown";
}
