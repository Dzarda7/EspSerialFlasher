// Flashes /app.bin from LittleFS to a target ESP at 0x10000.
//
// Wiring (host -> target):  TX -> RX0, RX <- TX0, RESET_PIN -> EN, BOOT_PIN -> IO0 (GPIO9 on C3/C6/H2), GND -> GND
// Upload app.bin with the "LittleFS Data Upload" tool (or `pio run -t uploadfs`).

#include <EspSerialFlasher.h>
#include <LittleFS.h>

#define TARGET_RX_PIN   16
#define TARGET_TX_PIN   17
#define TARGET_RESET    25
#define TARGET_BOOT     26
#define APP_PATH        "/app.bin"
#define APP_OFFSET      0x10000
#define HIGHER_BAUDRATE 460800

static ArduinoFlasherPort port;
static esp_loader_t loader;

static bool check(esp_loader_error_t err, const char *what)
{
    if (err != ESP_LOADER_SUCCESS) {
        Serial.printf("%s failed: %s\n", what, flasher_error_string(err));
        return false;
    }
    return true;
}

static bool flash_file(File &file, uint32_t offset)
{
    static uint8_t payload[1024];

    esp_loader_flash_cfg_t cfg = {};
    cfg.offset = offset;
    cfg.image_size = file.size();
    cfg.block_size = sizeof(payload);

    Serial.printf("Erasing %u bytes at 0x%x...\n", (unsigned)cfg.image_size, (unsigned)offset);
    if (!check(esp_loader_flash_start(&loader, &cfg), "flash start")) {
        return false;
    }

    size_t written = 0;
    while (written < cfg.image_size) {
        size_t n = file.read(payload, sizeof(payload));
        if (n == 0) {
            Serial.println("file read failed");
            return false;
        }
        if (!check(esp_loader_flash_write(&loader, &cfg, payload, n), "flash write")) {
            return false;
        }
        written += n;
        Serial.printf("\r%u%%", (unsigned)(written * 100 / cfg.image_size));
    }
    Serial.println();

    return check(esp_loader_flash_finish(&loader, &cfg), "flash finish (MD5 verify)");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }
    File file = LittleFS.open(APP_PATH, "r");
    if (!file) {
        Serial.println("Cannot open " APP_PATH);
        return;
    }

    // The ROM bootloader always starts at 115200.
    Serial1.begin(115200, SERIAL_8N1, TARGET_RX_PIN, TARGET_TX_PIN);

    port.serial = &Serial1;
    port.reset_pin = TARGET_RESET;
    port.boot_pin = TARGET_BOOT;
    port.log = &Serial;

    if (!check(esp_loader_init_serial(&loader, &port.port), "init")) {
        return;
    }

    // Stub gives faster writes and works around ROM quirks; plain
    // esp_loader_connect() avoids linking the stub blobs (~115 KB);
    // esp_loader_connect_with_stub_provider() links only the stubs you return (see README).
    esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
    if (!check(esp_loader_connect_with_stub(&loader, &args), "connect")) {
        return;
    }
    Serial.printf("Connected to target chip id %d\n", esp_loader_get_target(&loader));

    // Switches both the target and our Serial1.
    check(esp_loader_change_transmission_rate(&loader, HIGHER_BAUDRATE), "baud change");

    if (flash_file(file, APP_OFFSET)) {
        Serial.println("Done, resetting target");
        esp_loader_reset_target(&loader);
    }
    file.close();
}

void loop()
{
}
