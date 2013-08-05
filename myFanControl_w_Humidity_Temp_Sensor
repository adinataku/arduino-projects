// Example testing sketch for various DHT humidity/temperature sensors
// Written by ladyada, public domain
// modified very little by A. Mulawarman for personal project

#include "DHT.h"

#define DHTPIN 2     // what pin we're connected to

// Uncomment whatever type you're using!
#define DHTTYPE DHT11   // DHT 11 
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE);
int relay_pin = 8;
int led = 13;

void setup() {
  Serial.begin(9600); 
  Serial.println("DHTxx test!");
  pinMode(relay_pin, OUTPUT); 
  pinMode(led,OUTPUT);
  dht.begin();
}

void loop() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float t_inF = t * 9.0 / 5.0 + 32.0;

  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT");
  } else {
    Serial.print("Humidity: "); 
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: "); 
    Serial.print(t);
    Serial.println(" *C\t");
    Serial.print("Temperature: "); 
    Serial.print(t_inF);
    Serial.println(" *F");
    if (h>50.0 or t_inF>90.0){
      digitalWrite(relay_pin,HIGH);
    }
    else {
      digitalWrite(relay_pin,LOW);
    }  
    digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);               // wait for a second
    digitalWrite(led, LOW);    // turn the LED off by making the voltage LOW
    delay(1000);               // wait for a second
  }
}
