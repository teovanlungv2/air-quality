#include <SoftwareSerial.h>
#include <U8g2lib.h>
#include <DHT.h>

#define MEASUREMENT_INTERVAL 5

#define FONT u8g2_font_5x7_tr
#define FONT_WIDTH 5
#define FONT_HEIGHT 8

#define DHT_PIN 2
#define DHT_TYPE DHT22

#define TVOC_PIN A6
#define CO_PIN A7

#define BUZZER_PIN 3

#define RX_PIN 8
#define TX_PIN 9

#define POT_PIN A2

#define BATT_PIN A3

SoftwareSerial pms(RX_PIN, TX_PIN);
U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0);
DHT dht(DHT_PIN, DHT_TYPE);

float temperature, humidity;
float tvoc, co;
int pm_1_0, pm_2_5, pm_10_0;
char buffer[32];
int frame[32];
int loops;

int seconds, minutes, hours;
int brightness, prv_brightness;

void readPMS() {
  if (pms.available() >= 32 && pms.read() == 0x42 && pms.read() == 0x4D) {
    frame[0] = 0x42;
    frame[1] = 0x4D;
    
    for (int i = 2; i < 32; ++i) frame[i] = pms.read();
    
    int checksum = 0;
    int checksumReceived = (frame[30] << 8) | frame[31];
    
    for (int i = 0; i < 30; ++i) checksum += frame[i];

    if (checksum == checksumReceived) {
      pm_1_0 = (frame[10] << 8) | frame[11];
      pm_2_5 = (frame[12] << 8) | frame[13];
      pm_10_0  = (frame[14] << 8) | frame[15];
    }
  }
  while (pms.available()) pms.read();
}

void readSensors() {
  if (loops%(MEASUREMENT_INTERVAL*20) > 0) return;
  readPMS();
  float tmp_temperature = dht.readTemperature();
  float tmp_humidity = dht.readHumidity();
  if ( !isnan(tmp_temperature) || !isnan(tmp_humidity) ) {
    temperature = tmp_temperature;
    humidity = tmp_humidity;
  }
  
  float tmp_tvoc = analogRead(TVOC_PIN)*(1000.0/1023.0);
  float tmp_co = constrain(pow(10, analogRead(CO_PIN)*(6.43786/1023.0)-0.2865061), 0.0, 1000.0);
  
  tmp_tvoc = tmp_tvoc*(1000.0/1023.0);
  tmp_co = tmp_co*(1000.0/1023.0);

  tvoc = tmp_tvoc;
  co = tmp_co;
}

void drawBattery() {
  int box_size = constrain(analogRead(BATT_PIN)*(15.0/1023.0)-7.0, 0, 4.5)*(12.0/4.5);
  display.drawRFrame(128-15, 0, 14, 4, 0);
  display.drawRBox(127, 1, 1, 2, 0);
  display.drawRBox(128-14, 1, box_size, 2, 0);
}

void drawTopRow() {
  display.setFont(u8g2_font_4x6_mr);
  sprintf(buffer, "%04d:%02d:%02d", hours, minutes, seconds);
  display.drawStr(0, 6, buffer);
  drawBattery();
  display.drawHLine(0, 7, 128);
  display.setFont(FONT);
}

void printValues() {
  display.setCursor(0, FONT_HEIGHT+7);
  display.print("Nhiet do: " + String(temperature) + "C");
  
  display.setCursor(0, FONT_HEIGHT*2+7);
  display.print("Do am: " + String(humidity) + "%");
  
  display.setCursor(0, FONT_HEIGHT*3+7);
  display.print("TVOC ~ " + String(tvoc) + ' ' + buffer);
  
  display.setCursor(0, FONT_HEIGHT*4+7);
  display.print("CO ~ " + String(co) + ' ' + buffer);

  display.setCursor(0, FONT_HEIGHT*5+7);
  display.print("PM1.0: " + String(pm_1_0) + " ug/m3 " + buffer);

  display.setCursor(0, FONT_HEIGHT*6+7);
  display.print("PM2.5: " + String(pm_2_5) + " ug/m3 " + buffer);

  display.setCursor(0, FONT_HEIGHT*7+7);
  display.print("PM10.0: " + String(pm_10_0) + " ug/m3 " + buffer);
}

void setup() {
  temperature = 25.0, humidity = 60.0;
  tvoc = 0.0, co = 0.0;
  seconds = 0, minutes = 0, hours = 0;
  pm_1_0 = 0, pm_2_5 = 0, pm_10_0 = 0;
  loops = 0;
  brightness = 0;

  pms.begin(9600);
  dht.begin();
  display.begin();

  delay(10000);
  
  display.setFont(FONT);
  display.setContrast(75);
}

void loop() {
  brightness = (1023-analogRead(POT_PIN))/4;
  if (abs(brightness-prv_brightness) > 2) {
    display.setContrast(brightness);
    prv_brightness = brightness;
  }
  if (loops%20 == 0) {
    seconds = millis()/1000;
    minutes = seconds/60%60;
    hours = seconds/3600;
    seconds = seconds%60;
    
    readSensors();
  
    display.firstPage();
    do {
      drawTopRow();
      printValues();
    } while (display.nextPage());
  }
  ++loops;
  delay(50);
}
