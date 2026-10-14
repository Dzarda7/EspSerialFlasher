# EspSerialFlasher

Arduino packaging of Espressif's [esp-serial-flasher](https://github.com/espressif/esp-serial-flasher):
flash ESP32 / ESP32-S/C/H/P / ESP8266 targets from another microcontroller over UART.

- `src/EspSerialFlasher.{h,cpp}` – Arduino port (`HardwareSerial` + `digitalWrite` + `millis`). The only code of our own.
- everything else in `src/` – upstream core, copied verbatim by `update.sh`. Do not edit.

## Usage

The API is upstream's plain C one (`esp_loader.h`); see `examples/FlashFromFile`.

```cpp
Serial1.begin(115200, SERIAL_8N1, RX, TX);

ArduinoFlasherPort port;
port.serial    = &Serial1;
port.reset_pin = TARGET_EN;
port.boot_pin  = TARGET_IO0;

esp_loader_t loader;
esp_loader_init_serial(&loader, &port.port);
esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
esp_loader_connect_with_stub(&loader, &args);
```

Port options (fields of `ArduinoFlasherPort`): `reset_invert`, `boot_invert`,
`reset_hold_ms` (100), `boot_hold_ms` (50), `log` (a `Print*`).

## Notes

- Library logging is compiled out unless built with `-DSERIAL_FLASHER_LOG_LEVEL=1..4`
  (ERROR..DEBUG), e.g. PlatformIO `build_flags`. Errors are always returned as `esp_loader_error_t`.
- Only what you call ends up in the binary (the linker drops unused sections). Measured on ESP32:
  `esp_loader_connect()` (ROM loader) ~9 KB, `esp_loader_connect_with_stub()` ~124 KB (stubs for every chip),
  a custom stub provider with one chip ~16 KB. eFuse code costs nothing unless called.
- Needs a 32-bit core (ESP32, RP2040, STM32, SAMD...). AVR is too small.

## Only the stubs you need

```cpp
#include <esp_loader_stubs.h>

static const esp_stub_t *my_stubs(esp_loader_t *, target_chip_t chip, void *)
{
    switch (chip) {
    case ESP32C3_CHIP: return &esp_stub_esp32c3;
    case ESP32S3_CHIP: return &esp_stub_esp32s3;
    default:           return nullptr;  // connect fails with ESP_LOADER_ERROR_UNSUPPORTED_CHIP
    }
}

esp_loader_connect_with_stub_provider(&loader, &args, my_stubs, nullptr);
```

## Updating upstream

```sh
./update.sh v2.1.0   # an upstream release tag
```

Clones that tag from GitHub, replaces the core in `src/`, records it in `UPSTREAM_VERSION` and sets `version=` in `library.properties`.
