# PaperDIYWeather

A comprehensive weather station for the **M5Stack PaperDIY** e-ink display kit (ESP32-S3), featuring real-time weather data, 8-hour forecasts, and intelligent power management via the M5PM1 power chip. Built with the M5Unified library and powered by the Open-Meteo API.

[![Arduino](https://img.shields.io/badge/Arduino-Compatible-brightgreen.svg)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Ready-orange.svg)](https://platformio.org/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Supported-blue.svg)](https://www.espressif.com/)

## Features

- **Current Weather Conditions**: Large, easy-to-read temperature display with weather icons and conditions
- **8-Hour Forecast**: Hourly weather predictions with temperature and icons
- **Multiple Data Graphs**: Temperature, precipitation, humidity, and atmospheric pressure trends
- **User-Configurable Refresh Intervals**: Set day (5-120 min) and night (15-240 min) update frequencies via web interface
- **Smart Wake Detection**: Config button accessible only on manual reset (saves battery)
- **Customizable Night Mode**: Set your own night hours (0-23) for reduced refresh rate
- **Celsius/Fahrenheit Support**: Toggle between temperature units via configuration
- **Timezone Configuration**: User-configurable UTC offset (-12 to +14) via web interface
- **City-Based Location**: Automatic coordinate lookup by city name
- **Modern Web Configuration**: Enhanced portal with validation and mobile-friendly design
- **Minimum-Power Sleep**: M5PM1 timer wake (no ESP32 deep sleep) — power LED, 5V boost, and e-paper panel are powered down before sleep
- **Sun & Moon Information**: Sunrise/sunset times and moon phase display
- **Wind Compass**: Real-time wind speed and direction visualization
- **Battery & WiFi Status**: Always-visible system information
- **Modular Architecture**: Clean, maintainable code structure

## Hardware Requirements

- **M5Stack PaperDIY Assembly Kit** (ESP32-S3-WROOM-1-N16R8, ED047TC1 4.7" e-ink 960×540 16-grayscale, M5PM1 power chip)
- **USB-C cable** for programming and power
- **WiFi connection** for weather data

Note: The PaperDIY has **no touch screen**. Configuration is done via the physical buttons (GPIO4 / ButtonA and GPIO3 / ButtonB).

## Attribution

This project is based on:

- **[Bastelschlumpf's M5PaperWeather](https://github.com/Bastelschlumpf/M5PaperWeather)** — original design and panel-based display layout
- **[squirmen's PaperS3Weather](https://github.com/squirmen/PaperS3Weather)** — M5Paper S3 port using M5Unified and Open-Meteo API, which this project was adapted from

### Key Changes from PaperS3Weather

- **Hardware Platform**: Migrated from M5Paper S3 to M5Stack PaperDIY (no touch screen, M5PM1 power IC, 2 physical buttons)
- **Library Versions**: Bumped M5Unified to `^0.3.0` and M5GFX to `^0.3.0` (PaperDIY board support requires M5GFX ≥ 0.2.26, added in [M5Unified PR #281](https://github.com/m5stack/M5Unified/pull/281))
- **Buttons**: `M5.BtnA`/`M5.BtnB` are not mapped on PaperDIY; ButtonA (GPIO4) and ButtonB (GPIO3) are read via `digitalRead(INPUT_PULLUP)` (active-low)
- **Display Panel**: Renamed `drawM5PaperInfo()` → `drawPaperDIYInfo()`, panel title changed to "PaperDIY"
- **Power Management**: Pure M5PM1 timer wake — PM1 timer registers (TIM_CNT/TIM_CFG/TIM_KEY) are programmed via I2C, then SYS_CMD shutdown cuts L2 power. The PM1 timer restores L2 on expiry, rebooting the ESP32. No `esp_deep_sleep_start()` is used.
- **Peripheral Power-Down**: Before sleep, the power LED (LED_EN), 5V boost (ExtOutput), and e-paper panel power (PM1 GPIO2 / EPD_PWR) are turned off for minimum current draw
- **Timezone**: User-configurable UTC offset stored in Preferences, applied to NTP/RTC time via `configTime()`
- **Weather API**: Open-Meteo API (no API key required)
- **Location System**: City-based geocoding with automatic coordinate lookup

## Quick Start

### For Beginners

#### 1. Install PlatformIO

If you're new to embedded development, PlatformIO is an excellent development environment:

1. Download and install [Visual Studio Code](https://code.visualstudio.com/)
2. Open VS Code and install the PlatformIO extension:
   - Click the Extensions icon (or press `Ctrl+Shift+X`)
   - Search for "PlatformIO IDE"
   - Click "Install"
3. Restart VS Code

#### 2. Open This Project

1. Open VS Code
2. Click **File → Open Folder**
3. Select the `PaperDIYWeather` folder
4. Wait for PlatformIO to initialize and download dependencies (first time only)

#### 3. Connect Your PaperDIY

1. Connect your PaperDIY to your computer via USB-C cable
2. The device should be recognized automatically
3. Press the power button if the device doesn't turn on

#### 4. Build and Upload

1. In VS Code, look for the PlatformIO toolbar at the bottom
2. Click the **Build** button (checkmark icon) to compile
3. Once build succeeds, click the **Upload** button (arrow icon)
4. Wait for upload to complete (progress shown in terminal)

#### 5. Configure WiFi and Location

After first boot, press **ButtonA** (GPIO4) within 30 seconds of reset to enter configuration mode:

1. **Connect to WiFi**:
   - On your phone/computer, look for WiFi network: `M5Paper-Weather`
   - Password: `configure`
   - Open browser and go to: `http://192.168.4.1`

2. **Enter Settings**:
   - **WiFi SSID**: Your home WiFi network name
   - **WiFi Password**: Your WiFi password
   - **City Name**: Your city (e.g., "Auckland", "New York", "Tokyo")
     - Just enter the city name, coordinates will be looked up automatically
     - Leave Latitude/Longitude fields blank unless you want manual coordinates
   - **Temperature Unit**: Choose Fahrenheit (default) or Celsius from dropdown
   - **Timezone Offset**: Your local offset from UTC in hours (e.g., `1` for CET, `-5` for EST, `13` for NZST). Default is `1`.
   - **Night Mode**: Leave checked (default) to enable reduced refresh rate 10pm-5am

3. **Save & Restart**:
   - Click "Save & Restart"
   - Device will restart and connect to your WiFi
   - Your city coordinates will be geocoded automatically (requires internet)
   - Weather data will be fetched and displayed

4. **Enjoy!**:
   - Weather dashboard will display and update automatically
   - Device sleeps between updates to save battery
   - Check serial monitor (`pio device monitor`) to verify city geocoding succeeded

### For Developers

Quick command-line workflow:

```bash
# Add PlatformIO to PATH
export PATH="$PATH:/Users/lzaccaria/Library/Python/3.11/bin"

# Build project
cd ~/Projects/PaperDIYWeather
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

## Configuration

### WiFi Configuration Portal

**How to Access**:
1. Press the reset button on your PaperDIY device
2. Device wakes and displays weather
3. **Within 30 seconds**, press **ButtonA** (GPIO4) to enter configuration
4. Configuration portal opens automatically

**Automatic Portal Access**:
- First boot (no WiFi configured)
- Connection fails to saved network

**Portal Details**:
- Network: `M5Paper-Weather`
- Password: `configure`
- URL: `http://192.168.4.1`

**Note**: Automatic weather updates (every 10-60 minutes) do NOT show the 30-second config window — they immediately refresh and sleep. This saves battery. The config window only appears after manual reset button press.

### Location Settings

Two methods to set location:

1. **City Name** (Recommended):
   - Enter your city name (e.g., "London", "Sydney", "New York")
   - Leave latitude/longitude fields blank
   - Coordinates are looked up automatically AFTER WiFi connection
   - Simpler and more user-friendly
   - Examples: "Tokyo", "Paris", "Auckland", "San Francisco"

2. **Manual Coordinates** (Advanced):
   - Enter exact latitude and longitude in decimal degrees
   - Useful for specific locations not found by city search
   - Format: Decimal degrees (e.g., -36.8485, 174.7633)
   - Overrides city name if provided

**How City Geocoding Works:**

The configuration portal runs in WiFi Access Point mode (no internet access), so city coordinates cannot be looked up during configuration. Instead:

1. Enter your city name in the config portal
2. Save settings and device restarts
3. Device connects to your WiFi (now has internet)
4. Coordinates are automatically geocoded from city name
5. Coordinates are saved for future use

This means you won't see geocoding errors in the config portal — they only occur after WiFi connection if the city name can't be found.

### Temperature Unit

Choose between Fahrenheit (default) or Celsius via dropdown menu in configuration portal. This affects all temperature displays including current conditions, forecasts, and graphs.

### Timezone Offset

Set your local timezone offset from UTC (−12 to +14 hours). Examples:
- GMT+0: `0`
- CET (Central Europe): `1`
- EST (US Eastern): `-5`
- PST (US Pacific): `-8`
- JST (Japan): `9`
- NZST (New Zealand): `13`

This offset is applied to NTP time and RTC time, and is stored in Preferences.

### Night Mode

**Enabled by default** via checkbox. When enabled, night mode reduces refresh frequency from 10 minutes to 60 minutes between 10pm and 5am, significantly extending battery life for overnight operation.

## Development

### Project Structure

```
PaperDIYWeather/
├── src/
│   ├── main.cpp           # Main application entry point
│   ├── constants.h        # Configuration constants and defines
│   ├── utils.h/cpp        # Helper functions (temp, time, weather)
│   ├── weather_api.h/cpp  # Open-Meteo API communication
│   ├── config.h/cpp       # WiFi setup and web portal
│   ├── display.h/cpp      # Display rendering functions
│   └── Icons.h            # Weather icon bitmap data
├── platformio.ini         # PlatformIO configuration
├── CHANGELOG.md           # Version history
├── README.md              # This file
├── LICENSE                # MIT License
└── ADAPTATION_NOTES.md    # Detailed notes on PaperS3Weather → PaperDIY adaptation
```

### Customizing Refresh Intervals and Night Mode

**Via Web Interface (Recommended)**:

1. Press the reset button on your PaperDIY
2. Within 30 seconds, press ButtonA (GPIO4) to enter config
3. Connect to WiFi: `M5Paper-Weather` (password: `configure`)
4. Navigate to: `http://192.168.4.1`
5. Set custom refresh intervals:
   - **Day Time**: 5-120 minutes (how often to update during the day)
   - **Night Time**: 15-240 minutes (how often to update at night)
6. Set custom night mode hours:
   - **Start Hour**: 0-23 (when night mode begins, e.g., 22 = 10pm)
   - **End Hour**: 0-23 (when night mode ends, e.g., 5 = 5am)
7. Save and restart

**Via Code** (for advanced users):

Edit `src/constants.h`:

```cpp
#define REFRESH_INTERVAL_DAY_MS 600000      // 10 minutes (default)
#define REFRESH_INTERVAL_NIGHT_MS 3600000   // 60 minutes (default)
#define NIGHT_START_HOUR 22  // 10pm (default)
#define NIGHT_END_HOUR 5     // 5am (default)
```

These are fallback defaults. User preferences saved via web interface take precedence.

### Modifying Display Layout

The display uses Bastelschlumpf's panel-based layout:

- **Top Section**: 4 info panels (Current, Sun/Moon, Wind, PaperDIY Info)
- **Middle Section**: 8 hourly forecast columns
- **Bottom Section**: 4 data graphs

Panel positions are defined in `displayWeather()` function in `src/main.cpp`. Coordinates use absolute positioning for precise layout control.

### Adding Features

The code is modular and well-structured for extensions:

- **New weather data**: Modify `WeatherData` struct and `fetchWeatherData()`
- **Additional graphs**: Add new `drawGraph()` calls in `displayWeather()`
- **Custom panels**: Create new `draw*()` functions following existing patterns
- **Different APIs**: Replace Open-Meteo calls in `fetchWeatherData()`

## Troubleshooting

### Device Not Connecting to WiFi

**Problem**: Configuration portal doesn't appear or connection fails

**Solutions**:
- Ensure WiFi network is 2.4GHz (ESP32-S3 doesn't support 5GHz)
- Check WiFi password is correct (case-sensitive)
- Move device closer to router during initial setup
- Restart device by pressing power button

**Debug**:
```bash
pio device monitor --baud 115200
```
Watch serial output for connection errors.

### Config Mode Not Entering

**Problem**: Pressing ButtonA within 30 seconds doesn't open the config portal

**Solutions**:
- Make sure you press ButtonA (GPIO4) within 30 seconds of reset — the window is short to save battery
- ButtonA and ButtonB are read via direct GPIO (`digitalRead`) on PaperDIY; `M5.BtnA`/`M5.BtnB` are not mapped by M5Unified
- Try ButtonB (GPIO3) as an alternative
- Check serial output for "CONFIG button pressed!" message

### Display Not Updating

**Problem**: Weather display shows old data or "Failed to fetch weather"

**Solutions**:
- Verify internet connection is working
- Check serial monitor for API errors
- Confirm location coordinates are valid
- Open-Meteo API may be temporarily unavailable (retry in a few minutes)

**Manual Test**:
Visit in browser: `https://api.open-meteo.com/v1/forecast?latitude=40.7128&longitude=-74.0060&current=temperature_2m`
(Replace coordinates with your location)

### Upload Fails

**Problem**: PlatformIO can't upload to device

**Solutions**:
- Press and hold power button to ensure device is on
- Try different USB cable (some are charge-only)
- Try different USB port on your computer
- Install/update USB drivers for your operating system
- Check device is recognized: `pio device list`

### Serial Monitor Shows Gibberish

**Problem**: Serial output is unreadable

**Solution**:
Ensure baud rate is 115200:
```bash
pio device monitor --baud 115200
```

### Battery Drains Quickly

**Problem**: Device battery doesn't last long

**Solutions**:
- Enable Night Mode in configuration
- Increase refresh intervals (edit main.cpp or via web portal)
- Check deep sleep is working (serial monitor should show sleep messages)
- Verify WiFi signal is strong (weak signal increases power consumption)
- The power LED, 5V boost, and e-paper panel are powered down during sleep for minimum current

### Configuration Portal Won't Open

**Problem**: Can't access `192.168.4.1`

**Solutions**:
- Ensure you're connected to `M5Paper-Weather` WiFi network
- Disable mobile data on phone (forces use of WiFi)
- Try `http://` explicitly: `http://192.168.4.1`
- Clear browser cache or try different browser
- Check device serial output for AP startup messages

### Weather Icons Not Displaying

**Problem**: Weather icons appear as blank squares

**Solution**:
- Icons.h may be corrupt — re-download project
- Check sufficient flash memory during build
- Verify `Icons.h` is in `src/` directory

### City Name Not Found / Geocoding Failed

**Problem**: Serial monitor shows "Geocoding failed" or "could not find city use lat long"

**Solutions**:
- Try city name without state/country (e.g., "Portland" not "Portland, Oregon")
- Try adding country (e.g., "Portland USA" or "Portland UK")
- Use major city nearby if your location is small
- Switch to manual coordinates as fallback
- Check internet connection is working (geocoding requires internet)

**How to see geocoding results**:
```bash
pio device monitor --baud 115200
```
Look for lines like:
```
Geocoding city: Tokyo
Found: Tokyo, Japan at 35.6895, 139.6917
```

**Manual Override**:
If geocoding consistently fails, enter manual coordinates in the config portal. You can find coordinates by:
1. Searching "[your city] coordinates" in Google
2. Using [latlong.net](https://latlong.net/)
3. Right-clicking location in Google Maps → "What's here?"

### Wrong Time Zone

**Problem**: Times displayed don't match local time

**Solution**:
1. Press ButtonA within 30 seconds of reset to enter config mode
2. Connect to `M5Paper-Weather` WiFi and open `http://192.168.4.1`
3. Set **Timezone Offset** to your local offset from UTC (e.g., `1` for CET)
4. Click Save & Restart

The timezone is stored in Preferences and applied on every boot.

## API Information

This project uses the [Open-Meteo API](https://open-meteo.com/), a free weather API that requires no API key. The API provides:

- Current weather conditions
- Hourly forecasts
- Daily forecasts
- Sunrise/sunset times
- Multiple weather parameters

**API Limits**: Open-Meteo is free for non-commercial use with reasonable request limits. The default refresh interval (10 minutes) is well within limits.

## Power Consumption

The PaperDIY uses the M5PM1 power chip. During sleep:

- Power LED is turned off (PM1 LED_EN)
- 5V boost output is turned off (PM1 ExtOutput)
- E-paper panel power is turned off (PM1 GPIO2 / EPD_PWR)
- L2 rail (ESP32 core power) is cut by the PM1 shutdown command
- The PM1 timer restores L2 power when it expires, rebooting the ESP32

No ESP32 `deep_sleep` is used — wake is driven entirely by the M5PM1 timer.

## License

MIT License

Copyright (c) 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Acknowledgments

- **Original Design**: [Bastelschlumpf](https://github.com/Bastelschlumpf) for the excellent M5PaperWeather project
- **M5Paper S3 Port**: [squirmen](https://github.com/squirmen/PaperS3Weather) for the M5Unified/Open-Meteo adaptation this project is based on
- **Weather Data**: [Open-Meteo](https://open-meteo.com/) for providing free, no-API-key weather data
- **Hardware**: [M5Stack](https://m5stack.com/) for the PaperDIY device and M5Unified/M5GFX libraries
- **Icons**: Weather icons from the original M5PaperWeather project

## Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check existing issues for similar problems
- Include serial monitor output for debugging

---

**Enjoy your PaperDIYWeather station!**