#include <ESP8266WiFi.h>
#include <ArduinoHA.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const uint16_t kRecvPin = 4; //D2
const uint16_t kIrLed = 14; //D5
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 15;
const uint16_t kMinUnknownSize = 12;

IRsend irsend(kIrLed);
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results;

#define BROKER_ADDR     IPAddress(192,168,178,100)
#define WIFI_SSID       "WLAN_SSID"
#define WIFI_PASSWORD   "secret"
#define IR_REPEAT          15

#define DEVICE_NAME   "Fan"
#define FAN_NAME   "Ventilator"
#define FANLIGHT_NAME   "Light"

bool ir_stop = 0;
int ir_wait = 0;
bool verbose = 0; 

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
HAFan fan("ventilation", HAFan::SpeedsFeature);
HASwitch fanlight("light");

void onStateCommand(bool state, HAFan* sender) {
    ir_stop = 1;
    Serial.print("State: ");
    Serial.println(state);

    if (state == 0 ) {
      Serial.println("Fan Off");
      irsend.sendSymphony(0xC10, 12, IR_REPEAT);

    }
    else 
    {
      Serial.println("Fan On");
      int speed = fan.getCurrentSpeed();

      if (speed == 0) {
        Serial.println("Speed 0");
        irsend.sendSymphony(0xC10, 12, IR_REPEAT);
      }

      if (speed > 0 && speed <= 49) {
        Serial.println("Speed 1");
        irsend.sendSymphony(0xC01, 12, IR_REPEAT);
        fan.setSpeed(speed);
      }

      if (speed >= 50 && speed < 100) {
        Serial.println("Speed 2");
        irsend.sendSymphony(0xC04, 12, IR_REPEAT);
      }

      if (speed == 100) {
        Serial.println("Speed 3");
        irsend.sendSymphony(0xC43, 12, IR_REPEAT);
      }

    }
    fan.setState(state);
}

void onSpeedCommand(uint16_t speed, HAFan* sender) {
    ir_stop = 1;
    Serial.print("Speed percentage: ");
    Serial.println(speed);

    if (speed == 0) {
      Serial.println("Speed 0");
      irsend.sendSymphony(0xC10, 12, IR_REPEAT);
      fan.setState(0);
    }

    if (speed > 0 && speed <= 49) {
      Serial.println("Speed 1");
      irsend.sendSymphony(0xC01, 12, IR_REPEAT);
      fan.setSpeed(speed);
      fan.setState(1);
    }

    if (speed >= 50 && speed < 100) {
      Serial.println("Speed 2");
      irsend.sendSymphony(0xC04, 12, IR_REPEAT);
      fan.setState(1);
    }

    if (speed == 100) {
      Serial.println("Speed 3");
      irsend.sendSymphony(0xC43, 12, IR_REPEAT);
      fan.setState(1);
    }

    fan.setSpeed(speed);
}


void onSwitchStateChanged(bool state, HASwitch* s)
{

  if (verbose)  Serial.println("Disabling IR receiver");
  ir_stop = 1;
    
  if (state == 0) {
    Serial.println("Light off");
    irsend.sendSymphony(0xC20, 12, 3);
  }
  if (state == 1) {
    Serial.println("Light on");
    irsend.sendSymphony(0xC08, 12, 3);
  }
  fanlight.setState(state);
}

void setup() {
  Serial.begin(9600);
  irsend.begin();

  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
  }

  Serial.println();
  Serial.println("Connected to the network");

  device.setName(DEVICE_NAME);
  device.setSoftwareVersion("1.0.0");

  fan.setName(FAN_NAME);
  fan.setRetain(true);
  fan.setSpeedRangeMin(1);
  fan.setSpeedRangeMax(100);

  fan.onStateCommand(onStateCommand);
  fan.onSpeedCommand(onSpeedCommand);

  fanlight.setIcon("mdi:lightbulb");
  fanlight.setName(FANLIGHT_NAME);

  fanlight.onCommand(onSwitchStateChanged);

  mqtt.begin(BROKER_ADDR);

  #if DECODE_HASH
        irrecv.setUnknownThreshold(kMinUnknownSize);
  #endif
    
  irrecv.enableIRIn();

  ArduinoOTA.setHostname(DEVICE_NAME);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop() {
    
    if (ir_stop == 1) {
      ir_wait++;

      if (ir_wait > 2000) { 
        if (verbose) Serial.println("Enabling IR receiver");
        ir_stop = 0;
        ir_wait = 0;
      }
    }

    if (irrecv.decode(&results) && ir_stop == 0) {
      // Serial.print(resultToHumanReadableBasic(&results));
      // Serial.println(results.value, HEX);
      // Serial.println(results.value);
      Serial.print("IR: ");
      switch (results.value) {
        case 3088:
          Serial.println("Fan Speed 0");
          fan.setState(0);
          break;
        case 3073:
          Serial.println("Fan Speed 1");
          fan.setState(1);
          fan.setSpeed(25);
          break;
        case 3076:
          Serial.println("Fan Speed 2");
          fan.setState(1);
          fan.setSpeed(75);
          break;
        case 3139:
          Serial.println("Fan Speed 3");
          fan.setState(1);
          fan.setSpeed(100);
          break;
        case 3080:
          Serial.println("Fan Light On");
          fanlight.setState(1);
          break;        
        case 3104:
          Serial.println("Fan Light Off");
          fanlight.setState(0);
          break;
        default:
          Serial.println("IR code not known");
          break;
      }
      yield();
    }
    ArduinoOTA.handle();
    mqtt.loop();
}