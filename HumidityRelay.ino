#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPDash.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>     
#include <ESPAsyncWiFiManager.h>
#include "DHT.h"
#include <EasyBuzzer.h>

static const uint8_t D0 = 16, D1 = 5, D2 = 4,D3 = 0, D4 = 2, D5 = 14, D6 = 12, D7 = 13, D8 = 15, D9 = 3, D10 = 1;

long oldTime;
//const char *ssid = "Alpha Centauri";

int const HUMIDITY_LOWER_THRESHHOLD = 10;
int const HUMIDITY_UPPER_THRESHHOLD = 100;
int const HUMIDITY_MIN_DIFF = 2;
uint settingsAddr = 0;
struct { 
    int minHumidity;
    int maxHumidity;
} settings;

DHT dht(D1, DHT11);
AsyncWebServer server(80);
ESPDash dashboard(&server); 
DNSServer dns;

Card humidityCard(&dashboard, HUMIDITY_CARD, "HUMIDITY", "%");
Card temperatureCard(&dashboard, TEMPERATURE_CARD, "TEMPERATURE", "°C");
Card minHumiditySliderCard(&dashboard, SLIDER_CARD, "Humidity Min %", "", HUMIDITY_LOWER_THRESHHOLD, HUMIDITY_UPPER_THRESHHOLD-HUMIDITY_MIN_DIFF);
Card maxHumiditySliderCard(&dashboard, SLIDER_CARD, "Humidity Max %", "", HUMIDITY_LOWER_THRESHHOLD + HUMIDITY_MIN_DIFF, HUMIDITY_UPPER_THRESHHOLD);
Card mainSwitchButtonCard(&dashboard, BUTTON_CARD, "RELAY SWITCH");
Card autocontrolButtonCard(&dashboard, BUTTON_CARD, "AUTO CONTROL");

bool relayOn;
bool autoControl;



void loadDefaults(){
  settings.minHumidity = HUMIDITY_LOWER_THRESHHOLD;
  settings.maxHumidity = HUMIDITY_UPPER_THRESHHOLD;
}


void loadSettingsFromEEPROM(){
  EEPROM.begin(512);
  EEPROM.get(settingsAddr, settings);
}

void updateEEPROM(){
  EEPROM.put(settingsAddr,settings);
  EEPROM.commit();
}

float getH1(){
  return dht.readHumidity();
}


float getT1(){
  return dht.readTemperature();
}
//
//tone(Passive_buzzer, 523) ; //DO note 523 Hz
//tone(Passive_buzzer, 587) ; //RE note ...
//tone(Passive_buzzer, 659) ; //MI note ...
//tone(Passive_buzzer, 783) ; //FA note ...
//tone(Passive_buzzer, 880) ; //SOL note ...
//tone(Passive_buzzer, 987) ; //LA note ...
//tone(Passive_buzzer, 1046) ; // SI note .


bool prevState;

void setRelayEnabled(bool relayState){
    if(prevState == relayState){
      return;
    }
    
    prevState = relayState;

    if(relayState){
       mainSwitchButtonCard.update(String("OFF"), "danger");
       digitalWrite(D2, LOW);
       relayOn = relayState;
       EasyBuzzer.beep(
          523, 
          3
        );
    } else {
       mainSwitchButtonCard.update(String("ON"), "success");
       digitalWrite(D2, HIGH);
       relayOn = relayState;
       EasyBuzzer.beep(
          880, 
          2
        );
    }

}

void updateReadings(){
  float h1 = getH1();
  humidityCard.update(h1);
  float t1 = getT1();
  temperatureCard.update(t1);

  if(!autoControl){
    return;
  }
  
  if(h1 >= settings.minHumidity){
    setRelayEnabled(true);
    return;
  }

  if(h1 <= settings.maxHumidity){
    setRelayEnabled(false);
    return;
  }

}




void setup() {

Serial.begin(115200);
  Serial.println("\nInitializing....");

  loadSettingsFromEEPROM();

  if(settings.maxHumidity < 0 || settings.maxHumidity > HUMIDITY_UPPER_THRESHHOLD){
      loadDefaults();
  }

  pinMode(D2, OUTPUT);
  EasyBuzzer.setPin(D7);
  digitalWrite(D2, HIGH);
 
  /* Connect WiFi */
  WiFi.mode(WIFI_STA);
//  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
  } 
  
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  minHumiditySliderCard.update(settings.minHumidity); 
  minHumiditySliderCard.attachCallback([&](int value){
    
    settings.minHumidity=value;
    if(value >= HUMIDITY_UPPER_THRESHHOLD - HUMIDITY_MIN_DIFF){
      settings.minHumidity= HUMIDITY_UPPER_THRESHHOLD - HUMIDITY_MIN_DIFF;
    } 
    minHumiditySliderCard.update(settings.minHumidity);
    if(settings.minHumidity>settings.maxHumidity){
      settings.maxHumidity = settings.minHumidity+HUMIDITY_MIN_DIFF;
      if(settings.maxHumidity > HUMIDITY_UPPER_THRESHHOLD){
        settings.maxHumidity = HUMIDITY_UPPER_THRESHHOLD;
      }
      maxHumiditySliderCard.update(settings.maxHumidity);
    }

 
    updateEEPROM();
    dashboard.sendUpdates();
  });

  maxHumiditySliderCard.update(settings.maxHumidity);
  maxHumiditySliderCard.attachCallback([&](int value){

    settings.maxHumidity=value;
    if(value < HUMIDITY_LOWER_THRESHHOLD + HUMIDITY_MIN_DIFF){
      settings.maxHumidity= HUMIDITY_LOWER_THRESHHOLD + HUMIDITY_MIN_DIFF;
    } 
    
    maxHumiditySliderCard.update(settings.maxHumidity);
    
      if(settings.maxHumidity<settings.minHumidity-HUMIDITY_MIN_DIFF){
        settings.minHumidity = settings.maxHumidity-HUMIDITY_MIN_DIFF;
        
        if(settings.minHumidity < HUMIDITY_LOWER_THRESHHOLD){
          settings.minHumidity = HUMIDITY_LOWER_THRESHHOLD;
        }
        
        minHumiditySliderCard.update(settings.minHumidity);
      }
    updateEEPROM();
    dashboard.sendUpdates();
  });
  
  mainSwitchButtonCard.update(String("ON"), "success");
  mainSwitchButtonCard.attachCallback([&](bool value){
    if(!relayOn){
       setRelayEnabled(true);
    } else {
       setRelayEnabled(false);
    }
    dashboard.sendUpdates();
  });
  
  autocontrolButtonCard.update(String("ON"), "success");
  autocontrolButtonCard.attachCallback([&](bool value){
    if(!autoControl){
      autocontrolButtonCard.update(String("ON"), "success");
       autoControl = true;
    } else {
      autocontrolButtonCard.update(String("OFF"), "danger");
       autoControl = false;
    }
    dashboard.sendUpdates();
  });

  
  server.begin();
  dht.begin(); 
  updateReadings();
}


void loop() {

  if (millis() - oldTime > 700) {
      updateReadings();
      dashboard.sendUpdates();
      oldTime = millis();
  }

  if ( digitalRead(D3) == LOW ) {
    WiFi.disconnect(true);
    AsyncWiFiManager wifiManager(&server,&dns);
    wifiManager.startConfigPortal("Disel heater");
    Serial.println("connected...yeey :)");
  }
  
  EasyBuzzer.update();

}
