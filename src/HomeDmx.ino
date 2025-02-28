/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2023-2024 HomeDMX Project
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/
 
////////////////////////////////////////////////////////////
//                                                        //
//             HomeDMX: HomeKit DMX Controller           //
//                                                        //
////////////////////////////////////////////////////////////

#include "HomeSpan.h"
#include "config.h"
#include <Preferences.h>
#include <SparkFunDMX.h>
#include <nvs_flash.h>

// Global variables
Preferences preferences;
String wifiSSID;
String wifiPassword;

// SparkFun DMX shield
SparkFunDMX dmx;

// Status LED pin - ESP32 Thing Plus has a built-in LED on pin 13
const int STATUS_LED_PIN = 13;
// Control button pin - you can add a physical button for reset/configuration
const int CONTROL_BUTTON_PIN = 0;  // Built-in BOOT button on ESP32
// DMX control pin (enable pin)
const int DMX_ENABLE_PIN = 21;  // Adjust this to match your wiring

// DMX Light fixture
struct DMX_Light : Service::LightBulb {
  SpanCharacteristic *power;
  SpanCharacteristic *brightness;
  
  int dmxChannel;  // Base DMX channel for this fixture
  
  DMX_Light(const char* name, int channel) : Service::LightBulb() {
    power = new Characteristic::On(false);
    brightness = new Characteristic::Brightness(50);
    new Characteristic::Name(name);
    
    dmxChannel = channel;
    
    // Initialize DMX channel data to 0
    dmx.writeByte(0, dmxChannel);
    
    Serial.printf("Created DMX Light on channel %d\n", dmxChannel);
  }
  
  boolean update() override {
    bool powerState = power->getVal();
    int level = brightness->getVal();
    
    // Log the state changes
    if(power->getNewVal()) {
      Serial.print("Power: ");
      Serial.println(powerState ? "ON" : "OFF");
    }
    
    if(brightness->getNewVal()) {
      Serial.print("Brightness: ");
      Serial.println(level);
    }
    
    // Convert 0-100% brightness to 0-255 DMX value
    uint8_t dmxValue = powerState ? map(level, 0, 100, 0, 255) : 0;
    
    // Set DMX channel value
    dmx.writeByte(dmxValue, dmxChannel);
    
    Serial.printf("DMX Channel %d set to %d\n", dmxChannel, dmxValue);
    
    return true;
  }
};

// RGB DMX Light fixture (using 3 DMX channels)
struct DMX_RGB_Light : Service::LightBulb {
  SpanCharacteristic *power;
  SpanCharacteristic *brightness;
  SpanCharacteristic *hue;
  SpanCharacteristic *saturation;
  
  int redChannel;    // DMX channel for red
  int greenChannel;  // DMX channel for green
  int blueChannel;   // DMX channel for blue
  
  DMX_RGB_Light(const char* name, int baseChannel) : Service::LightBulb() {
    power = new Characteristic::On(false);
    brightness = new Characteristic::Brightness(50);
    hue = new Characteristic::Hue(0);
    saturation = new Characteristic::Saturation(0);
    new Characteristic::Name(name);
    
    redChannel = baseChannel;
    greenChannel = baseChannel + 1;
    blueChannel = baseChannel + 2;
    
    // Initialize RGB channels to 0
    dmx.writeByte(0, redChannel);
    dmx.writeByte(0, greenChannel);
    dmx.writeByte(0, blueChannel);
    
    Serial.printf("Created RGB DMX Light on channels %d-%d\n", baseChannel, baseChannel+2);
  }
  
  boolean update() override {
    bool powerState = power->getVal();
    int level = brightness->getVal();
    float h = hue->getVal();
    float s = saturation->getVal();
    
    Serial.printf("Update RGB Light - Power: %d, Brightness: %d, Hue: %.1f, Saturation: %.1f\n", 
                 powerState, level, h, s);
    
    // Only update if the device is on
    if (powerState) {
      // Convert HSV to RGB
      int r, g, b;
      HSVtoRGB(h, s/100.0, level/100.0, r, g, b);
      
      // Set DMX channels
      dmx.writeByte(r, redChannel);
      dmx.writeByte(g, greenChannel);
      dmx.writeByte(b, blueChannel);
      
      Serial.printf("DMX RGB channels set to R:%d G:%d B:%d\n", r, g, b);
    } else {
      // Turn off all channels if power is off
      dmx.writeByte(0, redChannel);
      dmx.writeByte(0, greenChannel);
      dmx.writeByte(0, blueChannel);
      
      Serial.println("DMX RGB channels all off");
    }
    
    return true;
  }
  
  // Helper function to convert HSV to RGB
  void HSVtoRGB(float h, float s, float v, int &r, int &g, int &b) {
    if (s <= 0.0) {
      r = g = b = static_cast<int>(v * 255);
      return;
    }
    
    h = fmod(h, 360) / 60.0;  // Convert to [0,6)
    int i = static_cast<int>(h);
    float f = h - i;
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t = v * (1 - s * (1 - f));
    
    switch (i) {
      case 0: 
        r = static_cast<int>(v * 255);
        g = static_cast<int>(t * 255);
        b = static_cast<int>(p * 255);
        break;
      case 1:
        r = static_cast<int>(q * 255);
        g = static_cast<int>(v * 255);
        b = static_cast<int>(p * 255);
        break;
      case 2:
        r = static_cast<int>(p * 255);
        g = static_cast<int>(v * 255);
        b = static_cast<int>(t * 255);
        break;
      case 3:
        r = static_cast<int>(p * 255);
        g = static_cast<int>(q * 255);
        b = static_cast<int>(v * 255);
        break;
      case 4:
        r = static_cast<int>(t * 255);
        g = static_cast<int>(p * 255);
        b = static_cast<int>(v * 255);
        break;
      default:
        r = static_cast<int>(v * 255);
        g = static_cast<int>(p * 255);
        b = static_cast<int>(q * 255);
        break;
    }
  }
};

// Function to load WiFi credentials with clear priority:
// 1. Stored preferences (if they exist)
// 2. Environment variables (from platformio.ini)
// 3. Default fallback values from config.h
void loadWiFiCredentials() {
  // Initialize NVS if not already initialized
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    Serial.println("Erasing NVS flash...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    // Retry initialization
    err = nvs_flash_init();
  }
  
  if (err != ESP_OK) {
    Serial.printf("Error initializing NVS: %d\n", err);
    // Fall back to environment variables
    wifiSSID = HOMEDMX_WIFI_SSID;
    wifiPassword = HOMEDMX_WIFI_PASS;
    Serial.println("Using WiFi credentials from environment/defaults (NVS error)");
    return;
  }
  
  // Try to open preferences
  if (!preferences.begin(PREFERENCES_NAMESPACE, true)) {
    Serial.println("Failed to open preferences namespace");
    // Fall back to environment variables
    wifiSSID = HOMEDMX_WIFI_SSID;
    wifiPassword = HOMEDMX_WIFI_PASS;
    Serial.println("Using WiFi credentials from environment/defaults (preferences error)");
    return;
  }
  
  // Check if credentials exist in preferences
  if (preferences.isKey(PREF_WIFI_SSID) && preferences.isKey(PREF_WIFI_PASS)) {
    wifiSSID = preferences.getString(PREF_WIFI_SSID, "");
    wifiPassword = preferences.getString(PREF_WIFI_PASS, "");
    
    // Validate that we got non-empty strings
    if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
      Serial.println("WiFi credentials loaded from preferences");
    } else {
      // Fall back to environment variables
      wifiSSID = HOMEDMX_WIFI_SSID;
      wifiPassword = HOMEDMX_WIFI_PASS;
      Serial.println("Using WiFi credentials from environment/defaults (empty stored values)");
    }
  } else {
    // Use the credentials defined in platformio.ini or the fallback defaults from config.h
    wifiSSID = HOMEDMX_WIFI_SSID;
    wifiPassword = HOMEDMX_WIFI_PASS;
    Serial.println("Using WiFi credentials from environment/defaults (no stored values)");
  }
  
  preferences.end();
}

// Function to save WiFi credentials to preferences after successful connection
void saveWiFiCredentials() {
  // Initialize NVS if not already initialized
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    Serial.println("Erasing NVS flash...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    // Retry initialization
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  
  // Begin preferences in read-write mode
  preferences.begin(PREFERENCES_NAMESPACE, false);
  
  // Check if we already have these credentials stored
  bool hasStoredSSID = preferences.isKey(PREF_WIFI_SSID);
  bool hasStoredPass = preferences.isKey(PREF_WIFI_PASS);
  String storedSSID = preferences.getString(PREF_WIFI_SSID, "");
  String storedPass = preferences.getString(PREF_WIFI_PASS, "");
  
  // Only save if credentials are different from what's stored
  if (!hasStoredSSID || !hasStoredPass || storedSSID != wifiSSID || storedPass != wifiPassword) {
    preferences.putString(PREF_WIFI_SSID, wifiSSID);
    preferences.putString(PREF_WIFI_PASS, wifiPassword);
    Serial.println("WiFi credentials saved to preferences");
  } else {
    Serial.println("WiFi credentials already stored in preferences");
  }
  
  preferences.end();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n*** HomeDMX Starting ***\n");
  
  // Load WiFi credentials from persistent storage
  loadWiFiCredentials();
  
  // Set up the status LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  // Set up the control button
  pinMode(CONTROL_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize DMX shield
  dmx.begin(Serial1, DMX_ENABLE_PIN, DMX_CHANNELS);
  
  // Set direction to output
  dmx.setComDir(DMX_WRITE_DIR);
  
  Serial.println("DMX Shield initialized");
  
  // Set up HomeSpan
  Serial.println("Setting up HomeDMX...");
  
  // Configure HomeSpan
  homeSpan.setStatusPin(STATUS_LED_PIN);
  homeSpan.setControlPin(CONTROL_BUTTON_PIN);
  homeSpan.setPairingCode(HOMEDMX_SETUP_CODE);
  homeSpan.setWifiCredentials(wifiSSID.c_str(), wifiPassword.c_str());
  
  // Initialize HomeSpan
  homeSpan.begin(Category::Lighting, DEVICE_NAME, DEVICE_MODEL, FIRMWARE_VERSION);
  homeSpan.enableOTA(); // Enable Over-the-Air updates
  
  // Monitor WiFi Events
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      Serial.print("Connected to WiFi! IP address: ");
      Serial.println(WiFi.localIP());
      
      // Save credentials to preferences if they came from environment/defaults
      saveWiFiCredentials();
    }
  });
  
  // Create the bridge device
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Name(DEVICE_MODEL);
      new Characteristic::Manufacturer(DEVICE_MANUFACTURER);
      new Characteristic::SerialNumber("123-ABC");
      new Characteristic::Model(DEVICE_MODEL);
      new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
      new Characteristic::Identify();
  
  // Simple DMX light (single channel) - starting at DMX channel 1
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("DMX Light 1");
      new Characteristic::Manufacturer(DEVICE_MANUFACTURER);
      new Characteristic::SerialNumber("DMX-001");
      new Characteristic::Model("Single Channel");
      new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
    
    new DMX_Light("DMX Light 1", 1);
  
  // RGB DMX light - starting at DMX channel 5
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("RGB Light");
      new Characteristic::Manufacturer(DEVICE_MANUFACTURER);
      new Characteristic::SerialNumber("DMX-RGB-001");
      new Characteristic::Model("RGB Light");
      new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
    
    new DMX_RGB_Light("RGB Light", 5);
}

void loop() {
  homeSpan.poll();
  
  // Update DMX shield - this will send out DMX data
  dmx.update();
  
  // Add HomeKit reset functionality
  static unsigned long buttonPressStartTime = 0;
  static bool buttonWasPressed = false;
  
  // Check if button is pressed (LOW when pressed, as it uses INPUT_PULLUP)
  if (digitalRead(CONTROL_BUTTON_PIN) == LOW) {
    // Button is currently pressed
    if (!buttonWasPressed) {
      // This is the start of a new press
      buttonPressStartTime = millis();
      buttonWasPressed = true;
      Serial.println("Control button pressed - hold for 10 seconds to reset HomeKit pairing");
    } else {
      // Continuing to hold the button
      if (millis() - buttonPressStartTime > 10000) { // 10 seconds
        // Perform factory reset
        Serial.println("\n*** PERFORMING FACTORY RESET ***");
        Serial.println("Removing all HomeKit pairings and resetting configuration...");
        
        // Reset HomeKit data using nvs_flash_erase() to erase all data
        nvs_flash_erase();
        nvs_flash_init();
        
        // Flash LED to indicate reset is complete
        for (int i = 0; i < 10; i++) {
          digitalWrite(STATUS_LED_PIN, HIGH);
          delay(100);
          digitalWrite(STATUS_LED_PIN, LOW);
          delay(100);
        }
        
        Serial.println("Factory reset complete. Restarting device...");
        delay(1000);
        ESP.restart();  // Restart the ESP32
      }
    }
  } else {
    // Button is released
    buttonWasPressed = false;
  }
  
  // Add a small delay to prevent 100% CPU usage
  delay(10);
} 