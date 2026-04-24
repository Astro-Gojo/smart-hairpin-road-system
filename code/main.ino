#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <SoftwareSerial.h>

hd44780_I2Cexp lcd;

// ---------------- HEAVY VEHICLE SYSTEM ----------------

const int trig1 = 2;
const int echo1 = 3;

const int trig2 = 4;
const int echo2 = 5;

const int redLED_L = 8;
const int greenLED_L = 9;
const int buzzer_L = 7;

const int redLED_R = A1;
const int greenLED_R = A2;
const int buzzer_R = A3;

// ---------------- FOG SYSTEM ----------------

const int ldrPin = A0;
const int streetLED = 6;

// ---------------- EMERGENCY SYSTEM ----------------

#define TRIG 12
#define ECHO 13

SoftwareSerial gsm(10,11);

float distance;
float lastDistance = 0;

unsigned long stopStartTime = 0;
unsigned long lastSMSTime = 0;

bool timerStarted = false;
bool messageSent = false;

const unsigned long detectionTime = 10000;
const unsigned long cooldownTime = 20000;

// ---------------- DISPLAY CONTROL ----------------

bool showAlert = false;
bool showCooldownDisplay = false;

unsigned long displayTimer = 0;

// ---------------- DOUBLE BEEP ----------------

unsigned long buzzerTimer_L = 0;
unsigned long buzzerTimer_R = 0;

int buzzerState_L = 0;
int buzzerState_R = 0;

bool buzzerRunning_L = false;
bool buzzerRunning_R = false;

// ---------------- DISTANCE ----------------

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  return duration * 0.034 / 2;
}

// ---------------- EMERGENCY DISTANCE ----------------

float getEmergencyDistance()
{
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);

  float d = duration * 0.034 / 2;
  if(d > 400 || d == 0) return 400;

  return d;
}

float getAverageDistance()
{
  float d1 = getEmergencyDistance();
  delay(30);
  float d2 = getEmergencyDistance();
  delay(30);
  float d3 = getEmergencyDistance();

  return (d1 + d2 + d3) / 3;
}

// ---------------- SMS ----------------

void sendSMS()
{
  Serial.println("Sending SMS...");

  gsm.listen();
  gsm.println("AT");
  delay(500);

  gsm.println("AT+CMGF=1");
  delay(1000);

  gsm.println("AT+CMGS=\"+91XXXXXXXXXX\"");
  delay(1500);

  gsm.print("Emergency Alert! Possible accident detected.");

  delay(500);
  gsm.write(26);

  delay(3000);

  Serial.println("SMS Sent");

  // DISPLAY CONTROL
  showAlert = true;
  showCooldownDisplay = false;
  displayTimer = millis();
}

// ---------------- SETUP ----------------

void setup() {

  Serial.begin(9600);
  gsm.begin(9600);

  lcd.begin(16,2);
  lcd.print("System Starting");
  delay(2000);
  lcd.clear();

  pinMode(trig1, OUTPUT);
  pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT);
  pinMode(echo2, INPUT);

  pinMode(redLED_L, OUTPUT);
  pinMode(greenLED_L, OUTPUT);
  pinMode(buzzer_L, OUTPUT);

  pinMode(redLED_R, OUTPUT);
  pinMode(greenLED_R, OUTPUT);
  pinMode(buzzer_R, OUTPUT);

  pinMode(streetLED, OUTPUT);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  digitalWrite(greenLED_L, HIGH);
  digitalWrite(greenLED_R, HIGH);

  Serial.println("System Ready");
}

// ---------------- LOOP ----------------

void loop() {

  int ldrValue = analogRead(ldrPin);

  int brightness = map(ldrValue, 200, 900, 255, 0);
  brightness = constrain(brightness, 0, 255);
  analogWrite(streetLED, brightness);

  int brightnessPercent = map(brightness, 0, 255, 0, 100);

  float distance1 = getDistance(trig1, echo1);
  delay(30);
  float distance2 = getDistance(trig2, echo2);

  String vehicleStatus = "ROAD CLEAR";

  // LEFT → RIGHT
  if (distance1 > 2 && distance1 < 15)
  {
    digitalWrite(redLED_R, HIGH);
    digitalWrite(greenLED_R, LOW);

    digitalWrite(redLED_L, LOW);
    digitalWrite(greenLED_L, HIGH);

    if(!buzzerRunning_R)
    {
      buzzerRunning_R = true;
      buzzerState_R = 0;
      buzzerTimer_R = millis();
    }

    vehicleStatus = "LEFT -> RIGHT";
  }

  // RIGHT → LEFT
  else if (distance2 > 2 && distance2 < 15)
  {
    digitalWrite(redLED_L, HIGH);
    digitalWrite(greenLED_L, LOW);

    digitalWrite(redLED_R, LOW);
    digitalWrite(greenLED_R, HIGH);

    if(!buzzerRunning_L)
    {
      buzzerRunning_L = true;
      buzzerState_L = 0;
      buzzerTimer_L = millis();
    }

    vehicleStatus = "RIGHT -> LEFT";
  }

  else
  {
    digitalWrite(redLED_L, LOW);
    digitalWrite(redLED_R, LOW);

    digitalWrite(greenLED_L, HIGH);
    digitalWrite(greenLED_R, HIGH);
  }

  // -------- DOUBLE BEEP --------

  if(buzzerRunning_L && millis() - buzzerTimer_L > 200)
  {
    buzzerTimer_L = millis();
    buzzerState_L++;
    digitalWrite(buzzer_L, (buzzerState_L == 1 || buzzerState_L == 3));

    if(buzzerState_L >= 4)
    {
      digitalWrite(buzzer_L, LOW);
      buzzerRunning_L = false;
    }
  }

  if(buzzerRunning_R && millis() - buzzerTimer_R > 200)
  {
    buzzerTimer_R = millis();
    buzzerState_R++;
    digitalWrite(buzzer_R, (buzzerState_R == 1 || buzzerState_R == 3));

    if(buzzerState_R >= 4)
    {
      digitalWrite(buzzer_R, LOW);
      buzzerRunning_R = false;
    }
  }

  // -------- EMERGENCY --------

  distance = getAverageDistance();

  if(!(messageSent && millis() - lastSMSTime < cooldownTime))
  {
    if(distance <= 10 && distance > 1)
    {
      if(abs(distance - lastDistance) < 0.5)
      {
        if(!timerStarted)
        {
          stopStartTime = millis();
          timerStarted = true;
        }

        if((millis() - stopStartTime) > detectionTime)
        {
          sendSMS();
          messageSent = true;
          lastSMSTime = millis();
        }
      }
      else timerStarted = false;
    }
    else timerStarted = false;
  }

  lastDistance = distance;

  // -------- LCD DISPLAY --------

  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print("                ");

  if(showAlert)
  {
    lcd.setCursor(0,0);
    lcd.print("ACCIDENT ALERT");
    lcd.setCursor(0,1);
    lcd.print("Sending SMS...");

    if(millis() - displayTimer > 3000)
    {
      showAlert = false;
      showCooldownDisplay = true;
      displayTimer = millis();
    }
  }
  else if(showCooldownDisplay)
  {
    lcd.setCursor(0,0);
    lcd.print("SMS SENT");
    lcd.setCursor(0,1);
    lcd.print("Cooldown...");

    if(millis() - displayTimer > 3000)
    {
      showCooldownDisplay = false;
    }
  }
  else
  {
    lcd.setCursor(0,0);
    lcd.print(vehicleStatus);

    lcd.setCursor(0,1);
    lcd.print("Fog:");
    lcd.setCursor(5,1);
    lcd.print(brightnessPercent);
    lcd.print("%");
  }

  // -------- SERIAL --------

  Serial.println("----");
  Serial.print("Left Dist: "); Serial.println(distance1);
  Serial.print("Right Dist: "); Serial.println(distance2);
  Serial.print("Emergency: "); Serial.println(distance);
  Serial.print("Status: "); Serial.println(vehicleStatus);

  delay(200);
}
