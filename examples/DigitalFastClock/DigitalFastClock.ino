/*
 * Fast Clock
 *
 * This sketch implements a Digital Fast Time clock.
 *
 * The hardware is use is an Adafruit Huzzah32 (ESP32 based),
 * with a 7-segment display feather wing.
 *
 * Huzzah32 - https://www.adafruit.com/product/3405
 * Display  - https://www.adafruit.com/product/3108
 *
 * This combination makes a battery powered portable fast clock for your
 * layout.  Or power it with 5V and you have a permanent installation.
 *
 * Copyright 2018 by david d zuhn <zoo@blueknobby.com>
 *
 * This work is licensed under the Creative Commons Attribution-ShareAlike
 * 4.0 International License. To view a copy of this license, visit
 * http://creativecommons.org/licenses/by-sa/4.0/deed.en_US.
 *
 * You may use this work for any purposes, provided that you make your
 * version available to anyone else.
 */

// You need to change these values to suit your network...

#define WIFI_SSID     "your_network_name"
#define WIFI_PASSWORD "your_password"

#define JMRI_SERVER_ADDRESS "your JMRI server"



#include <string>


#include <Time.h>
#include <TimeLib.h>

#include <WiFi.h>
#include <WiThrottle.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

// I2C address of the display.  Stick with the default address of 0x70
// unless you've changed the address jumpers on the back of the display.
#define DISPLAY_ADDRESS   0x70


const std::string ssid = WIFI_SSID;
const std::string password = WIFI_PASSWORD;

const std::string host = JMRI_SERVER_ADDRESS;
const int port = 12090;


static volatile bool wifi_connected = false;

WiFiClient client;
WiThrottle wiThrottle;

Adafruit_7segment clockDisplay = Adafruit_7segment();

void wifiOnConnect() {
  Serial.println("STA Connected");
  Serial.print("STA IPv4: ");
  Serial.println(WiFi.localIP());

  Serial.print("connecting to ");
  Serial.println(host.c_str());

  if (!client.connect(host.c_str(), port)) {
    Serial.println("connection failed");
    return;
  }
  else {
    Serial.println("connected succeeded");
    wiThrottle.connect(&client);
    wiThrottle.setDeviceName("mylittlethrottle");
  }
}

void wifiOnDisconnect() {
  wiThrottle.disconnect();
  Serial.println("STA Disconnected");
  delay(1000);
  WiFi.begin(ssid.c_str(), password.c_str());
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {

    case SYSTEM_EVENT_STA_START:
      //set sta hostname here
      WiFi.setHostname("mylittlethrottle");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      wifiOnConnect();
      wifi_connected = true;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      wifi_connected = false;
      wifiOnDisconnect();
      break;
    default:
      break;
  }
}




void setup() {
  clockDisplay.begin(DISPLAY_ADDRESS);

  Serial.begin(115200);
  WiFi.disconnect();
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_MODE_STA);

  WiFi.begin(ssid.c_str(), password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}


void updateFastTimeDisplay()
{
  static bool blinkColon = true;
  int hour = wiThrottle.fastTimeHours();
  int minutes = wiThrottle.fastTimeMinutes();

  // Show the time on the display by turning it into a numeric
  // value, like 3:30 turns into 330, by multiplying the hour by
  // 100 and then adding the minutes.
  int displayValue = hour * 100 + minutes;


  // Handle when hours are past 12 by subtracting 12 hours (1200 value).
  if (hour > 12) { displayValue -= 1200; }
  // Handle hour 0 (midnight) being shown as 12.
  else if (hour == 0) { displayValue += 1200; }


  // Now print the time value to the display.
  clockDisplay.print(displayValue, DEC);

  // Blink the colon by flipping its value every loop iteration
  // (which happens every second).
  blinkColon = !blinkColon;
  clockDisplay.drawColon(blinkColon);

  clockDisplay.writeDisplay();
}



void loop()
{
  static int lastMinutes = -1;

  // put your main code here, to run repeatedly:

  if (wiThrottle.check()) {
    if (wiThrottle.clockChanged) { updateFastTimeDisplay(); }
  }
}
