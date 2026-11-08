/*
   Adapted from Bastelschlumpf/M5PaperWeather for M5Paper S3
   Ported to M5Stack PaperDIY (ESP32-S3, ED047TC1 e-paper)
   Modified to use M5Unified instead of M5EPD
   Uses Open-Meteo API instead of OpenWeatherMap

   Version 1.13-PaperDIY - Refactored for better modularity and performance
*/

#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>

// Include all our new modular headers
#include "constants.h"
#include "utils.h"
#include "weather_api.h"
#include "config.h"
#include "display.h"

// Global objects
Preferences preferences;
M5Canvas canvas(&M5.Display);
WeatherData currentWeather;

// Configuration state
bool useCelsius = false;
bool nightModeSleep = true;
String cityName = DEFAULT_CITY;

// Runtime state
unsigned long lastRefreshTime = 0;
int refreshCounter = 0;

// PM1 register map (from M5PM1 datasheet)
static constexpr uint8_t PM1_ADDR = 0x6E;
static constexpr uint32_t PM1_FREQ = 100000;

static constexpr uint8_t PM1_REG_SYS_CMD   = 0x0C;   // shutdown command
static constexpr uint8_t PM1_SYS_CMD_SHUTDOWN = 0xA1;

static constexpr uint8_t PM1_REG_TIM_CNT0  = 0x38;   // timer counter seconds [3:0] big-endian
static constexpr uint8_t PM1_REG_TIM_CFG   = 0x3C;   // bit3=ARM, bits2:0=ACTION
static constexpr uint8_t PM1_REG_TIM_KEY   = 0x3D;   // write 0xA5 to arm/reload

static constexpr uint8_t PM1_TIM_ACTION_STOP      = 0b000;
static constexpr uint8_t PM1_TIM_ACTION_POWERON   = 0b011;

void enterDeepSleep(unsigned long sleepTimeMs) {
    Serial.printf("Entering deep sleep for %lu ms (%lu minutes)\n",
                  sleepTimeMs, sleepTimeMs / 60000);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Put display to sleep and wait for it to finish
    M5.Display.sleep();
    M5.Display.waitDisplay();
    delay(200);

    // Flush serial before sleep
    Serial.flush();

    // --- Power down PM1 peripherals for minimum current ---
    // Turn off the power LED (PM1 LED_EN pin)
    M5.Power.M5pm1.setLedEnLevel(false);

    // Turn off the 5V boost output (not needed during sleep)
    M5.Power.M5pm1.setExtOutput(false);

    // Turn off e-paper display power via PM1 GPIO2 (EPD_PWR).
    // PM1 GPIO2 is configured as EPD_PWR output in M5Unified's board init;
    // driving it LOW cuts power to the ED047TC1 panel.
    M5.Power.M5pm1.setGPIOOutput(m5::M5PM1_Class::gpio2, false);

    // --- Arm M5PM1 timer for power-on wake ---
    // The PM1 timer cuts L2 power (ESP32 fully off), then restores L2 when the
    // timer expires, causing a full ESP32 reboot. No ESP32 deep sleep is used —
    // wake is driven entirely by the M5PM1.
    uint32_t seconds = sleepTimeMs / 1000;
    if (seconds == 0) seconds = 1;

    // Timer count is 4 bytes little-endian starting at TIM_CNT_0 (0x38)
    uint8_t tim_cmd[4] = {
        static_cast<uint8_t>(seconds & 0xFF),
        static_cast<uint8_t>((seconds >> 8)  & 0xFF),
        static_cast<uint8_t>((seconds >> 16) & 0xFF),
        static_cast<uint8_t>((seconds >> 24) & 0xFF),
    };
    M5.In_I2C.writeRegister(PM1_ADDR, PM1_REG_TIM_CNT0, tim_cmd, 4, PM1_FREQ);

    // TIM_CFG: ARM=1, ACTION=POWERON(0b011)
    uint8_t tim_cfg = (1 << 3) | PM1_TIM_ACTION_POWERON;  // 0x0B
    M5.In_I2C.writeRegister8(PM1_ADDR, PM1_REG_TIM_CFG, tim_cfg, PM1_FREQ);

    // TIM_KEY: write 0xA5 to arm/reload the timer
    M5.In_I2C.writeRegister8(PM1_ADDR, PM1_REG_TIM_KEY, 0xA5, PM1_FREQ);

    // Shutdown: PM1 closes L2/L3A/L3B switches, cutting ESP32 power. The PM1
    // timer restores L2 when it expires, rebooting the ESP32 via CHIP_PU.
    M5.In_I2C.writeRegister8(PM1_ADDR, PM1_REG_SYS_CMD, PM1_SYS_CMD_SHUTDOWN, PM1_FREQ);

    // If shutdown did not take effect (e.g. powered via USB where PM1 won't cut
    // power), fall back to ESP32 deep sleep with timer wake so the device still
    // wakes on schedule.
    M5.Power.deepSleep(sleepTimeMs * 1000);
}

void setup() {
    M5.begin();
    M5.Display.begin();
    Serial.begin(115200);

    // Small delay to ensure display is fully initialized after wake
    delay(100);

    Serial.println("\n=================================");
    Serial.println("PaperDIYWeather " + String(VERSION));
    Serial.println("Based on Bastelschlumpf design");
    Serial.println("=================================");

    // Reinitialize canvas after M5.Display is ready
    // This fixes the automatic wake display issue
    canvas.setColorDepth(16);
    canvas.createSprite(1, 1);  // Create minimal sprite to initialize
    canvas.deleteSprite();       // Clean up

    // Configure display
    M5.Display.setRotation(1);
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 20);
    M5.Display.println("PaperDIYWeather " + String(VERSION));
    M5.Display.setCursor(20, 50);
    M5.Display.println("Initializing...");
    M5.Display.endWrite();
    M5.Display.display();

    Serial.println("Splash screen displayed");
    delay(2000);

    setupWiFi();

    // Load timezone offset before setting time (applied to NTP/RTC time)
    int timezoneOffsetHours = loadTimezoneOffset();

    // Set system time: use RTC if it has a valid date, otherwise fall back to NTP
    setupTime(timezoneOffsetHours);

    // Load preferences and fetch weather
    if (WiFi.status() == WL_CONNECTED) {
        float latitude, longitude;
        loadPreferences(latitude, longitude, cityName);

        Serial.printf("Fetching weather for: %.4f, %.4f (%s)\n",
                     latitude, longitude, cityName.c_str());

        bool fetchSuccess = false;
        for (int retry = 0; retry < HTTP_RETRY_ATTEMPTS; retry++) {
            if (retry > 0) {
                Serial.printf("Weather fetch retry %d/%d...\n", retry + 1, HTTP_RETRY_ATTEMPTS);
                delay(HTTP_RETRY_DELAY_MS);
            }

            if (fetchWeatherData(latitude, longitude)) {
                Serial.println("Weather fetch successful!");
                displayWeather();
                lastRefreshTime = millis();
                fetchSuccess = true;
                break;
            }
        }

        if (!fetchSuccess) {
            Serial.println("All weather fetch attempts failed!");
            M5.Display.startWrite();
            M5.Display.fillScreen(TFT_WHITE);
            M5.Display.setTextColor(TFT_BLACK);
            M5.Display.setCursor(20, 20);
            M5.Display.println("Failed to fetch weather");
            M5.Display.println("Will retry in 1 minute");
            M5.Display.endWrite();
            M5.Display.display();
            // Retry sooner on failure
            lastRefreshTime = millis() - REFRESH_INTERVAL_DAY_MS + 60000;
        }
    } else {
        M5.Display.startWrite();
        M5.Display.fillScreen(TFT_WHITE);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setCursor(20, 20);
        M5.Display.println("No WiFi - Press BtnA to configure");
        M5.Display.endWrite();
        M5.Display.display();
        lastRefreshTime = millis();
    }

    Serial.println("Setup complete!");
}

void loop() {
    static bool hasWaited = false;

    // Check WHY we woke up - only show interaction window on manual reset
    if (!hasWaited) {
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

        // ESP_SLEEP_WAKEUP_UNDEFINED means: power on or reset button pressed
        // ESP_SLEEP_WAKEUP_TIMER means: automatic wake from deep sleep
        if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
            // Manual reset/power on - give user time to interact
            Serial.println("\n*** Manual wake detected (reset button or power on) ***");
            Serial.println("*** Waiting 30 seconds for user interaction ***");
            Serial.println("*** Press ButtonA (GPIO4) for CONFIG ***");

            unsigned long startWait = millis();
            const unsigned long waitDuration = USER_INTERACTION_TIMEOUT_MS;
            unsigned long lastSerialUpdate = 0;

            pinMode(CFG_BUTTON_PIN, INPUT_PULLUP);
            pinMode(ALT_CFG_BUTTON_PIN, INPUT_PULLUP);

            while (millis() - startWait < waitDuration) {
                // Check for ButtonA press (CFG trigger) - buttons are active-low
                bool btnAPressed = (digitalRead(CFG_BUTTON_PIN) == LOW);

                // Check for ButtonB press (alternate CFG trigger)
                bool btnBPressed = (digitalRead(ALT_CFG_BUTTON_PIN) == LOW);

                if (btnAPressed || btnBPressed) {
                    Serial.println("\n*** CONFIG button pressed! ***");
                    M5.Display.startWrite();
                    M5.Display.fillScreen(TFT_WHITE);
                    M5.Display.setTextSize(2);
                    M5.Display.setCursor(20, 20);
                    M5.Display.println("Opening Configuration...");
                    M5.Display.println("\nConnect to:");
                    M5.Display.println("  M5Paper-Weather");
                    M5.Display.println("Password: configure");
                    M5.Display.println("URL: 192.168.4.1");
                    M5.Display.endWrite();
                    M5.Display.display();

                    // Wait for button release before starting config portal,
                    // otherwise the same press would be seen as an exit request
                    Serial.println("*** Waiting for button release ***");
                    while (digitalRead(CFG_BUTTON_PIN) == LOW || digitalRead(ALT_CFG_BUTTON_PIN) == LOW) {
                        delay(10);
                    }
                    Serial.println("*** Button released, opening Configuration ***");

                    // Disconnect from WiFi and start config portal
                    WiFi.disconnect();
                    delay(500);
                    startConfigPortal();

                    // After config, restart
                    ESP.restart();
                }

                // Update serial countdown every second
                unsigned long remaining = (waitDuration - (millis() - startWait)) / 1000;
                if (millis() - lastSerialUpdate >= 1000) {
                    Serial.printf("Waiting for config button press... %lu seconds remaining\n", remaining);
                    lastSerialUpdate = millis();
                }

                delay(100);  // Small delay to reduce CPU usage
            }

            Serial.println("*** Wait period ended, entering sleep mode ***\n");
        } else {
            // Automatic wake from timer - skip interaction window
            Serial.println("\n*** Automatic wake from timer - skipping interaction window ***");
            // Give e-ink display time to complete refresh before sleeping
            Serial.println("*** Waiting 3 seconds for display to refresh ***");
            delay(3000);
        }

        hasWaited = true;
    }

    unsigned long sleepTime = getRefreshInterval();

    // Get current settings for display
    preferences.begin("weather", true);
    int nightStart = preferences.getInt("night_start", 22);
    int nightEnd = preferences.getInt("night_end", 5);
    preferences.end();

    Serial.println("=================================");
    Serial.printf("Night mode: %s\n", nightModeSleep ? "ENABLED" : "DISABLED");
    if (nightModeSleep) {
        Serial.printf("Night hours: %d:00 - %d:00\n", nightStart, nightEnd);
    }
    Serial.printf("Current time is: %s\n", isNightTime() ? "NIGHT" : "DAY");
    Serial.printf("Refresh interval: %lu minutes\n", sleepTime / 60000);
    Serial.printf("Refresh counter: %d/6\n", refreshCounter % 6);
    Serial.println("=================================");

    enterDeepSleep(sleepTime);
}
