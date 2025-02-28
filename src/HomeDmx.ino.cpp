# 1 "/var/folders/rx/g5c2w2x531q4lltfkz71b2pc0000gn/T/tmpbptr32mt"
#include <Arduino.h>
# 1 "/Users/george/HomeDMX/src/HomeDmx.ino"
# 32 "/Users/george/HomeDMX/src/HomeDmx.ino"
#include "HomeSpan.h"
#include "config.h"
#include <Preferences.h>
#include <SparkFunDMX.h>


Preferences preferences;
String wifiSSID;
String wifiPassword;


SparkFunDMX dmx;


const int STATUS_LED_PIN = 13;

const int CONTROL_BUTTON_PIN = 0;

const int DMX_ENABLE_PIN = 21;


struct DMX_Light : Service::LightBulb {
  SpanCharacteristic *power;
  SpanCharacteristic *brightness;

  int dmxChannel;

  DMX_Light(const char* name, int channel) : Service::LightBulb() {
    power = new Characteristic::On(false);
    brightness = new Characteristic::Brightness(50);
    new Characteristic::Name(name);

    dmxChannel = channel;


    dmx.writeByte(0, dmxChannel);

    Serial.printf("Created DMX Light on channel %d\n", dmxChannel);
  }

  boolean update() override {
    bool powerState = power->getVal();
    int level = brightness->getVal();


    if(power->getNewVal()) {
      Serial.print("Power: ");
      Serial.println(powerState ? "ON" : "OFF");
    }

    if(brightness->getNewVal()) {
      Serial.print("Brightness: ");
      Serial.println(level);
    }


    uint8_t dmxValue = powerState ? map(level, 0, 100, 0, 255) : 0;


    dmx.writeByte(dmxValue, dmxChannel);

    Serial.printf("DMX Channel %d set to %d\n", dmxChannel, dmxValue);

    return true;
  }
};


struct DMX_RGB_Light : Service::LightBulb {
  SpanCharacteristic *power;
  SpanCharacteristic *brightness;
  SpanCharacteristic *hue;
  SpanCharacteristic *saturation;

  int redChannel;
  int greenChannel;
  int blueChannel;

  DMX_RGB_Light(const char* name, int baseChannel) : Service::LightBulb() {
    power = new Characteristic::On(false);
    brightness = new Characteristic::Brightness(50);
    hue = new Characteristic::Hue(0);
    saturation = new Characteristic::Saturation(0);
    new Characteristic::Name(name);

    redChannel = baseChannel;
    greenChannel = baseChannel + 1;
    blueChannel = baseChannel + 2;


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


    if (powerState) {

      int r, g, b;
      HSVtoRGB(h, s/100.0, level/100.0, r, g, b);


      dmx.writeByte(r, redChannel);
      dmx.writeByte(g, greenChannel);
      dmx.writeByte(b, blueChannel);

      Serial.printf("DMX RGB channels set to R:%d G:%d B:%d\n", r, g, b);
    } else {

      dmx.writeByte(0, redChannel);
      dmx.writeByte(0, greenChannel);
      dmx.writeByte(0, blueChannel);

      Serial.println("DMX RGB channels all off");
    }

    return true;
  }


  void HSVtoRGB(float h, float s, float v, int &r, int &g, int &b) {
    if (s <= 0.0) {
      r = g = b = static_cast<int>(v * 255);
      return;
    }

    h = fmod(h, 360) / 60.0;
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
void loadWiFiCredentials();
void saveWiFiCredentials();
void setup();
void loop();
#line 215 "/Users/george/HomeDMX/src/HomeDmx.ino"
void loadWiFiCredentials() {
  preferences.begin(PREFERENCES_NAMESPACE, true);


  if (preferences.isKey(PREF_WIFI_SSID) && preferences.isKey(PREF_WIFI_PASS)) {
    wifiSSID = preferences.getString(PREF_WIFI_SSID, "");
    wifiPassword = preferences.getString(PREF_WIFI_PASS, "");
    Serial.println("WiFi credentials loaded from preferences");
  } else {

    wifiSSID = HOMEDMX_WIFI_SSID;
    wifiPassword = HOMEDMX_WIFI_PASS;
    Serial.println("Using WiFi credentials from environment/defaults");
  }

  preferences.end();
}


void saveWiFiCredentials() {

  if (!preferences.isKey(PREF_WIFI_SSID) || !preferences.isKey(PREF_WIFI_PASS)) {
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putString(PREF_WIFI_SSID, wifiSSID);
    preferences.putString(PREF_WIFI_PASS, wifiPassword);
    preferences.end();
    Serial.println("WiFi credentials saved to preferences");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\n*** HomeDMX Starting ***\n");


  loadWiFiCredentials();


  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);


  pinMode(CONTROL_BUTTON_PIN, INPUT_PULLUP);


  dmx.begin(Serial1, DMX_ENABLE_PIN, DMX_CHANNELS);


  dmx.setComDir(DMX_WRITE_DIR);

  Serial.println("DMX Shield initialized");


  Serial.println("Setting up HomeDMX...");


  homeSpan.setStatusPin(STATUS_LED_PIN);
  homeSpan.setControlPin(CONTROL_BUTTON_PIN);
  homeSpan.setPairingCode(SETUP_CODE);
  homeSpan.setWifiCredentials(wifiSSID.c_str(), wifiPassword.c_str());


  homeSpan.begin(Category::Lighting, DEVICE_NAME, DEVICE_MODEL, FIRMWARE_VERSION);
  homeSpan.enableOTA();


  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      Serial.print("Connected to WiFi! IP address: ");
      Serial.println(WiFi.localIP());


      saveWiFiCredentials();
    }
  });


  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Name(DEVICE_MODEL);
      new Characteristic::Manufacturer(DEVICE_MANUFACTURER);
      new Characteristic::SerialNumber("123-ABC");
      new Characteristic::Model(DEVICE_MODEL);
      new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
      new Characteristic::Identify();


  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("DMX Light 1");
      new Characteristic::Manufacturer(DEVICE_MANUFACTURER);
      new Characteristic::SerialNumber("DMX-001");
      new Characteristic::Model("Single Channel");
      new Characteristic::FirmwareRevision(FIRMWARE_VERSION);

    new DMX_Light("DMX Light 1", 1);


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


  dmx.update();


  delay(10);
}