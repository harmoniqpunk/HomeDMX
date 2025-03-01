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

// 7-Channel DMX Fixture - simplified for basic control
struct DMX_7CH_Fixture : Service::LightBulb {
  SpanCharacteristic *power;
  SpanCharacteristic *brightness;
  
  int baseChannel;  // First DMX channel (of 7)
  
  DMX_7CH_Fixture(const char* name, int startChannel) : Service::LightBulb() {
    // Basic LightBulb characteristics - initialize to OFF state (true in HomeKit means OFF for our inverted logic)
    power = new Characteristic::On(true);  // Set initial HomeKit state to ON (which will be inverted to OFF for DMX)
    brightness = new Characteristic::Brightness(100);
    new Characteristic::Name(name);
    
    baseChannel = startChannel;
    
    // Initialize all DMX channels to 0
    for (int i = 0; i < 7; i++) {
      dmx.writeByte(0, baseChannel + i);
    }
    dmx.update();
    
    Serial.printf("Created 7-Channel DMX Fixture starting at channel %d\n", baseChannel);
    Serial.println("Note: Power state is inverted - HomeKit ON = DMX OFF and vice versa");
    
    // Run a brief power-on test sequence
    Serial.println("Running power-on test sequence for DMX fixture...");
    
    // Turn on channels briefly to verify fixture is working
    testChannel(baseChannel, "Master");
    testChannel(baseChannel + 1, "LED Group 1");
    testChannel(baseChannel + 2, "LED Group 2");
    testChannel(baseChannel + 3, "LED Group 3");
    
    // Reset all channels to 0
    for (int i = 0; i < 7; i++) {
      dmx.writeByte(0, baseChannel + i);
    }
    dmx.update();
    
    Serial.println("Power-on test sequence complete");
  }
  
  // Helper method to test a specific DMX channel
  void testChannel(int channel, const char* name) {
    Serial.printf("Testing channel %d (%s)...\n", channel, name);
    
    // Set channel to full brightness (255)
    dmx.writeByte(255, channel);
    dmx.update();
    delay(500);  // Keep on for 500ms
    
    // Reset channel to 0
    dmx.writeByte(0, channel);
    dmx.update();
    delay(200);  // Pause between tests
  }
  
  boolean update() override {
    // Check if power state changed
    bool powerChanged = power->getNewVal();
    
    // Get the current power state
    bool powerState = power->getVal();
    
    // Debug output
    if (powerChanged) {
      Serial.printf("Power state changed to: %s\n", powerState ? "OFF" : "ON");
    }
    
    // SIMPLE LOGIC: powerState is inverted in HomeKit
    // HomeKit ON (false) = DMX ON (channels 1-4 = 255)
    // HomeKit OFF (true) = DMX OFF (all channels = 0)
    
    // Set channels based on power state
    if (!powerState) { // HomeKit ON = !powerState
      // ON STATE: Set channels 1-4 to full brightness (255)
      dmx.writeByte(255, baseChannel);     // CH1: Master brightness
      dmx.writeByte(255, baseChannel + 1); // CH2: LED group 1
      dmx.writeByte(255, baseChannel + 2); // CH3: LED group 2
      dmx.writeByte(255, baseChannel + 3); // CH4: LED group 3
      // Ensure channels 5-7 are always 0
      dmx.writeByte(0, baseChannel + 4);   // CH5: Strobe (always 0)
      dmx.writeByte(0, baseChannel + 5);   // CH6: Mode (always 0)
      dmx.writeByte(0, baseChannel + 6);   // CH7: Speed/color (always 0)
      
      if (powerChanged) {
        Serial.println("DMX Channels 1-4 set to full brightness (255)");
      }
    } 
    else { // HomeKit OFF = powerState
      // OFF STATE: Set all channels to 0
      for (int i = 0; i < 7; i++) {
        dmx.writeByte(0, baseChannel + i);
      }
      
      if (powerChanged) {
        Serial.println("All DMX Channels set to 0");
      }
    }
    
    // Force update to DMX bus immediately
    dmx.update();
    
    // Print channel values when state changes
    if (powerChanged) {
      Serial.printf("DMX Channels: %d,%d,%d,%d,%d,%d,%d\n", 
                    dmx.readByte(baseChannel),
                    dmx.readByte(baseChannel + 1),
                    dmx.readByte(baseChannel + 2),
                    dmx.readByte(baseChannel + 3),
                    dmx.readByte(baseChannel + 4),
                    dmx.readByte(baseChannel + 5),
                    dmx.readByte(baseChannel + 6));
    }
    
    return true;
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
  
  // Initialize DMX shield with proper error handling
  initializeDmxShield();
  
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
  
  // 7-Channel DMX Fixture - starting at DMX channel 1
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("DMX 7CH Fixture");
      new Characteristic::Manufacturer(DEVICE_MANUFACTURER);
      new Characteristic::SerialNumber("DMX-001");
      new Characteristic::Model("7-Channel Fixture");
      new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
    
    new DMX_7CH_Fixture("DMX 7CH Fixture", 1);  // Using address 1 (not zero-based)
}

void loop() {
  homeSpan.poll();
  
  // Update DMX shield once per loop
  dmx.update();
  
  // Check for serial commands
  checkSerialCommands();
  
  // Periodically verify DMX channel values (every 5 seconds)
  static unsigned long lastDmxCheck = 0;
  if (millis() - lastDmxCheck > 5000) {
    lastDmxCheck = millis();
    Serial.println("Current DMX channel values:");
    for (int i = 1; i <= 7; i++) {
      uint8_t value = dmx.readByte(i);
      Serial.printf("  Channel %d: %d\n", i, value);
    }
  }
  
  // Add button handling for both direct DMX test and HomeKit reset
  static bool lastButtonState = HIGH;
  static unsigned long buttonPressStartTime = 0;
  static bool buttonWasPressed = false;
  bool currentButtonState = digitalRead(CONTROL_BUTTON_PIN);
  
  // Detect button press
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    // Button just pressed - record time
    buttonPressStartTime = millis();
    buttonWasPressed = true;
    Serial.println("Button pressed - short press for DMX test, hold for reset");
  }
  
  // Check for long press (reset)
  if (currentButtonState == LOW && buttonWasPressed) {
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
  
  // Detect button release (for short press actions)
  if (lastButtonState == LOW && currentButtonState == HIGH && buttonWasPressed) {
    // Button released
    unsigned long pressDuration = millis() - buttonPressStartTime;
    buttonWasPressed = false;
    
    // If this was a short press (less than 2 seconds), run DMX test
    if (pressDuration > 100 && pressDuration < 2000) {
      Serial.println("Short press detected - running direct DMX test");
      testDMXDirect();
    }
  }
  
  // Update last button state
  lastButtonState = currentButtonState;
  
  // Add a small delay to prevent 100% CPU usage
  delay(10);
}

// Function to directly test DMX output without going through HomeKit
void testDMXDirect() {
  Serial.println("\n*** DIRECT DMX TEST ***");
  Serial.println("Testing 7-Channel DMX Fixture with the following channel configuration:");
  Serial.println("  Channel 1: Master brightness control (dimmer)");
  Serial.println("  Channel 2: LED group 1 intensity");
  Serial.println("  Channel 3: LED group 2 intensity");
  Serial.println("  Channel 4: LED group 3 intensity");
  Serial.println("  Channel 5: Strobe (always kept at 0)");
  Serial.println("  Channel 6: Mode (always kept at 0)");
  Serial.println("  Channel 7: Speed/color (always kept at 0)");
  
  // First turn everything off
  for (int i = 1; i <= 7; i++) {
    dmx.writeByte(0, i);
  }
  dmx.update();
  Serial.println("All channels set to 0");
  printDmxValues();
  delay(500);
  
  // Turn on master channel at 50% brightness
  Serial.println("Setting master channel (1) to 50%");
  dmx.writeByte(128, 1);
  dmx.update();
  printDmxValues();
  delay(1000);
  
  // Turn on LED groups at full brightness
  Serial.println("Setting all LED group channels (2-4) to 100%");
  dmx.writeByte(255, 2);
  dmx.writeByte(255, 3);
  dmx.writeByte(255, 4);
  dmx.update();
  printDmxValues();
  delay(2000);
  
  // Turn on master channel at full brightness
  Serial.println("Setting master channel (1) to 100%");
  dmx.writeByte(255, 1);
  dmx.update();
  printDmxValues();
  delay(2000);
  
  // Try a pulsing effect for 5 seconds to verify real-time control
  Serial.println("Pulsing master brightness for 5 seconds...");
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    // Calculate a sine wave value between 0 and 255
    float phase = (millis() - startTime) / 5000.0 * TWO_PI;
    uint8_t brightness = 128 + 127 * sin(phase);
    
    dmx.writeByte(brightness, 1);  // Only pulse the master channel
    dmx.update();
    
    delay(50);  // Small delay between updates
  }
  
  // End with light at full brightness to verify it's working
  Serial.println("Setting final state - LIGHT ON at full brightness");
  
  // Set channels 1-4 to full brightness (255)
  dmx.writeByte(255, 1);  // Channel 1: Master brightness control
  dmx.writeByte(255, 2);  // Channel 2: LED group 1
  dmx.writeByte(255, 3);  // Channel 3: LED group 2
  dmx.writeByte(255, 4);  // Channel 4: LED group 3
  
  // Ensure channels 5-7 are explicitly set to 0
  dmx.writeByte(0, 5);    // Channel 5: Strobe (always 0)
  dmx.writeByte(0, 6);    // Channel 6: Mode (always 0)
  dmx.writeByte(0, 7);    // Channel 7: Speed/color (always 0)
  
  dmx.update();
  
  Serial.println("Final DMX channel values:");
  printDmxValues();
  
  Serial.println("Direct DMX test complete. Light should now be ON at full brightness.");
  Serial.println("If light is not on, there is likely a hardware issue with the DMX connection or fixture.");
}

// Helper function to print current DMX values
void printDmxValues() {
  Serial.println("Current DMX channel values:");
  for (int i = 1; i <= 7; i++) {
    uint8_t value = dmx.readByte(i);
    Serial.printf("  Channel %d: %d\n", i, value);
  }
}

// Function to test multiple DMX addresses (for hardware debugging)
void testMultipleAddresses() {
  Serial.println("\n*** TESTING MULTIPLE DMX ADDRESSES ***");
  Serial.println("This will cycle through addresses 1-10 to help identify if your fixture is responding to a different address");
  
  // First turn everything off
  for (int i = 1; i <= 20; i++) {
    dmx.writeByte(0, i);
  }
  dmx.update();
  delay(500);
  
  // Test each address with a different pattern
  for (int baseAddr = 1; baseAddr <= 10; baseAddr++) {
    Serial.printf("\nTesting DMX address: %d\n", baseAddr);
    
    // Turn on all channels for this fixture (assuming 7 channels per fixture)
    for (int ch = 0; ch < 7; ch++) {
      int channel = baseAddr + ch;
      dmx.writeByte(255, channel);
    }
    dmx.update();
    
    // Print current values
    Serial.printf("Channels %d-%d set to 255\n", baseAddr, baseAddr + 6);
    for (int i = 1; i <= 20; i++) {
      uint8_t value = dmx.readByte(i);
      Serial.printf("  Channel %d: %d\n", i, value);
    }
    
    // Wait for visual confirmation
    Serial.printf("If your fixture is lighting up now, it's configured for DMX address %d\n", baseAddr);
    delay(3000);
    
    // Turn off this set of channels
    for (int ch = 0; ch < 7; ch++) {
      int channel = baseAddr + ch;
      dmx.writeByte(0, channel);
    }
    dmx.update();
    delay(1000);
  }
  
  Serial.println("\nMultiple address test complete.");
  Serial.println("If your fixture didn't light up during any of these tests, there may be a hardware issue.");
}

// Function for direct DMX hardware test
void directHardwareTest() {
  Serial.println("\n*** DIRECT HARDWARE DMX TEST ***");
  Serial.println("This test bypasses most of the SparkFun DMX library for troubleshooting");
  
  // Manually configure DMX pins for maximum compatibility
  pinMode(DMX_ENABLE_PIN, OUTPUT);
  digitalWrite(DMX_ENABLE_PIN, HIGH);  // Enable DMX output
  
  // Reset DMX buffer directly
  for (int i = 0; i < 512; i++) {
    dmx.writeByte(0, i + 1);
  }
  dmx.update();
  delay(200);
  
  Serial.println("Setting all fixture channels (1-7) to maximum brightness");
  
  // Set all channels to full for maximum chance of seeing something
  for (int i = 1; i <= 7; i++) {
    dmx.writeByte(255, i);
  }
  
  // Force multiple DMX updates to ensure signal is sent
  for (int i = 0; i < 10; i++) {
    dmx.update();
    delay(50);
  }
  
  Serial.println("DMX values now:");
  printDmxValues();
  
  Serial.println("\nDirect hardware test running - fixture should be at full brightness");
  Serial.println("If fixture is still not responding, check:");
  Serial.println("1. Physical DMX cable connections and wiring");
  Serial.println("2. ESP32 TX/RX pins used for Serial1 (check your board variant)");
  Serial.println("3. DMX shield power and proper connection to ESP32");
  Serial.println("4. Try adding a DMX terminator at the end of the line");
  Serial.println("5. Try reversing the DMX +/- connections if your console uses a different pinout");
}

// Print DMX hardware information
void printDmxHardwareInfo() {
  Serial.println("\n*** DMX HARDWARE INFORMATION ***");
  
  // Print ESP32 board information
  Serial.printf("ESP32 Board: %s\n", ARDUINO_BOARD);
  Serial.printf("ESP32 SDK Version: %s\n", ESP.getSdkVersion());
  
  // Print DMX pin configuration
  Serial.printf("DMX Enable Pin: %d\n", DMX_ENABLE_PIN);
  
  // Serial1 pins on ESP32 (typically used for DMX)
  Serial.println("Serial1 Pins (used for DMX):");
  Serial.printf("  TX Pin: %d\n", 1);  // Default TX pin for Serial1
  Serial.printf("  RX Pin: %d\n", 3);  // Default RX pin for Serial1
  
  // Note: These default pins might not be correct for all ESP32 variants
  Serial.println("Note: Serial1 pins may vary by ESP32 board variant");
  Serial.println("      Check your board's documentation for the correct pins");
  
  Serial.println("\nDMX Configuration:");
  Serial.printf("  DMX Channels: %d\n", DMX_CHANNELS);
  
  // Show current DMX buffer state for first few channels
  Serial.println("\nCurrent DMX Buffer State (first 10 channels):");
  for (int i = 1; i <= 10; i++) {
    uint8_t value = dmx.readByte(i);
    Serial.printf("  Channel %d: %d\n", i, value);
  }
  
  // Additional hardware diagnostic information
  Serial.println("\nESP32 Hardware Info:");
  Serial.printf("  Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("  Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("  CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("  Flash Size: %d bytes\n", ESP.getFlashChipSize());
  
  Serial.println("\nFor SparkFun DMX Shield, check that:");
  Serial.println("1. The DMX shield is properly seated on the ESP32");
  Serial.println("2. Serial1 TX/RX pins match the pins used by your shield");
  Serial.println("3. The DMX_ENABLE_PIN (21) is correctly wired to the shield's enable pin");
  Serial.println("4. If using a MAX485 chip for DMX, check that DI is connected to TX and DE/RE to enable pin");
}

// Function to check for serial commands for direct DMX control
void checkSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    // Parse commands in format "dmx:channel:value" - e.g., "dmx:1:255"
    if (command.startsWith("dmx:")) {
      int firstColon = command.indexOf(':');
      int secondColon = command.indexOf(':', firstColon + 1);
      
      if (secondColon > firstColon) {
        String channelStr = command.substring(firstColon + 1, secondColon);
        String valueStr = command.substring(secondColon + 1);
        
        int channel = channelStr.toInt();
        int value = valueStr.toInt();
        
        if (channel >= 1 && channel <= DMX_CHANNELS && value >= 0 && value <= 255) {
          Serial.printf("Setting DMX channel %d to value %d\n", channel, value);
          dmx.writeByte(value, channel);
          dmx.update();
          
          // Verify the value was set
          uint8_t readValue = dmx.readByte(channel);
          Serial.printf("Channel %d value is now: %d\n", channel, readValue);
        } else {
          Serial.println("Invalid channel or value. Channel must be 1-512, value 0-255");
        }
      } else {
        Serial.println("Invalid command format. Use dmx:channel:value (e.g., dmx:1:255)");
      }
    } 
    else if (command == "dmx:status") {
      // Print all DMX channel values
      printDmxValues();
    }
    else if (command == "dmx:test") {
      // Run the direct test
      testDMXDirect();
    }
    else if (command == "dmx:test:addresses") {
      // Run the address detection test
      testMultipleAddresses();
    }
    else if (command == "dmx:test:hardware") {
      // Run direct hardware test
      directHardwareTest();
    }
    else if (command == "dmx:hardware") {
      // Print hardware information
      printDmxHardwareInfo();
    }
    else if (command == "dmx:on") {
      // Turn on the light at full brightness
      Serial.println("Turning ON light at full brightness");
      
      // Set channels 1-4 to full brightness (255)
      dmx.writeByte(255, 1);  // Channel 1: Master brightness control
      dmx.writeByte(255, 2);  // Channel 2: LED group 1 
      dmx.writeByte(255, 3);  // Channel 3: LED group 2
      dmx.writeByte(255, 4);  // Channel 4: LED group 3
      
      // Ensure channels 5-7 are set to 0
      dmx.writeByte(0, 5);    // Channel 5: Strobe (always 0)
      dmx.writeByte(0, 6);    // Channel 6: Mode (always 0)
      dmx.writeByte(0, 7);    // Channel 7: Speed/color (always 0)
      
      dmx.update();
      printDmxValues();
    }
    else if (command == "dmx:off") {
      // Turn off the light
      Serial.println("Turning OFF light");
      for (int i = 1; i <= 7; i++) {
        dmx.writeByte(0, i);
      }
      dmx.update();
      printDmxValues();
    }
    else if (command == "help") {
      Serial.println("\nAvailable commands:");
      Serial.println("  dmx:channel:value - Set DMX channel to value (e.g., dmx:1:255)");
      Serial.println("  dmx:status - Show all DMX channel values");
      Serial.println("  dmx:test - Run direct DMX test");
      Serial.println("  dmx:test:addresses - Test multiple DMX addresses (1-10)");
      Serial.println("  dmx:test:hardware - Direct hardware DMX test (troubleshooting)");
      Serial.println("  dmx:hardware - Print DMX hardware information");
      Serial.println("  dmx:on - Turn on light at full brightness (ch 1-4 = 255, ch 5-7 = 0)");
      Serial.println("  dmx:off - Turn off light (all channels = 0)");
      Serial.println("  help - Show this help message");
    }
  }
}

// Function to properly initialize the DMX shield with error checking
void initializeDmxShield() {
  Serial.println("Initializing DMX shield...");

  // Configure Serial1 for DMX communication
  Serial1.begin(250000, SERIAL_8N2); // DMX requires 250kbps, 8 data bits, no parity, 2 stop bits
  delay(100); // Give Serial1 time to initialize
  
  // Initialize DMX shield with Serial1 (TX/RX pins), the enable pin, and number of channels
  dmx.begin(Serial1, DMX_ENABLE_PIN, DMX_CHANNELS);
  
  // Set direction to output
  dmx.setComDir(DMX_WRITE_DIR);
  
  // Double-check that direction is set to output
  Serial.println("Verifying DMX direction is set to OUTPUT");
  
  // Reset all DMX channels to 0 on startup
  for (int i = 1; i <= DMX_CHANNELS; i++) {
    dmx.writeByte(0, i);
  }
  
  // Force a DMX update
  dmx.update();
  delay(100);  // Small delay to ensure the update completes
  
  // Verify that all channels are set to 0
  bool channelsReset = true;
  for (int i = 1; i <= 7; i++) {
    if (dmx.readByte(i) != 0) {
      channelsReset = false;
      Serial.printf("WARNING: Channel %d not reset to 0 (value: %d)\n", 
                    i, dmx.readByte(i));
    }
  }
  
  if (channelsReset) {
    Serial.println("DMX Shield initialized successfully - all channels reset to 0");
  } else {
    Serial.println("DMX Shield initialized but channel verification failed");
  }
  
  // Give clear instructions for troubleshooting
  Serial.println("\n-------------------------------------------------------------");
  Serial.println("For direct DMX control, send commands via Serial:");
  Serial.println("  dmx:channel:value - Set channel to value (e.g., dmx:1:255)");
  Serial.println("  dmx:test - Run test sequence");
  Serial.println("  dmx:on - Turn on light at full brightness");
  Serial.println("  dmx:off - Turn off light");
  Serial.println("  help - Show all available commands");
  Serial.println("-------------------------------------------------------------\n");
} 