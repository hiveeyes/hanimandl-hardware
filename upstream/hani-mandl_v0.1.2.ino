/*
  Abfuellwaage Version 0.1.2
  --------------------------

  Copyright (C) 2018-2019 by Marc Vasterling, Marc Wetzel, Clemens Gruber  
            
  2018-05 Marc Vasterling | initial version, 
                            published in the Facebook group "Imkerei und Technik. Eigenbau",
                            Marc Vasterling: "meinen Code kann jeder frei verwenden, ändern und hochladen wo er will, solange er nicht seinen eigenen Namen drüber setzt."
  2018-06 Marc Vasterling | improved version, 
                            published in the Facebook group also
  2019-01 Marc Wetzel     | Refakturierung und Dokumentation, 
                            published in the Facebook group also
  2019-02 Clemens Gruber  | code beautifying mit kleineren Umbenennungen bei Funktionen und Variablen
                            Anpssung fuer Heltec WiFi Kit 32 (ESP32 onboard OLED) 
                            - pins bei OLED-Initialisierung geaendert
                            - pins geaendert, um Konflikte mit hard wired pins des OLEDs zu vermeiden 


  This code is in the public domain.


  Hinweise zur Hardware
  ---------------------
  - alle digitalen Eingänge müssen mit PullDown-Widerständen gegen GND geführt werden 

*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>      /* aus dem Bibliotheksverwalter */
#include <HX711.h>        /* https://github.com/bogde/HX711 */
#include <ESP32_Servo.h>  /* https://github.com/jkb-git/ESP32Servo */
#include <Preferences.h>  /* aus dem BSP von expressif */

#define isDebug 

Servo servo;
HX711 scale;
Preferences preferences;

//U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

// fuer Heltec WiFi Kit 32 (ESP32 onboard OLED) 
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

const int poti_pin = 39;
const int servo_pin = 2;  // changed
const int button_start_pin = 13;
const int button_stop_pin = 12;
const int switch_betrieb_pin = 14;
const int switch_setup_pin = 27;
const int hx711_dt_pin = 17;
const int hx711_sck_pin = 5;  // changed
// the pin for LED_BUILTIN is also used!
// it's pin 25 for the Heltec WiFi Kit 32 

const char* gewicht_char = "";

int pos;
int gewicht;
int menu_kalib;
int menu_korrek;
int tara;
int tara_raw;
int gewicht_raw;
float faktor;
int faktor2;
int fmenge;
int korrektur;
int autostart;
int winkel;
int winkel_min = 0;
int winkel_max = 155;
int winkel_dosier_min = 45;
float fein_dosier_gewicht = 60;
int i;
int u;
int a;
int z;

void print2serial(String displayname, int value) {
  Serial.print(displayname);
  Serial.println(value);
}

void print2serial(String displayname, float value) {
  Serial.print(displayname);
  Serial.println(value);
}

void getPreferences(void) {
  // EEPROM //
  preferences.begin("EEPROM", false);       //faktor und tara aus eeprom lesen
  faktor2 = preferences.getUInt("faktor2", 0);

  #ifdef isDebug
    if (faktor2 == 0) {
      Serial.println("Waage ist nicht kalibiert!");
      for (int i=0; i < 200; i++) {
        delay(50);
        digitalWrite(LED_BUILTIN, LOW);
        delay(50);
        digitalWrite(LED_BUILTIN, HIGH);
      }
    }
  #endif
  
  faktor = (faktor2 / 10000.00);
  tara = preferences.getUInt("tara", 0);
  tara_raw = preferences.getUInt("tara_raw", 0);
  fmenge = preferences.getUInt("fmenge", 0);
  korrektur = preferences.getUInt("korrektur", 0);
  autostart = preferences.getUInt("autostart", 0);

  print2serial("faktor = ", faktor);
  print2serial("tara_raw = ", tara_raw);
  print2serial("tara = ", tara);
  preferences.end();
}

void setupTara(void) {
  u8g2.setCursor(0, 8);
  u8g2.print("*");
  
  if ((digitalRead(button_start_pin)) == HIGH) {
    tara = ((int(scale.read_average(10)) - tara_raw) / faktor);
    u8g2.setCursor(100, 12);
    u8g2.print("OK");
    u8g2.sendBuffer();
    delay(2000);
    preferences.begin("EEPROM", false);
    preferences.putUInt("tara", tara);
    preferences.end();
  }
}

void setupCalibration(void) {
  u8g2.setCursor(0, 22);
  u8g2.print("*");
  
  if ((digitalRead(button_start_pin)) == HIGH) {
    i = 1;
    delay(300);
    u8g2.setFont(u8g2_font_courB14_tf);
    u8g2.clearBuffer();
    u8g2.setCursor(10, 12);
    u8g2.print("Bitte 500g");
    u8g2.setCursor(10, 28);
    u8g2.print("aufstellen");
    u8g2.setCursor(10, 44);
    u8g2.print("& mit Start");
    u8g2.setCursor(10, 60);
    u8g2.print("bestaetigen");
    u8g2.sendBuffer();
    
    while (i > 0) {
      if ((digitalRead(button_start_pin)) == HIGH) {
        gewicht_raw = (int(scale.read_average(10)));
        delay(2000);
        i = 0;
      }
    }
    
    i = 1;
    u8g2.clearBuffer();
    u8g2.setCursor(10, 12);
    u8g2.print("Bitte Waage");
    u8g2.setCursor(10, 28);
    u8g2.print("leeren");
    u8g2.setCursor(10, 44);
    u8g2.print("& mit Start");
    u8g2.setCursor(10, 60);
    u8g2.print("bestaetigen");
    u8g2.sendBuffer();
    
    while (i > 0) {
      if ((digitalRead(button_start_pin)) == HIGH) {
        tara_raw = (int(scale.read_average(10)));
        delay(2000);
        i = 0;
        faktor = ((gewicht_raw - tara_raw) / 500.000);

        preferences.begin("EEPROM", false);  // faktor und tara ins eeprom schreiben
        preferences.putUInt("faktor2", (faktor * 10000));
        preferences.putUInt("tara_raw", tara_raw);
        preferences.end();
      }
    }
  }
}

void setupKorrektur(void) {
  u8g2.setCursor(0, 36);
  u8g2.print("*");
  
  if ((digitalRead(button_start_pin)) == HIGH) {
    i = 1;
    delay(300);
    u8g2.setFont(u8g2_font_courB14_tf);
    u8g2.clearBuffer();
    
    while (i > 0) {
      pos = (map(analogRead(poti_pin), 0, 4095, -50, 10));
      u8g2.setFont(u8g2_font_courB14_tf);
      u8g2.clearBuffer();
      u8g2.setCursor(10, 12);
      u8g2.print("Korrektur");
      u8g2.setCursor(40, 28);
      u8g2.print(pos);
      u8g2.sendBuffer();
      
      if ((digitalRead(button_start_pin)) == HIGH) {
        korrektur = pos;
        u8g2.setCursor(100, 28);
        u8g2.print("OK");
        u8g2.sendBuffer();
        delay(2000);
        i = 0;
      }
      
      preferences.begin("EEPROM", false);
      preferences.putUInt("korrektur", korrektur);
      preferences.end();
    }
  }
}

void setupFuellmenge(void) {
  u8g2.setCursor(0, 50);
  u8g2.print("*");
  
  if ((digitalRead(button_start_pin)) == HIGH) {
    i = 1;
    delay(200);
    u8g2.setFont(u8g2_font_courB14_tf);
    u8g2.clearBuffer();
    
    while (i > 0) {
      pos = (map(analogRead(poti_pin), 0, 4095, 1, 4));
      u8g2.setFont(u8g2_font_courB14_tf);
      u8g2.clearBuffer();
      u8g2.setCursor(10, 12);
      u8g2.print(" 500g");
      u8g2.setCursor(10, 28);
      u8g2.print(" 250g");
      u8g2.setCursor(10, 44);
      u8g2.print("  50g");
      u8g2.setCursor(10, 60);
      u8g2.print("  20g");
      u8g2.sendBuffer();
      
      if (pos == 1) {
        u8g2.setCursor(0, 12);
        u8g2.print("*");
        u8g2.sendBuffer();
        
        if ((digitalRead(button_start_pin)) == HIGH) {
          fmenge = 500;
          u8g2.setCursor(100, 12);
          u8g2.print("OK");
          u8g2.sendBuffer();
          delay(2000);
          i = 0;
          preferences.begin("EEPROM", false);
          preferences.putUInt("fmenge", fmenge);
          preferences.end();
        }
      }
      
      if (pos == 2) {
        u8g2.setCursor(0, 28);
        u8g2.print("*");
        u8g2.sendBuffer();
        
        if ((digitalRead(button_start_pin)) == HIGH) {
          fmenge = 250;
          u8g2.setCursor(100, 28);
          u8g2.print("OK");
          u8g2.sendBuffer();
          delay(2000);
          i = 0;
          preferences.begin("EEPROM", false);
          preferences.putUInt("fmenge", fmenge);
          preferences.end();
        }
      }
      
      if (pos == 3) {
        u8g2.setCursor(0, 44);
        u8g2.print("*");
        u8g2.sendBuffer();
        
        if ((digitalRead(button_start_pin)) == HIGH) {
          fmenge = 50;
          u8g2.setCursor(100, 44);
          u8g2.print("OK");
          u8g2.sendBuffer();
          delay(2000);
          i = 0;
          preferences.begin("EEPROM", false);
          preferences.putUInt("fmenge", fmenge);
          preferences.end();
        }
      }
      
      if (pos == 4) {
        u8g2.setCursor(0, 60);
        u8g2.print("*");
        u8g2.sendBuffer();
        
        if ((digitalRead(button_start_pin)) == HIGH) {
          fmenge = 20;
          u8g2.setCursor(100, 60);
          u8g2.print("OK");
          u8g2.sendBuffer();
          delay(2000);
          i = 0;
          preferences.begin("EEPROM", false);
          preferences.putUInt("fmenge", fmenge);
          preferences.end();
        }
      }
    }
  }
}

void setupAutostart(void) {
  u8g2.setCursor(0, 64);
  u8g2.print("*");
  
  if ((digitalRead(button_start_pin)) == HIGH) {
    i = 1;
    delay(200);
    u8g2.setFont(u8g2_font_courB14_tf);
    u8g2.clearBuffer();
    
    while (i > 0) {
      pos = (map(analogRead(poti_pin), 0, 4095, 1, 2));
      u8g2.setFont(u8g2_font_courB14_tf);
      u8g2.clearBuffer();
      u8g2.setCursor(10, 12);
      u8g2.print("Auto EIN");
      u8g2.setCursor(10, 28);
      u8g2.print("Auto AUS");
      u8g2.sendBuffer();
      
      if (pos == 1) {
        u8g2.setCursor(0, 12);
        u8g2.print("*");
        u8g2.sendBuffer();
        
        if ((digitalRead(button_start_pin)) == HIGH) {
          autostart = 1;
          u8g2.setCursor(105, 12);
          u8g2.print("OK");
          u8g2.sendBuffer();
          delay(2000);
          i = 0;
          preferences.begin("EEPROM", false);
          preferences.putUInt("autostart", autostart);
          preferences.end();
        }
      }
      
      if (pos == 2) {
        u8g2.setCursor(0, 28);
        u8g2.print("*");
        u8g2.sendBuffer();
        
        if ((digitalRead(button_start_pin)) == HIGH) {
          autostart = 2;
          u8g2.setCursor(105, 28);
          u8g2.print("OK");
          u8g2.sendBuffer();
          delay(2000);
          i = 0;
          preferences.begin("EEPROM", false);
          preferences.putUInt("autostart", autostart);
          preferences.end();
        }
      }
    }
  }
}

void processSetup(void) {
  pos = (map(analogRead(poti_pin), 0, 4095, 1, 5));

  u8g2.setFont(u8g2_font_courB10_tf);
  u8g2.clearBuffer();
  u8g2.setCursor(10, 8);
  u8g2.print("Tara");
  u8g2.setCursor(10, 22);
  u8g2.print("Kalibrieren");
  u8g2.setCursor(10, 36);
  u8g2.print("Korrektur");
  u8g2.setCursor(10, 50);
  u8g2.print("Fuellmenge");
  u8g2.setCursor(10, 64);
  u8g2.print("Autostart");

  // Tara 
  if (pos == 1)
    setupTara();
    
  // Kalibrieren 
  if (pos == 2)
    setupCalibration();

  // Korrektur 
  if (pos == 3)
    setupKorrektur();
    
  // Füllmenge 
  if (pos == 4)
    setupFuellmenge();

  // Autostart 
  if (pos == 5)
    setupAutostart();

  u8g2.sendBuffer();
  a = 0;
}

void processBetrieb(void)
{
  pos = (map(analogRead(poti_pin), 0, 4095, 0, 100));
  gewicht = ((((int(scale.read())) - tara_raw) / faktor) - tara);
  
  if ((autostart == 1) && (gewicht <= 5) && (gewicht >= -5) && (a == 0)) {
    delay(1000);
    
    if ((gewicht <= 5) && (gewicht >= -5)) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_courB24_tf);
      u8g2.setCursor(20, 43);
      u8g2.print("START");
      u8g2.sendBuffer();
      delay(3000);
      a = 1;
    }
  }
  
  if ((autostart == 1) && (gewicht < -20)) {
    winkel = winkel_min;
    a = 0;
  }
  
  if ((digitalRead(button_start_pin)) == HIGH) {
    a = 1;
  }
  
  if ((digitalRead(button_stop_pin)) == HIGH) {
    (winkel = winkel_min);
    a = 0;
  }
  
  if (a == 1) {
    winkel = ((winkel_max * pos) / 100);
  }
  
  if ((a == 1) && (fmenge + korrektur - gewicht <= fein_dosier_gewicht)) {
    winkel = ((((winkel_max * pos) / 100)
        * ((fmenge + korrektur - gewicht) / fein_dosier_gewicht)));
  }
  
  if ((a == 1) && (winkel <= winkel_dosier_min)) {
    winkel = winkel_dosier_min;
  }
  
  if ((a == 1) && ((gewicht - korrektur) >= fmenge)) {
    winkel = winkel_min;
    a = 0;
  }
  
  if ((digitalRead(button_stop_pin)) == HIGH) {
    (winkel = winkel_min);
    a = 0;
  }
  
  servo.write(winkel);
  float y = ((fmenge + korrektur - gewicht) / fein_dosier_gewicht);
  Serial.println(y);
  
  #ifdef isDebug
    Serial.print(scale.read_average(3));
    Serial.print(" Tara_raw:");
    Serial.print(tara_raw);
    Serial.print(" Faktor ");
    Serial.print(faktor);
    Serial.print(" Gewicht ");
    Serial.println(gewicht);
  #endif

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_courB24_tf);
  
  if (gewicht < 100) {
    u8g2.setCursor(55, 43);
  }
  
  if (gewicht < 10) {
    u8g2.setCursor(75, 43);
  }
  
  if (gewicht >= 100) {
    u8g2.setCursor(35, 43);
  }
  
  if (gewicht >= 1000) {
    u8g2.setCursor(15, 43);
  }
  
  if (gewicht < 0) {
    u8g2.setCursor(55, 43);
  }
  
  if (gewicht <= -10) {
    u8g2.setCursor(35, 43);
  }
  
  if (gewicht <= -100) {
    u8g2.setCursor(15, 43);
  }
  
  u8g2.print(gewicht);
  u8g2.setCursor(95, 43);
  u8g2.print("g");
  u8g2.setFont(u8g2_font_courB14_tf);
  
  #ifdef isDebug
    u8g2.setCursor(0,13);
    u8g2.print("t=");
    u8g2.setCursor(24,13);
    u8g2.print(tara);
  #endif
  
  u8g2.setCursor(0, 13);
  u8g2.print("W=");
  u8g2.setCursor(24, 13);
  u8g2.print(winkel);
  
  if (autostart == 1) {
    u8g2.setCursor(58, 13);
    u8g2.print("AS");
  }
  
  u8g2.setCursor(0, 64);
  u8g2.print("k=");
  u8g2.setCursor(24, 64);
  u8g2.print(korrektur);
  u8g2.setCursor(73, 64);
  u8g2.print("f=");
  u8g2.setCursor(97, 64);
  u8g2.print(fmenge);
  
  if (pos < 100) {
    u8g2.setCursor(98, 13);
  }
  
  if (pos < 10) {
    u8g2.setCursor(108, 13);
  }
  
  if (pos >= 100) {
    u8g2.setCursor(88, 13);
  }
  
  u8g2.print(pos);
  u8g2.setCursor(120, 13);
  u8g2.print(char(37));
  u8g2.sendBuffer();
}

void processHandbetrieb(void)
{
  pos = (map(analogRead(poti_pin), 0, 4095, 0, 100));
  gewicht = ((((int(scale.read())) - tara_raw) / faktor) - tara);
  
  if ((digitalRead(button_start_pin)) == HIGH) {
    a = 1;
  }
  
  if ((digitalRead(button_stop_pin)) == HIGH) {
    (winkel = winkel_min);
    a = 0;
  }
  
  if ((digitalRead(button_stop_pin)) == HIGH) {
    (winkel = winkel_min);
  }
  
  if (a == 1) {
    winkel = ((winkel_max * pos) / 100);
  }
  
  servo.write(winkel);
  
  #ifdef isDebug
    Serial.print(scale.read_average(3));
    Serial.print(" Tara_raw:");
    Serial.print(tara_raw);
    Serial.print(" Faktor ");
    Serial.print(faktor);
    Serial.print(" Gewicht ");
    Serial.println(gewicht);
  #endif
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_courB24_tf);
  
  if (gewicht < 100) {
    u8g2.setCursor(55, 43);
  }
  
  if (gewicht < 10) {
    u8g2.setCursor(75, 43);
  }
  
  if (gewicht >= 100) {
    u8g2.setCursor(35, 43);
  }
  
  if (gewicht >= 1000) {
    u8g2.setCursor(15, 43);
  }
  
  if (gewicht < 0) {
    u8g2.setCursor(55, 43);
  }
  
  if (gewicht <= -10) {
    u8g2.setCursor(35, 43);
  }
  
  if (gewicht <= -100) {
    u8g2.setCursor(15, 43);
  }
  
  u8g2.print(gewicht);
  u8g2.setCursor(95, 43);
  u8g2.print("g");
  u8g2.setFont(u8g2_font_courB14_tf);
  u8g2.setCursor(0, 13);
  u8g2.print("W=");
  u8g2.setCursor(24, 13);
  u8g2.print(winkel);
  
  if (pos < 100) {
    u8g2.setCursor(98, 13);
  }
  
  if (pos < 10) {
    u8g2.setCursor(108, 13);
  }
  
  if (pos >= 100) {
    u8g2.setCursor(88, 13);
  }
  
  u8g2.print(pos);
  u8g2.setCursor(120, 13);
  u8g2.print(char(37));
  u8g2.sendBuffer();
//a=0;
}


void setup()
{
  pinMode(button_start_pin, INPUT);
  pinMode(button_stop_pin, INPUT);
  pinMode(switch_betrieb_pin, INPUT);
  pinMode(switch_setup_pin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.begin(115200);
  while (!Serial) {
  }
  
  u8g2.begin();
  scale.begin(hx711_dt_pin, hx711_sck_pin);
  scale.power_up();

  servo.attach(servo_pin, 750, 2500);

  getPreferences();
}


void loop()
{
  if ((digitalRead(switch_setup_pin)) == HIGH)
    processSetup();

  // Betrieb 
  if ((digitalRead(switch_betrieb_pin)) == HIGH)
    processBetrieb();

  // Handbetrieb 
  if ((digitalRead(switch_betrieb_pin) == LOW)
      && (digitalRead(switch_setup_pin) == LOW))
    processHandbetrieb();
}
