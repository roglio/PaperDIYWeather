# PaperS3Weather → PaperDIYWeather Adaptation Notes

## Hardware differences addressed

| Feature | M5Paper S3 (original target) | PaperDIY (this build) |
|---|---|---|
| Display | 4.7" e-ink, 960×540 | 4.7" ED047TC1, 960×540, 16-grayscale, 8-bit parallel bus |
| Touch | Yes (GT911) | **No** — 2 physical buttons only |
| IMU sensor | Yes (internal temperature) | **No** |
| Power IC | AXP192 | M5PM1 |
| User buttons | Touch screen + power | ButtonA (GPIO4), ButtonB (GPIO3), power |

M5PaperDIY board auto-detection was added in [M5Unified PR #281](https://github.com/m5stack/M5Unified/pull/281), which requires M5GFX ≥ 0.2.26. The original project pinned M5Unified 0.2.7 / M5GFX 0.2.9, which predate this support.

## Files changed

### platformio.ini
- Renamed build env from `PaperS3` → `PaperDIY`
- Bumped `m5stack/M5Unified` `0.2.7` → `^0.3.0`
- Bumped `m5stack/M5GFX` `0.2.9` → `^0.3.0`
- Added `-DPAPERDIY` build flag

### src/constants.h
- Replaced hardcoded `TIMEZONE_OFFSET_HOURS 13` (Auckland) with `DEFAULT_TIMEZONE_OFFSET_HOURS 1` (Central European Time)

### src/constants.h
- Removed touch-area constants (`CFG_BUTTON_TOUCH_WIDTH`, `CFG_BUTTON_TOUCH_HEIGHT`)
- Added button pin defines: `CFG_BUTTON_PIN 4` (ButtonA / GPIO4), `ALT_CFG_BUTTON_PIN 3` (ButtonB / GPIO3)
- Version string updated to `Version 1.13-PaperDIY`
- Screen dimensions unchanged: 960×540

### src/main.cpp
- Removed `M5.Imu.init()` / `M5.Imu.sleep()` from `enterDeepSleep()` (no IMU on PaperDIY)
- **Buttons read via direct GPIO:** `M5.BtnA`/`M5.BtnB` are NOT mapped on PaperDIY (M5Unified's board init doesn't configure them). ButtonA=GPIO4 and ButtonB=GPIO3 are read with `digitalRead(INPUT_PULLUP)` (buttons are active-low). `pinMode()` is set once before the config wait loop and before the config portal exit loop.
- Updated `M5.Rtc.setAlarmIRQ(seconds)` → `M5.Rtc.setTimerIRQ(seconds * 1000)` — the `int` overload of `setAlarmIRQ` was deleted in the newer M5Unified API in favor of `setTimerIRQ(milliseconds)`
- **Pure M5PM1 timer wake (no ESP32 deep sleep):** `enterDeepSleep()` now relies entirely on the M5PM1 for wake-up:
  - `M5.Power.M5pm1.setLedEnLevel(false)` — turns off the power LED
  - `M5.Power.M5pm1.setExtOutput(false)` — turns off the 5V boost output
  - `M5.Power.M5pm1.setGPIOOutput(m5::M5PM1_Class::gpio2, false)` — turns off EPD_PWR (PM1 GPIO2), cutting power to the ED047TC1 e-paper panel
  - Writes PM1 timer registers directly via I2C (address 0x6E):
    - `0x38–0x3B` (TIM_CNT): sleep duration in seconds, big-endian
    - `0x3C` (TIM_CFG): bit3=ARM, bits2:0=ACTION=POWERON(0b011)
    - `0x3D` (TIM_KEY): write 0xA5 to arm the timer
  - Sends shutdown command `0xA1` to register `0x0C` (SYS_CMD). The PM1 closes L2/L3A/L3B switches, cutting ESP32 power entirely. When the PM1 timer expires, it restores L2 power, rebooting the ESP32 via CHIP_PU — a full restart from `setup()`.
  
  **No `esp_deep_sleep_start()` is used.** Wake is driven entirely by the M5PM1 timer, matching the [M5PM1 datasheet register map](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_EN.pdf) and the [M5PM1 Arduino library timer API](https://github.com/m5stack/M5PM1).
  
  Fallback: if PM1 shutdown does not take effect (e.g. powered via USB where PM1 won't cut power), `M5.Power.deepSleep()` is called as a fallback so the device still wakes on schedule.
- Updated splash screen text from "PaperS3Weather" → "PaperDIYWeather"
- Updated no-WiFi message from "Touch to configure" → "Press BtnA to configure"

### src/utils.cpp
- `readInternalTemperature()` now returns `SENSOR_ERROR_VALUE` unconditionally (no IMU); the display panel falls back to the weather API temperature reading
- `readInternalHumidity()` already returned `SENSOR_ERROR_VALUE` — unchanged behavior

### src/config.cpp
- Config portal exit loop accepts either ButtonA or ButtonB press
- Config portal now includes a **Timezone Offset** field (−12 to +14 hours from UTC), stored in preferences as `"timezone"`
- Added `loadTimezoneOffset()` function that reads the stored timezone (defaults to `DEFAULT_TIMEZONE_OFFSET_HOURS`)

### src/display.cpp / src/display.h
- Renamed `drawM5PaperInfo()` → `drawPaperDIYInfo()`; panel title text changed from "M5Paper S3" → "PaperDIY"
- Config indicator text changed from `[CFG]` → `[BTN_A=CFG]`

## Build results

```
RAM:   15.6% (51,104 / 327,680 bytes)
Flash: 21.4% (1,400,453 / 6,553,600 bytes)
Status: SUCCESS
```

## Flashing instructions

1. Connect your PaperDIY via USB-C
2. Add PlatformIO to PATH:
   ```bash
   export PATH="$PATH:/Users/lzaccaria/Library/Python/3.11/bin"
   ```
3. Build and upload:
   ```bash
   cd ~/Projects/PaperDIYWeather
   pio run --target upload
   ```
4. On first boot, press **ButtonA** within 30 seconds of reset to enter config mode
5. Connect your phone/laptop to WiFi `M5Paper-Weather` (password: `configure`)
6. Open `http://192.168.4.1` to set your WiFi, city, temperature units, and refresh intervals

## What was NOT changed (still works the same)

- Open-Meteo weather API calls and geocoding
- Web configuration portal (accessible over the `M5Paper-Weather` AP)
- E-ink partial refresh and ghosting avoidance logic
- Display layout (960×540 panels, graphs, compass, icons)
- Night mode and day/night refresh intervals
- Battery and RSSI status indicators
- Sun/moon and wind compass visualizations

**Changed sleep mechanism:** `M5.Power.powerOff()` was replaced with direct M5PM1 timer register writes + SYS_CMD shutdown. `powerOff()` closes the PMIC L2 rail (ESP32 power) with no wake source — the device would never wake up. The new sequence powers down PM1 peripherals (LED, boost, e-paper), arms the PM1 timer via I2C registers (TIM_CNT/TIM_CFG/TIM_KEY), then sends shutdown (SYS_CMD=0xA1). The PM1 timer restores L2 when it expires, rebooting the ESP32 via CHIP_PU. Fallback to `M5.Power.deepSleep()` if PM1 shutdown doesn't take effect (e.g. USB power).

## Suggested next steps

- Remove the inherited `.git` directory and initialize your own repository
- Consider reading battery level through the M5PM1 power chip if `M5.Power.getBatteryLevel()` needs adjustment for PaperDIY's power circuit
- Customize layout for portrait orientation if preferred (currently landscape, matching the original)
- Add battery voltage monitoring via M5PM1 ADC if more accurate readings are needed