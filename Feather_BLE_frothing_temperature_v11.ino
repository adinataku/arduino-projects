// project log, start 7 22 2025
// note the following version must be Arduino IDE 2.3.4
// esp32 by Espressif Systems Bord support v 2.0.17
// arduinoBLE.h version 1.3.6 
// if you happen to update any of these, you may have to reverse your new
// 
// v1 ESP32-S3 just setting up the basic stuff graphic etc.
// v2 added ArduinoBLE.h to incorporate BLE test publishing random values via BLE to smartphone app
// v3 added DHT11 for testing the concept, later an RTD or Thermocouple will be replacing DHT11 as better more accurate sensors
// v4 remove DHT11 usage and replace with P100 RTD and use Adafruit Max31865 library example
// v5 make graphic nicer
// v6 rotate display and trying to keep display that are not updated constant instead of refreshing or blinking too much
// v7 adding ability to change set points up and down using D0 and D1
// v8 cleaning up some of the variable and making it easier to read; commenting out unneeded serial print no longer necessary for troubleshooting.
// v9 base code but modified to use "interrupt"
// v10 is the shorter code with the removal of BLE code as it errors after the latest arduino update and board
// v11 BLE back to working mode, pay attention to the versions requirement in the beginning of the log above, 2/17/2026.

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "Adafruit_MAX1704X.h"
#include <Adafruit_Sensor.h>


#include <Adafruit_MAX31865.h>
#define SD_CS_PIN 10 // if using the feather esp32-s3 use 10 for Chip Select Pin
Adafruit_MAX31865 thermo = Adafruit_MAX31865(10, 11, 12, 9);


// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF      430.0
// The 'nominal' 0-degrees-C resistance of the sensor
// 100.0 for PT100, 1000.0 for PT1000
#define RNOMINAL  100.0

uint32_t delayMS=250;

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_MAX17048 maxlipo;

#include <ArduinoBLE.h>
BLEService FrothService("1809"); // creating the service, 1809 is standard UUID for Thermometer Service
BLEUnsignedCharCharacteristic TemperatureReading("2A1C", BLERead | BLENotify); // 2A1C is the UUID for Temperature Measrement creating the Analog Value characteristic BLERead allow reading value from board to phone
BLEByteCharacteristic switchChar("2A56", BLERead | BLEWrite); // 2A56 creating the LED characteristic, not currently used
BLEUnsignedCharCharacteristic TemperatureSetting("2A20", BLERead| BLEWrite | BLENotify); // 2A20, creating the Analog Value characteristic BLEWrite allow writing new value from phone to board via BLE


const int ledPin = 13;
long previousMillis = 0;

/* a little bit about targetTemp, since the measurement is on the wall of the pitcher and not actually the milk
temperature directly, this is set lower because the milk temperature will get hotter faster than the pitcher
overtime this can be adjusted but from testing, I found when the pitcher wall is at 82F, the milk temperature is 
still moving up to 150F and even after frothing stop, the temperature will still go up a lot for a few more seconds. Target
temperature for Oatmilk is between 130F-155F, so stopping at 150F probably be perfect. Experiementing shows after stopping at 82, the 
milk temperature still goes up after steaming is stopped to 150 after a few seconds of rest*/
float targetTempF=145.00; //target temperature in  deg F, can be used to trigger alarm.
float targetTempC; // target temperature in deg C, always calculated from targetTempF. 

volatile int Button0Pressed,Button1Pressed,Button2Pressed;


void setup() {
  
  Serial.begin(57600);    // initialize serial communication


  //setting up TFT
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // initialize TFT
  tft.init(135, 240); // Init ST7789 240x135
  tft.setRotation(4);
  tft.fillScreen(ST77XX_BLACK);
  //canvas.setFont(&FreeSans12pt7b);
  //canvas.setTextColor(ST77XX_WHITE); 
  tft.setTextWrap(true);
  tft.setCursor(0, 0);tft.setTextColor(ST77XX_BLUE);tft.setTextSize(2);

  //Serial.println(F("ST7789 TFT Initialized"));
  tft.println(F("ST7789 TFT Initialized"));

  while (!maxlipo.begin()) {
    //Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    tft.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    delay(5000);
  }

  //Serial.print(F("Found MAX17048"));Serial.print(F(" with Chip ID: 0x"));Serial.println(maxlipo.getChipID(), HEX);
  tft.print(F("Found MAX17048"));tft.print(F(" with Chip ID: 0x"));tft.println(maxlipo.getChipID(), HEX);

  // standard Feather front push button setups
  pinMode(0, INPUT_PULLUP);pinMode(1, INPUT_PULLDOWN);pinMode(2, INPUT_PULLDOWN); 
  attachInterrupt(digitalPinToInterrupt(0), DetectButton0, FALLING);
  attachInterrupt(digitalPinToInterrupt(1), DetectButton1, RISING);
  attachInterrupt(digitalPinToInterrupt(2), DetectButton2, RISING);

  thermo.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary

  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED pin to indicate when a central is connected
  pinMode(ledPin, OUTPUT); // initialize the built-in LED pin to indicate when a central is connected


// intialize ArduinoBLE   
  if (!BLE.begin()) { //this is the line that cause the code to break, because of incompativle library and board support and IDE
    Serial.println("starting Bluetooth® Low Energy failed!");
    tft.println("starting Bluetooth® Low Energy failed!");
    while (1);
  }

  BLE.setLocalName("Feather"); //Setting a name that will appear when scanning for Bluetooth® devices
  BLE.setAdvertisedService(FrothService);

  FrothService.addCharacteristic(switchChar); //add characteristics to a service
  FrothService.addCharacteristic(TemperatureReading);
  FrothService.addCharacteristic(TemperatureSetting);

  BLE.addService(FrothService);  // adding the service

  switchChar.writeValue(0); //set initial value for characteristics
  TemperatureReading.writeValue(0);
  TemperatureSetting.writeValue(targetTempF);
  

  BLE.advertise(); //start advertising the service // this line also cause the code to break
  tft.println(" Bluetooth® device active, waiting for connections...");
  tft.setTextWrap(true);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);tft.setTextColor(ST77XX_GREEN);tft.setTextSize(2);
}

void loop() {
  int xo=80,yo=31,xl=60,yl=15; // 15 is a good coverage/font height for text size of 2
  int xo1=0,yo1=48,xl1=135,yl1=15; 
  int xo3=0,yo3=78,xl3=135,yl3=15;
  int xo4=0,yo4=16,xl4=135,yl4=15;
  int xo5=0,yo5=32,xl5=135,yl5=15;
  targetTempC=(targetTempF-32)*5/9;
  //float currentTemp=ReadTemperature();
  float cellVoltage = maxlipo.cellVoltage();

  //tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);tft.setTextColor(ST77XX_GREEN);tft.setTextSize(2);
  
  if (isnan(cellVoltage)) {
    Serial.println("Failed to read cell voltage, check battery is connected!");
    delay(2000);
    return;
  }

  if (maxlipo.cellPercent()<25){
    tft.setTextColor(ST77XX_RED);
  }
  else {
    tft.setTextColor(ST77XX_GREEN);
  }  
  tft.println("Batt: "); 
  
  tft.setCursor(xo4,yo4);tft.fillRect(xo4,yo4,xl4,yl4,ST77XX_BLACK);
  tft.print(cellVoltage, 1); tft.print(" V,");
  tft.print(maxlipo.cellPercent(), 0); tft.println(" %");
  
  uint16_t rtd = thermo.readRTD();

  Serial.print("RTD value: "); Serial.println(rtd);
  float ratio = rtd;
  ratio /= 32768;
  //Serial.print("Ratio = "); Serial.println(ratio,8);
  //Serial.print("Resistance = "); Serial.println(RREF*ratio,8);
  //Serial.print("Temperature = "); Serial.println(thermo.temperature(RNOMINAL, RREF));
  
  tft.println("SetF/C:");
  tft.setCursor(xo5,yo5);tft.fillRect(xo5,yo5,xl5,yl5,ST77XX_BLACK);
  tft.print(targetTempF,0);tft.print("/");tft.println(targetTempC,1);
  //uint8_t fault = thermo.readFault();

  BLEDevice central = BLE.central(); // wait for a Bluetooth® Low Energy central
  tft.print("BLE:");
  tft.setCursor(xo3,yo3);tft.fillRect(xo3,yo3,xl3,yl3,ST77XX_BLACK);
  tft.println("Disconnect");
  Serial.print("Disconnected from central: ");
  Serial.println(central.address());  
  

  ReadTemperature();



  if (Button0Pressed) {
    targetTempF = targetTempF + 1.0;
    tft.setCursor(xo1,yo1);tft.fillRect(xo1,yo1,xl1,yl1,ST77XX_BLACK);
    Button0Pressed=0;
  }
  if (Button1Pressed) {
    targetTempF = targetTempF - 1.0;
    tft.setCursor(xo1,yo1);tft.fillRect(xo1,yo1,xl1,yl1,ST77XX_BLACK);
    Button1Pressed=0;
  }

   while (central.connected()) {  // if a central is connected to the peripheral
    //tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);tft.setTextColor(ST77XX_GREEN);tft.setTextSize(2); 
    Serial.print("Connected to central: ");    
    Serial.println(central.address()); // print the central's BT address    
    //digitalWrite(LED_BUILTIN, HIGH); // turn on the LED to indicate the connection
    //tft.print("BLE:");tft.println(central.address());
    if (maxlipo.cellPercent()<25){
      tft.setTextColor(ST77XX_RED);
    }
    else {
      tft.setTextColor(ST77XX_GREEN);
    }  

    tft.println("Batt: "); tft.print(cellVoltage, 1); tft.print(" V,");
    tft.setCursor(xo4,yo4);tft.fillRect(xo4,yo4,xl4,yl4,ST77XX_BLACK);
    tft.print(maxlipo.cellPercent(), 0); tft.println(" %"); 
    tft.println("SetF/C:");
    tft.setCursor(xo5,yo5);tft.fillRect(xo5,yo5,xl5,yl5,ST77XX_BLACK);
    tft.print(targetTempF,0);tft.print("/");tft.println(targetTempC,1);
    tft.print("BLE:");
    tft.setCursor(xo3,yo3);tft.fillRect(xo3,yo3,xl3,yl3,ST77XX_BLACK);
    tft.println("Connected");
    Serial.print("SettingfromBLEdevice: ");Serial.println(TemperatureSetting.value());
    ReadTemperature();
    
    if (Button0Pressed) {
      targetTempF = targetTempF + 1.0;TemperatureSetting.writeValue(targetTempF); 
      tft.setCursor(xo1,yo1);tft.fillRect(xo1,yo1,xl1,yl1,ST77XX_BLACK);
      
      Button0Pressed=0;
    }
    if (Button1Pressed) {
      targetTempF = targetTempF - 1.0;TemperatureSetting.writeValue(targetTempF); 
      tft.setCursor(xo1,yo1);tft.fillRect(xo1,yo1,xl1,yl1,ST77XX_BLACK);
      Button1Pressed=0;
    } 
    

    if (TemperatureSetting.written()){
      targetTempF=TemperatureSetting.value();
    }


  //digitalWrite(LED_BUILTIN, LOW); // when the central disconnects, turn off the LED
  
    delay(delayMS);
   }
  delay(delayMS);

}

void ReadTemperature() {
  uint8_t fault = thermo.readFault();
  int xo2=0,yo2=116,xl2=135,yl2=80;
  if (fault){
    Serial.print("Fault 0x"); Serial.println(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH) {
      Serial.println("RTD High Threshold"); 
    }
    if (fault & MAX31865_FAULT_LOWTHRESH) {
      Serial.println("RTD Low Threshold"); 
    }
    if (fault & MAX31865_FAULT_REFINLOW) {
      Serial.println("REFIN- > 0.85 x Bias"); 
    }
    if (fault & MAX31865_FAULT_REFINHIGH) {
      Serial.println("REFIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_RTDINLOW) {
      Serial.println("RTDIN- < 0.85 x Bias - FORCE- open"); 
    }
    if (fault & MAX31865_FAULT_OVUV) {
      Serial.println("Under/Over voltage"); 
    }
    thermo.clearFault();
  
  }
  else {
    
      Serial.print(F("Temperature: "));
      Serial.print(thermo.temperature(RNOMINAL, RREF));
      Serial.println(F("°C"));
      //tft.print("ReadF/C:");tft.print((thermo.temperature(RNOMINAL, RREF))*9.0/5.0+32.0,0);tft.print("/");tft.println((thermo.temperature(RNOMINAL, RREF)),1);
      tft.println("");
      int TemperatureValueC = thermo.temperature(RNOMINAL, RREF); // this is in celcius by default
      int TemperatureValueF = TemperatureValueC*9/5+32;
      tft.setTextSize(5);
      if (TemperatureValueF>targetTempF) {tft.setTextColor(ST77XX_RED);Serial.println("TemperatureValueF>targetTempF");}else {tft.setTextColor(ST77XX_GREEN);Serial.println("TemperatureValueF<=targetTempF");}
      tft.setCursor(xo2,yo2);tft.fillRect(xo2,yo2,xl2,yl2,ST77XX_BLACK); //135x240 is the pixel dimension of feather TFT display
      tft.print(TemperatureValueF,0);tft.println("F");tft.print((TemperatureValueC),1);tft.println("C");
      tft.setTextSize(2);
      TemperatureReading.writeValue(TemperatureValueF);//replace with TemperatureValueF 


  }
  return;
}

void DetectButton0() {
  Button0Pressed = 1;
}
void DetectButton1() {
  Button1Pressed = 1;
}
void DetectButton2() {
  Button2Pressed = 1;
}
