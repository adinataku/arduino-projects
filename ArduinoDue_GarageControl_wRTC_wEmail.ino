

// include the library code:
#include <LiquidCrystal.h>
#include "RTCdue.h"
//this version use RTCdue.h which is better for the Due

RTC_DS1307 rtc;

const int GarageSwitchPin=2; // garaga limit switch
const int GarageAlarmLEDIndication=13; //garage led open close indication
const int buzzerPin=5;

//setup for email
int minSecsBetweenEmails = 600; //10 min
long lastSend = -minSecsBetweenEmails * 1000l;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

void setup() {
  Serial.begin(57600);
  
  rtc.begin();

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }
  
  pinMode(GarageSwitchPin, INPUT); // This setup garageSwitchPin as an Input
  digitalWrite(GarageSwitchPin, HIGH); // this turns on internal pull up resistors
  // so when the button is not pressed, the input will be High (normal) LOW (garage open)
  // indicate alarm condition
  
  pinMode(GarageAlarmLEDIndication, OUTPUT);
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  // Print a message to the LCD.
  //lcd.print("Arduino Due Rocks!");
  
  pinMode(buzzerPin, OUTPUT);
  beep(50);
}

void loop() {
  int garageState;
  long now_email = millis();
  
  garageState=digitalRead(GarageSwitchPin);
  Serial.println(garageState);
  DateTime now = rtc.now();
  
  if (garageState == LOW) //alarm state detected
  {
    // blink LED EVERY SECOND
     digitalWrite(GarageAlarmLEDIndication, HIGH);
     delay(500);
     digitalWrite(GarageAlarmLEDIndication, LOW);
     delay(500);
     lcd.setCursor(0,1);
     lcd.print("Garage Open!");
     if (now.hour()>=20 && now.hour()<=23) // alarms only function between 8am to 11pm 
     {
       beep(100); // enable after testing.
     
       //send email routine
       if (now_email > (lastSend + minSecsBetweenEmails * 1000l)) // this line also limits email alert every 10 minutes.
       
      {
        Serial.println("GARAGE OPEN");
        lastSend = now_email;
      }
      else
      {
        Serial.println("Too soon");
      }
    }
     
  }  
  else //normal state
  {
  digitalWrite(GarageAlarmLEDIndication, LOW);
  lcd.setCursor(0,1);
  lcd.print("Garage Close");
  //noTone(buzzerPin);
  }
  lcd.setCursor(0,0);
  lcd.print(now.hour(), DEC);
  lcd.print(":");
  lcd.print(now.minute(), DEC);
  lcd.print(" ");
  lcd.print(now.month(), DEC);
  lcd.print(".");
  lcd.print(now.day(), DEC);
  lcd.print(".");
  lcd.print(now.year(), DEC);

}

void beep(unsigned char delayms) {
  analogWrite(buzzerPin,220);
  delay(delayms);
  analogWrite(buzzerPin,0);
  delay(delayms);
}
