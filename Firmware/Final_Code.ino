#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
Adafruit_SHT31 sht31 = Adafruit_SHT31();
// FAN 
#define FAN_READER_PIN 27
#define FAN_SPEED_PIN 26

// ENCODER
#define ENCODER_CLK 32
#define ENCODER_DT 33
#define ENCODER_SW 25

// OLED
U8G2_SSD1309_128X64_NONAME2_F_4W_HW_SPI u8g2(
  U8G2_R0,
  5,   // CS
  4,   // DC
  U8X8_PIN_NONE
);

// VARIABLES 
bool Current;
bool TurnRight;
bool TurnLeft;
bool IsModeTwo = false;
int LastTurn;
int CurrentTurn;
int TurnNumber = 0;
int HistoricalSpeed;
int lastCLK = HIGH;

float temperature = 0.0;

String TempGather;

String temp2;

String IsModeTwoText;

String PWMString;

String TargetTempString;
int TargetTemp = 0.0;
// FAN SPEED
volatile uint32_t pulseCount = 0;
volatile uint32_t lastPulseMicros = 0;

float rpm = 0;
unsigned long lastTime = 0;

// TACH ISR
void IRAM_ATTR countPulse() {
  uint32_t now = micros();

  // Ignore electrical noise
  if (now - lastPulseMicros > 200) {
    pulseCount++;
    lastPulseMicros = now;
  }
}

// FAN PWM
void SetFanPWM(int percent)
{
  percent = constrain(percent, 0, 100);

  TurnNumber = percent;

  int duty = map(percent, 0, 100, 0, 255);

  // ESP32 Arduino Core 3.x API
  ledcWrite(FAN_SPEED_PIN, duty);
}

int GetFanPWM()
{
  return TurnNumber;
}

// ENCODER
void TurnPercentage(String Direction)
{
  if (Direction == "Right")
  {
    if (IsModeTwo == true)
    {
      TargetTemp = TargetTemp +2;
    }
    if (TurnNumber < 100)
    {
      if (IsModeTwo == true)
      {
        Serial.println("Turning is now turned off");
      } else
      {
        TurnNumber += 5;
      }
     // Serial.println("R");
    }
  }

  if (Direction == "Left")
  {
      if (IsModeTwo == true)
    {
      if (TargetTemp > 0)
      {
        TargetTemp = TargetTemp -2;
      }
    }
    if (TurnNumber > 0)
    {
      if (IsModeTwo == true)
      {
        Serial.println("Turning is now turned off");
      } else
      {
        TurnNumber -= 5;
      }
    //  Serial.println("L");
    }
  }

  SetFanPWM(TurnNumber);
}

void setup()
{
  Serial.begin(115200);
  IsModeTwoText = "false";
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);

  pinMode(FAN_READER_PIN, INPUT_PULLUP);

  SPI.begin(18, -1, 23, 5);
  u8g2.begin();

  // Fan Tachometer
  attachInterrupt(
    digitalPinToInterrupt(FAN_READER_PIN),
    countPulse,
    FALLING);

  // PWM Output
  ledcAttach(FAN_SPEED_PIN, 25000, 8);
  SetFanPWM(TurnNumber);

  lastCLK = digitalRead(ENCODER_CLK);
  lastTime = millis();

 // Serial.println("Fan Controller Started");

  Wire.begin(21, 22);

if (!sht31.begin(0x44))
{
    Serial.println("Couldn't find SHT31!");
}
else
{
    Serial.println("SHT31 Found");
}
}

void loop()
{
  Current = digitalRead(ENCODER_SW);

  // Encoder 
  int clkState = digitalRead(ENCODER_CLK);

  if (clkState != lastCLK)
  {
    if (digitalRead(ENCODER_DT) != clkState)
    {
      TurnPercentage("Right");
    }
    else
    {
      TurnPercentage("Left");
    }

    lastCLK = clkState;
  }

  // RPM
  unsigned long now = millis();

  if (now - lastTime >= 1000)
  {
    noInterrupts();
    uint32_t count = pulseCount;
    pulseCount = 0;
    interrupts();

    rpm = (count / 2.0f) * 60.0f;

    lastTime = now;
  }

float temp = sht31.readTemperature();

  if (!isnan(temp))
  {
      temperature = temp;
      temp2 = String(int(round(temperature)));
  }

  // OLED
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 13, "Cool Companion");
  u8g2.drawCircle(110, 30, 40);
  u8g2.drawCircle(109,30, 40);
  //u8g2.drawStr(0, 28, ("Mode: " + IsModeTwoText).c_str());
  if (Current == 1)
  {
    CurrentTurn = 1;
  }
  else
  {
    CurrentTurn = 0;

    if (!CurrentTurn == LastTurn)
    {
      Serial.println("NEW");
      if (IsModeTwo == false)
      {
        IsModeTwo = true;
        IsModeTwoText = "true";
      } else
      {
        IsModeTwo = false;
        IsModeTwoText = "false";
      }
    }
  }

  LastTurn = CurrentTurn;
  CurrentTurn = 2;

  if (IsModeTwo == false)
  {
  u8g2.setFont(u8g2_font_ncenB12_tr);
  char pwmText[20];
  PWMString = String(GetFanPWM());
  if (TargetTemp > 0)
  {
    TargetTempString = "Target: "+ String(TargetTemp) + "C";
  } else
  {
    TargetTempString = "Off";
  }
  sprintf(pwmText, PWMString.c_str());
  u8g2.drawStr(94, 32, pwmText);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 45, TargetTempString.c_str());
  char rpmText[20];
  sprintf(rpmText, "RPM: %.0f", rpm);
  u8g2.drawStr(0, 58, rpmText);

  String TempGather = temp2 + "°C";
  u8g2.drawStr(0, 29, TempGather.c_str());
  } else
  {
  u8g2.setFont(u8g2_font_ncenB12_tr);
  TargetTempString = String(TargetTemp);
  u8g2.drawStr(94, 32, TargetTempString.c_str());
  u8g2.setFont(u8g2_font_ncenB08_tr);
  char pwmText[20];
  PWMString = "Speed: " + String(GetFanPWM());
  sprintf(pwmText, PWMString.c_str());
  u8g2.drawStr(0, 45, pwmText);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  char rpmText[20];
  sprintf(rpmText, "RPM: %.0f", rpm);
  u8g2.drawStr(0, 58, rpmText);

  String TempGather = temp2 + "°C";
  u8g2.drawStr(0, 29, TempGather.c_str());
  }



  if (TargetTemp > temperature && !TargetTemp == 0)
  {
    SetFanPWM(0);
  }

  if (TargetTemp == round(temperature) && !TargetTemp == 0)
  {
    Serial.println("At Temp");
    SetFanPWM(0);
  } else
  {
    if (TargetTemp < round(temperature) && !TargetTemp == 0)
    {
      Serial.println("Temp Is Too High!");
      if (IsModeTwo == true)
      {
      SetFanPWM(100);
      }
    }
  }

  u8g2.sendBuffer();
  delay(10);
}