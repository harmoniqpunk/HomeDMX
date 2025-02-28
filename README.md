# HomeDMX

HomeKit Bridge Accesory built on top of ESP32 [HomeSpan](https://github.com/HomeSpan/HomeSpan) to controll DMX512 Light fixtures. Tested on ESP32-WROOM with Sparkfun DMX to LED Shield

## Description

I installed some moving heads and DMX-controlled LED dimmers in my daughter's room a few years ago. Recently, she expressed a desire to be able to trigger different lighting scenes using Siri on her HomePod, so I purchased an ESP32 microcontroller with a DMX 512 shield to enable this functionality. Now, she can simply say "Hey Siri, turn on Scene X" and the lighting in her room will change to the specified scene.

The project use VSCode editor, Arduino frameworks and ESP32 toolchain for Arduino   

## Secure Credential Management

This project uses a template-based configuration approach to avoid hardcoding credentials:

1. `config.h.template` - A template file showing required configuration fields
2. `config.h` - Your actual configuration file with credentials (not committed to git)

### Setting Up WiFi Credentials

There are two ways to set up your WiFi credentials:

#### Option 1: Using Environment Variables

Set environment variables in your shell:

```bash
export HOMEDMX_WIFI_SSID="Your WiFi Name"
export HOMEDMX_WIFI_PASS="Your WiFi Password"
```

Then run the build script which will automatically generate a config.h:

```bash
./build.sh
```

#### Option 2: Manual Configuration

Copy the template file:

```bash
cp config.h.template config.h
```

Then edit config.h with your actual credentials.

### Building and Uploading

The build script provides several options:

```bash
./build.sh        # Build and upload
./build.sh build  # Build only
./build.sh upload # Upload only
./build.sh config # Generate config.h only
./build.sh clean  # Clean build directory
```

### Security Notes

- The `config.h` file is excluded in .gitignore to prevent accidental commits
- For production use, consider using encrypted storage on the ESP32
- Always use unique credentials for your IoT devices

## Getting Started

### Dependencies

* [HomeSpan](https://github.com/HomeSpan/HomeSpan)
* [SparkFunDMX](https://github.com/sparkfun/SparkFunDMX)

### macOS Installation

1. Install Arduino IDE:
   - Download [Arduino IDE](https://www.arduino.cc/en/software) for macOS
   - Drag Arduino application to your Applications folder

2. Install USB to UART Driver for ESP32:
   - Download [CP210x USB to UART Bridge VCP Drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
   - Open the downloaded .pkg file and follow installation instructions
   - Restart your computer after installation
   - After connecting ESP32, you should see `/dev/tty.usbserial-*` in Terminal when running `ls /dev/tty.*`

3. Install ESP32 Board Support:
   - Open Arduino IDE
   - Go to Arduino -> Preferences
   - Add ESP32 board URL to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools -> Board -> Boards Manager
   - Search for "esp32"
   - Install "ESP32 by Espressif Systems"

4. Install Required Libraries:
   - In Arduino IDE, go to Tools -> Manage Libraries (or press Shift+⌘+I)
   - Install HomeSpan:
     * Search for "HomeSpan" and install "HomeSpan by Gregg E. Berman"
   
   For SparkFunDMX, manual installation is required:
   ```bash
   # Navigate to Arduino libraries directory
   cd ~/Documents/Arduino/libraries
   
   # Clone the SparkFunDMX repository
   git clone https://github.com/sparkfun/SparkFunDMX.git
   ```
   
   Alternatively, you can:
   1. Download [SparkFunDMX](https://github.com/sparkfun/SparkFunDMX) as ZIP
   2. In Arduino IDE: Sketch -> Include Library -> Add .ZIP Library
   3. Select the downloaded ZIP file

5. Install VSCode and Extensions:
   - Download and install [Visual Studio Code](https://code.visualstudio.com/download)
   - Open VSCode
   - Press ⌘+Shift+X to open Extensions
   - Search and install:
     * "Arduino" by Vscode-arduino (author: moozzyk)
     * "C/C++" by Microsoft

### Board Configuration

The project is configured for ESP32-WROOM. The configuration in `.vscode/arduino.json` includes:

```json
{
    "board": "esp32:esp32:esp32",
    "configuration": "PSRAM=disabled,PartitionScheme=default,CPUFreq=240,FlashMode=qio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,DebugLevel=none"
}
```

Key settings:
- Board: ESP32 Dev Module
- CPU Frequency: 240MHz
- Flash Mode: QIO
- Flash Frequency: 80MHz
- Flash Size: 4MB
- Upload Speed: 921600
- PSRAM: Disabled

### VSCode Setup and Usage

1. Configure Arduino Extension:
   - Press ⌘+Shift+P (Command Palette)
   - Type "Arduino: Board Config" and select it
   - Choose "esp32:esp32:esp32" from the list
   - Type "Arduino: Select Serial Port" and select your port

2. Keyboard Shortcuts:
   ```
   ⌘+Shift+P, then type:
   - "Arduino: Verify"            - Compile sketch
   - "Arduino: Upload"           - Upload sketch
   - "Arduino: Open Serial Monitor" - Monitor serial output
   ```

3. Status Bar:
   - Click "⚡" icon to verify/compile
   - Click "→" icon to upload
   - Click "<plug>" icon to select port
   - Click "esp32:esp32:esp32" to change board

4. Troubleshooting:
   - If permission denied:
     ```bash
     sudo chmod a+rw /dev/tty.usbserial-*
     ```
   - If board not found:
     - ⌘+Shift+P -> "Arduino: Board Manager"
     - Search for "esp32" and install if needed
   - If port not found:
     - Unplug and replug the USB cable
     - Install the CP210x driver if needed

## Authors

Contributors names and contact info

Harmoniq Punk  
[Harmoniq Punk](https://github.com/harmoniqpunk)

## Version History

* 0.1.0
    * Initial Release

## License

This project is licensed under the MIT License - see the LICENSE.md file for details

## Acknowledgments

Inspiration, code snippets, etc.
* [homespan-examples-bridges](https://github.com/HomeSpan/HomeSpan/blob/master/examples/08-Bridges/08-Bridges.ino)

