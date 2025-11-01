#include <SoftwareSerial.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <DFRobot_ENS160.h>
#include <Wire.h>

#define MEASUREMENT_INTERVAL 5

#define FONT u8g2_font_5x8_tr
#define FONT_WIDTH 5
#define FONT_HEIGHT 8

#define DHT_PIN 2
#define DHT_TYPE DHT22

#define CO_PIN A3

#define BUZZER_PIN 3

#define RX_PIN 8
#define TX_PIN 9

#define POT_PIN A6

#define BATT_PIN A7

SoftwareSerial pms(RX_PIN, TX_PIN);
U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0);
DHT dht(DHT_PIN, DHT_TYPE);
DFRobot_ENS160_I2C ENS160(&Wire, 0x53);

float temperature, humidity;
float co;
uint16_t tvoc, co2, aqi;
uint16_t pm_1_0, pm_2_5, pm_10_0;
char buffer[50];
uint16_t data[50];
uint16_t frame[32];
uint16_t loops;

uint16_t start_second, prv_second, seconds_since_powered, seconds, minutes, hours;
uint8_t curr_mode, prv_mode;
uint8_t index;

void warnLevel(float val, float a, float b, float c, float d, float e) {
  if (val <= a) strcpy(buffer, "Tot");
  else if (val <= b) strcpy(buffer, "Kha");
  else if (val <= c) strcpy(buffer, "Trung binh");
  else if (val <= d) strcpy(buffer, "Xau");
  else if (val <= e) strcpy(buffer, "Rat xau");
  else strcpy(buffer, "Nguy hai");
}

int countDigits(int n) {
  if (n >= 10000) return 5;
  if (n >= 1000) return 4;
  if (n >= 100) return 3;
  if (n >= 10) return 2;
  return 1;
}

int calcAQI() {
  if (pm_1_0 > 50 || pm_2_5 > 100 || pm_10_0 > 300 || co > 25.0 || aqi == 5) {
    aqi = 5;
  } else if (pm_1_0 > 35 || pm_2_5 > 50 || pm_10_0 > 100 || aqi == 4) {
    aqi = 4;
  } else if (pm_1_0 > 20 || pm_2_5 > 25 || pm_10_0 > 50 || aqi == 3) {
    aqi = 3;
  } else if (pm_1_0 > 10 || pm_2_5 > 12 || pm_10_0 > 25 || aqi == 2) {
    aqi = 2;
  } else {
    aqi = 1;
  }
}

void giveRecommendation(int type) {
  if (type == 1) {
    switch (aqi) {
      case 1:
        strcpy(buffer, "Khong can lam gi");
        break;
      case 2:
        strcpy(buffer, "Mo cua so thoang khi");
        break;
      case 3:
        strcpy(buffer, "Loc khi, mo cua thong gio");
        break;
      case 4:
        strcpy(buffer, "Tim nguon o nhiem");
      case 5:
        strcpy(buffer, "Ra khoi nha, chay di!!!");
    }
  } else {
    switch (aqi) {
      case 1:
        strcpy(buffer, "Khong can lam gi");
        break;
      case 2:
        strcpy(buffer, "Khong can lam gi");
        break;
      case 3:
        strcpy(buffer, "Deo khau trang");
        break;
      case 4:
        strcpy(buffer, "Han che ra ngoai");
      case 5:
        strcpy(buffer, "Khong ra ngoai, loc khi");
    }
  }
}

void drawGraph(int minX, int maxX, int minY, int maxY) {
  display.setFont(u8g2_font_4x6_mr);
  int offset = countDigits(maxY)*4;

  display.drawVLine(offset+2, 9, 48);
  display.drawHLine(offset+1, 55, 127-offset);
  for (int i = 0; i <= 3; ++i) {
    int x = offset+1, y = 56/4*(i+1)-1;
    display.drawPixel(x, y);
    display.setCursor(0, y);
    display.print(minY + (maxY-minY)*(3-i)/3);
    x = offset + 2 + (109 - 2*offset)*i/3, y = 56;
    display.drawPixel(x, y);
    display.setCursor(x, 63);
    display.print(minX + (maxX-minX)*i/3);
  }
  display.setFont(FONT);
}

void readPMS() {
  if (pms.available() >= 32 && pms.read() == 0x42 && pms.read() == 0x4D) {
    frame[0] = 0x42;
    frame[1] = 0x4D;
    
    for (int i = 2; i < 32; ++i) frame[i] = pms.read();
    
    uint16_t checksum = 0;
    uint16_t checksumReceived = (frame[30] << 8) | frame[31];
    
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
  prv_second = seconds_since_powered-5;
  readPMS();
  float tmp_temperature = dht.readTemperature();
  float tmp_humidity = dht.readHumidity();
  if ( !isnan(tmp_temperature) || !isnan(tmp_humidity) ) {
    temperature = tmp_temperature;
    humidity = tmp_humidity;
  }
  
  co = constrain(pow(10, analogRead(CO_PIN)*(5.0/1023.0)*1.287572 - 0.286506), 0.0, 1000.0);

  ENS160.setTempAndHum(temperature, humidity);
  if (ENS160.getENS160Status() == 0 || ENS160.getENS160Status() == 2) {
    tvoc = ENS160.getTVOC();
    co2 = ENS160.getECO2();
    aqi = ENS160.getAQI();
    calcAQI();
  }
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
  switch(curr_mode) {
    case 0:
      display.setCursor(0, FONT_HEIGHT+7);
      display.print("Nhiet do: " + String(temperature) + "C");
      
      display.setCursor(0, FONT_HEIGHT*2+7);
      display.print("Do am: " + String(humidity) + "%");

      warnLevel(tvoc, 65, 220, 660, 2200, 5500);
      display.setCursor(0, FONT_HEIGHT*3+7);
      display.print("TVOC ~ " + String(tvoc) + " ppb " + buffer);
    
      warnLevel(co, 6.0, 9.0, 13.5, 25.0, 35.0);
      display.setCursor(0, FONT_HEIGHT*4+7);
      display.print("CO: " + String(co) + " ppm " + buffer);

      warnLevel(co2, 400.0, 1000.0, 2000.0, 5000.0, 50000.0);
      display.setCursor(0, FONT_HEIGHT*5+7);
      display.print("CO2: " + String(co2) + " ppm " + buffer);

      warnLevel(pm_1_0, 10.0, 20.0, 35.0, 50.0, 100.0);
      display.setCursor(0, FONT_HEIGHT*6+7);
      display.print("PM1.0: " + String(pm_1_0) + " ug/m3 " + buffer);

      warnLevel(pm_2_5, 12.0, 25.0, 50.0, 150.0, 300.0);
      display.setCursor(0, FONT_HEIGHT*7+7);
      display.print("PM2.5: " + String(pm_2_5) + " ug/m3 " + buffer);
      break;
    case 1:
      warnLevel(pm_10_0, 25.0, 50.0, 100.0, 300.0, 600.0);
      display.setCursor(0, FONT_HEIGHT+7);
      display.print("PM10.0: " + String(pm_10_0) + " ug/m3 " + buffer);

      warnLevel(aqi, 1, 2, 3, 4, 5);
      display.setCursor(0, FONT_HEIGHT*2+7);
      display.print("AQI: " + String(buffer));

      display.drawStr(0, FONT_HEIGHT*3+7, "Neu o trong nha:");
      giveRecommendation(1);
      display.setCursor(0, FONT_HEIGHT*4+7);
      display.print(buffer);
      display.drawStr(0, FONT_HEIGHT*5+7, "Neu dang o ngoai:");
      giveRecommendation(2);
      display.setCursor(0, FONT_HEIGHT*6+7);
      display.print(buffer);
      break;
  }
}

void setup() {
  temperature = 25.0, humidity = 60.0;
  ENS160.begin();
  ENS160.setPWRMode(ENS160_STANDARD_MODE);
  pms.begin(9600);
  dht.begin();
  display.begin();
  pinMode(BUZZER_PIN, OUTPUT);

  delay(10000);
  
  ENS160.setTempAndHum(temperature, humidity);
  display.setFont(FONT);
  display.setContrast(50);
}

void loop() {
  curr_mode = (1023-analogRead(POT_PIN))/512;
  if (abs(curr_mode-prv_mode) > 0) {
    start_second = seconds_since_powered;
    display.firstPage();
    do {
      drawTopRow();
      printValues();
    } while (display.nextPage());
    prv_mode = curr_mode;
  }
  if (loops%20 == 0) {
    seconds_since_powered = millis()/1000;
    seconds = seconds_since_powered%60;
    minutes = seconds_since_powered/60%60;
    hours = seconds_since_powered/3600;
    seconds = seconds_since_powered%60;
    
    readSensors();

    if (tvoc > 2200 || co > 35.0 || co2 > 5000.0 || pm_1_0 > 100 || pm_2_5 > 150 || pm_10_0 > 300) {
      for (int i = 1; i <= 4; ++i) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(500);
        digitalWrite(BUZZER_PIN, LOW);
        delay(500);
      }
    }
    
    display.firstPage();
    do {
      drawTopRow();
      printValues();
    } while (display.nextPage());
  }
  ++loops;
  delay(50);
}
