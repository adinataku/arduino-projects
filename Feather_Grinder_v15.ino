#include <Adafruit_NAU7802.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "RTClib.h"
#include <SD.h>

/* v15 modifying the button press into interrupts */

//v14 fix problem where the g/s flow rate is not updating but jumbled together and impossible to read
// also want to add feature where the weight is still measured if the TAR is not pushed instead of just frozen at 0 or at the last values
// I need to rewrite this also because I think the last v13 has lots of error. Problem is if I screw this up, I don't have a spare.

//v13 add a measurement of g/s flow, this is an attempt to later measure flow rate
// output of ml/s of the portafilter. Assuming 1g equal 1ml, this will be a good test


//v12 is a copy of v10 but with adding Adaloger (RealTimeClock and SD card)
// target weight and target time settings are now stored in SD card. Data is stored as soon as the TAR button is set. 
//note to future self. On connecting. Sometime you need to turn off your wifi or ethernet connection 1st.
//then start the Arduino IDE. Then plug in your USB cable to the Featherboard. This sometimes help resolving
//the issues where the com port is not showing up or device not connected and failing upload.
//you should see something like /dev/cu.usbmodem14101 in the PORT setting
//of you see other than that or something like /dev/cu.Bluetooth-incoming-port then you will not be able to
//download!

//define the variable called "display" as the OLED so if you called this tft = Adafruit_SH1107 you will use tft.drawPixel() for example.
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire); // define the OLED variable as variable called "display"
Adafruit_NAU7802 nau; //define the analog to digital unit as variable "nau"
RTC_PCF8523 rtc;
File myFile;
int pointer=0, settingmax=2;
String data;
String dataarray[2]; //use pointermax for size of array, this depends on how many settings you need to use.
int timesetting;
float target_g;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define BUTTON_A  9 //default button for Feather M4 change accordingly
#define BUTTON_B  6 //default button for F M4
#define BUTTON_C  5 //default button for F M4

#define VBATPIN A6 // for battery volt reading; shared with BUTTON_A caution; see Adafruit pin out for feather
// this may not be necessary after we figure out if the board use MAX1704x or LC09203F

volatile int Button_APressed,Button_BPressed,Button_CPressed;

void setup() {  
  Serial.begin(9600); // not really necessary for the actual product but always useful to include Serial for troubleshooting.
  
  Serial.println("128x64 OLED FeatherWing test");
  delay(250); // wait for the OLED to power up
  display.begin(0x3C, true); // Address 0x3C default

  Serial.println("OLED begun");

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display(); //display all the above
  delay(1000);
  // Clear the buffer.
  display.clearDisplay();
  display.display();
  display.setRotation(1);
  pinMode(BUTTON_A, INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(BUTTON_A), DetectButton_A, FALLING);
  pinMode(BUTTON_B, INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(BUTTON_B), DetectButton_B, FALLING);
  pinMode(BUTTON_C, INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(BUTTON_C), DetectButton_C, FALLING);

//  pinMode(BUTTON_TAR, INPUT_PULLUP);

  

  // text display tests
 // display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.display(); // actually display all of the above
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Mulawarman-Grinder v15");display.println("October 2024, Maple Grove, MN, USA");
  display.println("FeatherM4Express");
  display.display();
  //more text here if needed 
  delay(1000);
  display.clearDisplay();
  display.setCursor(0,0);
  Serial.println("NAU7802");
  display.println("NAU7802");

  if (! nau.begin()) {
    Serial.println("Failed to find NAU7802");
    display.println("Failed to find NAU7802");
  }
  Serial.println("Found NAU7802");
  display.println("Found NAU7802");display.display();

  nau.setLDO(NAU7802_3V0); //set according to the NAU you purchase
  nau.setGain(NAU7802_GAIN_128); // higher gain more accurate measurement, for other selection check the example in Adafruit
  nau.setRate(NAU7802_RATE_10SPS);

  // Take 10 readings to flush out readings
  for (uint8_t i=0; i<10; i++) {
    while (! nau.available()) delay(1);
    nau.read();
  }

  while (! nau.calibrate(NAU7802_CALMOD_INTERNAL)) {
    Serial.println("Failed to calibrate internal offset, retrying!");
    display.println("Failed to calibrate internal offset, retrying!");display.display();
    delay(1000);
  }
  Serial.println("Calibrated internal offset");
  display.println("Calibrated internal offset");display.display();

  while (! nau.calibrate(NAU7802_CALMOD_OFFSET)) {
    Serial.println("Failed to calibrate system offset, retrying!");
    display.println("Failed to calibrate system offset, retrying!");
    display.display();
    delay(1000);
  }
  Serial.println("Calibrated system offset");
  display.println("Calibrated system offset");
  

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    display.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  rtc.start();


  if (!SD.begin(10)) {  //CS Pin for Feather is pin 10
    Serial.println("SD Card initialization failed!");
    display.println("SD Card initialization failed!");
  }
  Serial.println("SD Card initialization done.");
  display.println("SD Card initialization done.");
  
  if (SD.exists("Grindset.txt")) {
    Serial.println("Grindset.txt exists.");
    display.println("Grindset.txt exists.");
  } else {
    Serial.println("Grindset.txt doesn't exist.");
    display.println("Grindset.txt doesn't exists.");
    Serial.println("Creating Grindset.txt");
    display.println("Creating Grindset.txt");

    myFile = SD.open("Grindset.txt", FILE_WRITE);
    myFile.close();
  }

  if (SD.exists("Grindset.txt")) {
    Serial.println("Grindset.txt creation confirmed.");
    display.println("Grindset.txt creation confirmed.");

  } else {
    Serial.println("Grindset.txt creation failed.");
    display.println("Grindset.txt creation failed.");
  }
  myFile = SD.open("Grindset.txt");
  if (myFile) {
    Serial.println("Grindset.txt:");
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    myFile.close();
  } else {
    Serial.println("error opening Grindset.txt");
  }

  // this section read the Grindset.txt and read the first 2 rows
  // and stored it into String arrays
  // data is stored in the following order : 
  // array[0] = target weight, 1st row in the text file
  // array [1] = target time, 2nd row in the text file
  myFile = SD.open("Grindset.txt");
  while (pointer<settingmax) {
    char c= myFile.read();
    if (isPrintable(c)) {
      data.concat(c);
    } 
    else if ( c == '\n')
    {
      dataarray[pointer]=data;
      data=""; pointer++;
    }
  }
  Serial.print("Read from SD 1st row: ");Serial.println(dataarray[0]);
  Serial.print("Read from SD 2nd row: ");Serial.println(dataarray[1]);
  target_g=dataarray[0].toFloat();
  timesetting=dataarray[1].toInt();
  Serial.print("Assign array 0 to setting target_g: ");Serial.println(target_g);
  Serial.print("Assign array 1 to setting timesetting: ");Serial.println(timesetting);

  display.display();display.clearDisplay();



} // end setup



//next are all the customizable variables some are used to display default settings or parameters
int n = 1; float nmax=10.0;
float offset_values=978.73; // update this after manual calibration 976.97
float valtotal=0;
float average_val=0;
float displayed_g=0.0, frozen_g=0.0;
float nau_adjusted_g=0.0;
float stored_nau_adjusted_g=0.0;
int timebase=0,weightbase=0,modecounter=1,changecounter=0,ready,updatetime=0, onetime,increase=0,decrease=0;
unsigned long time_at_tar;  
//int timesetting; as of v12 the setting of time and weight has been moved up into the setup loop, stored in SD card
int displayedtime,frozentime; //30000ms = 30s default timer
//float target_g; // example 16.00 g as of v12 the setting of time and weight has been moved into the setup loop, stored in SD card
char modechar='g',changechar='T';
int rectangle_x_start=0,rectangle_y_start=24;
int rectangle_x_width=127, rectangle_y_width=40;
int relaypin = 11;

// loop start
void loop() {
  float val = nau.read();
  pinMode(relaypin, OUTPUT);  
  DateTime now=rtc.now();
  
  //Serial.print(now.year(),DEC);Serial.print('/');
  //Serial.print(now.month(),DEC);Serial.print('/');
  //Serial.print(now.day(),DEC);Serial.print(" (");
  //Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  //Serial.print(") ");
  //Serial.print(now.hour(), DEC);Serial.print(':');
  //Serial.print(now.minute(), DEC);Serial.print(':');
  //Serial.print(now.second(), DEC);Serial.println();
  
  // Battery volt current reading
  digitalWrite(VBATPIN,LOW);
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  Serial.print("VBat: " ); Serial.println(measuredvbat);
  display.setCursor(0,0);display.println("");
  //             0123456789012345678901  
  display.print("                ");
  display.print(measuredvbat);display.println("V");
  digitalWrite(VBATPIN,HIGH);

  while (! nau.available()) {
    delay(1);
  }
  while (n<=nmax) {
    valtotal=valtotal+nau.read()/offset_values;
    n=n+1;
    delay(5);
  }
  average_val=valtotal/nmax;
  n=1;
  valtotal=0.0;
 
  nau_adjusted_g=average_val;
  Serial.print("Read "); Serial.print(val/offset_values,1); Serial.print(" average = ");Serial.print(average_val,1);
  Serial.print(" actual grams = "); Serial.println(nau_adjusted_g,1);
   
  displayed_g=nau_adjusted_g-stored_nau_adjusted_g;
  Serial.print("displayed_g ");Serial.print(displayed_g);
  
  if (displayed_g>0.20 && displayed_g<=target_g && ready==1 && (weightbase==1 || timebase==1)){
    updatetime=1;
    while (onetime==1) {
     time_at_tar=millis();onetime=0;}
    
  }
 
  if (updatetime==1 && ready==1) { //if TAR is pressed and time is updating
    displayedtime=int(millis())-int(time_at_tar);frozentime=displayedtime;
    frozen_g=displayed_g;
    update1(displayedtime,displayed_g);
  }
  else if (updatetime==0 && ready==0) { // if TAR is not present/pressed and time is not updating, just update the weight, but not increment the time
    //update1(frozentime,frozen_g); disabling this and switching with the else below to see if this allow update of weight without TAR (ready=1)
    update1(frozentime,displayed_g);
  }
  else  { // if it does not meet any the conditions aboved, do not update the weight and the time
    //update1(frozentime,displayed_g); disabling this since it make sense 
    update1(frozentime,frozen_g);
  }
     
  Serial.print("displayedtime ");Serial.print(displayedtime/1000);Serial.print(" ");Serial.println(int(64-(displayed_g/target_g)*rectangle_y_start));
  display.drawLine(rectangle_x_start,64-rectangle_y_start,64,64-rectangle_y_start,1); //display line representing target_g
  display.drawLine(int(timesetting/1000),rectangle_y_start,int(timesetting/1000),64,1); //display line representing timesetting
  
  if ((displayedtime/1000)<=rectangle_x_width && (64-(displayed_g/target_g)*rectangle_y_start)>=rectangle_y_start){
    display.drawPixel(int(displayedtime/1000),int(64-(displayed_g/target_g)*rectangle_y_start),1);//displaying trace/pixel
  }

  display.setCursor(0,0); // line1
  display.print(timesetting);display.print("ms");display.print(" ");
  display.print(target_g,1);display.print("g");display.print(" Mode:");display.print(modechar);display.println(changechar);
  Serial.print("displayed_g ");Serial.print(displayed_g);Serial.print(" target_g ");Serial.print(target_g,1);Serial.print(" Update=");Serial.print(updatetime);
  Serial.print(" Ready=");Serial.print(ready);Serial.print(" Onetime=");Serial.println(onetime);
  display.display();
  
  if (displayed_g>=target_g && weightbase==1 && ready==1){
    frozen_g=displayed_g;
    update1(frozentime,frozen_g);
    relaycontrol();
  }

  if (displayedtime>=timesetting && timebase==1 && ready==1){
    frozen_g=displayed_g;
    update1(frozentime,frozen_g);
    relaycontrol();
  }

  if (Button_APressed) {
    if (weightbase==1 && increase==1){
      target_g=target_g+.1;
    }
    else if (timebase==1 && increase==1){
        timesetting=timesetting+100;
    }
    else if (weightbase==1 && decrease==1){
      target_g=target_g-0.1;
    }
    else if (timebase==1 && decrease==1){
      timesetting=timesetting-100;
    }
    else if (increase==0 && decrease==0) {
      Serial.println("TAR");
      stored_nau_adjusted_g=nau_adjusted_g;
      displayedtime=0;
      time_at_tar=millis();
      ready=1; onetime=1;frozentime=0;updatetime=0;
      cleangrapharea();
      updatesettings(target_g,timesetting);
    }
    Button_APressed=0;
  }
  
  if (Button_BPressed) {
    Serial.print("B ");Serial.print(changecounter);Serial.print(" ");
    if (changecounter==0){
      changecounter=1; Serial.println(changecounter);
    }
    else if (changecounter==1){
      changecounter=2; Serial.println(changecounter);
    }
    else if (changecounter==2){
      changecounter=0; Serial.println(changecounter);
    }
    Button_BPressed=0;
  }

  if (Button_CPressed) {
    Serial.print("C ");Serial.print(modecounter);Serial.print(" ");
    if (modecounter==0){
      modecounter=1; Serial.println(modecounter);
    }
    else if (modecounter==1){
      modecounter=2;Serial.println(modecounter);
    }
    else if (modecounter==2){
      modecounter=0;Serial.println(modecounter);
    }
    Button_CPressed=0;
  }

  switch (modecounter) {
    case (0):
      weightbase=0;timebase=0;modechar=' ';break;
    case (1):
      weightbase=1;timebase=0;modechar='g';break;
    case (2):
      weightbase=0;timebase=1;modechar='s';break;
  }
  switch (changecounter) {
    case (0):
      increase=0;decrease=0;changechar='T';break;
    case (1):
      increase=1;decrease=0;changechar='+';break;
    case (2):
      increase=0;decrease=1;changechar='-';break;
  }

  

  if (ready==1) {
    display.fillCircle(120,60,3,1);display.display();
  }
  else {
    display.fillCircle(120,60,3,0);display.display();
  }

  display.display();
  //display.clearDisplay(); 
  display.fillRect(0,0,rectangle_x_width,rectangle_y_start,0); // instead of using clearDisplay, drawing a black rectangle over the area that need to be cleared is easy
  // this allow some section to be cleared and some not, the area where the Pixel is drawn is not cleared so it create nice trace of the weight
  delay(100);



}

void update1(int time,float weight) {
    display.setCursor(0,0);display.println();
    // line2           
    display.print(time);
    display.print("ms");
    display.setCursor(0,0);display.println();
    //             0123456789012345678901
    //             30000ms 
    display.print("        ");
    display.print(weight,1);display.println("g ");
    display.print("             ");
    display.print(weight/(time/1000),1);display.println("g/s");//not a quite accurate measurement of flow rate but can be improved later
    //line 3
    //             0123456789012345678901
    //             30000ms 14.0g
    //display.print("           ");display.print("Ratio ");display.print(weight/target_g,1); there is no point of displaying ratio if the weight is always close to target
    //only useful I suppose if it is based on time

    //plot the point
    if ((time/1000)<=rectangle_x_width && (64-(weight/target_g)*rectangle_y_start)>=rectangle_y_start){
    display.drawPixel(int(time/1000),int(64-(weight/target_g)*rectangle_y_start),1);//displaying trace/pixel
    }
    display.display();
}

void relaycontrol() {
    display.setCursor(0,0);display.println();display.println();display.println("Grinder off");display.display();
    //turn relay on to open contact
    digitalWrite(relaypin, HIGH);
    Serial.println("Grinder off");
    TimeoutTimer(30);
    //delay(30000); // keep open contact for 30s; // maybe good to make the delay time a parameter that can be updated from the UI
    //then display a timer that counts down
    digitalWrite(relaypin, LOW);
    Serial.println("Grinder on");
    updatetime = 0; // stop updating timer;
    ready=0; // require TAR to be push again  
}

void cleangrapharea() {
  // function to clean only graph area
  display.fillRect(rectangle_x_start,rectangle_y_start,64*2,rectangle_y_width,0);
}

void updatesettings(float target_g,int timesetting) {
  // write new settings to SD card
  SD.remove("Grindset.txt");
  myFile.close();
  myFile = SD.open("Grindset.txt", FILE_WRITE);
  myFile.println(target_g);
  myFile.println(timesetting);
  myFile.close();
}

void TimeoutTimer (int timeout) {
  while (timeout>=0) {
    Serial.println(timeout);
    display.setCursor(0,0);display.println();display.println();display.println();display.println();
    display.print("            ");display.setTextSize(3);
    display.println(timeout);
    display.setTextSize(1);
    display.display();
    timeout=timeout-1;
    delay(1000);
    display.fillRect(rectangle_x_start+64,rectangle_y_start,64,rectangle_y_width,0);
  }
}

void DetectButton_A() {
  Button_APressed = 1;
}

void DetectButton_B() {
  Button_BPressed = 1;
}

void DetectButton_C() {
  Button_CPressed = 1;
}

