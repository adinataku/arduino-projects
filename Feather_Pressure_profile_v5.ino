// project log, start 2 22 2025
// note the Arduino IDE 2.3.5 nightly 20241212 was used to compile
// it seems the newest one is not compatible with this old mac OS.
// there may be a newer version than 2.3.5 that will still work but certainly
// the latest IDE version is not working as of Feb 2025 due to some
// Python compatibility with old mac OS 10.5 
// to upload into ESP32-S3, hold D0 and press-release Reset, and release D0.
// When program finished uploading, you have to press-release Reset button to start the code.
// start with RTC PFC8523 from Adafruit; removing all notes/comments not needed to keep code concise
// if time drift too far just upload again
// add TFT Feather example from Adafruit
// add MAX1704X example
// add RTC example 
// 
// v1 added additional graphic pages and fixed start up by commenting out waiting for serial connection to happen
// v2 added arrays to store pressure profile into memory card, 
// v3 added ability to load default profile file, and store or change where to store the profile.
// v4 added ability to plot back the stored profile file
// v5 using interrupts to detect button pressed fore better performance

// Date and time functions using a PCF8523 RTC connected via I2C and Wire lib
#include "RTClib.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include "Adafruit_MAX1704X.h"
#include <Adafruit_NeoPixel.h>
#include "SdFat.h"
#include <limits.h>


#define SD_CS_PIN 10 // if using the feather esp32-s3 use 10 for Chip Select Pin

RTC_PCF8523 rtc;
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_MAX17048 maxlipo;
//GFXcanvas16 canvas(240,135);
File myFile;
File myProfileFile;
File myTempFile;
SdFat SD;
int reading_index=0;
char delim=','; //update this from coma to semicolon if using ; for delimiter
char filename[12]; // max file name in SD card limited to 12


char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int pressure_sensor=A5;
float pressure_sensor_value = 0;
float pressure_in_bar,pressure_in_pa;

float bar=0.0;
int bar_line=0;
int elapsed_time=0;
int start_time=0;
int timer_run=0;
int pause_time=0;
int update_time=0;
String dfilename;

volatile int Button0Pressed,Button1Pressed,Button2Pressed;

void setup () {
  Serial.begin(57600);
// disabling the next 3 lines allow the code to start and display without having to be plugged into
//#ifndef ESP8266
//  while (!Serial); // wait for serial port to connect. Needed for native USB
//#endif

  //float drift = 0; // seconds plus or minus over oservation period - set to 0 to cancel previous calibration.
  //float period_sec = (7 * 86400);  // total obsevation period in seconds (86400 = seconds in 1 day:  7 days = (7 * 86400) seconds )
  //float deviation_ppm = (drift / period_sec * 1000000); //  deviation in parts per million (μs)
  //float drift_unit = 4.34; // use with offset mode PCF8523_TwoHours
  // float drift_unit = 4.069; //For corrections every min the drift_unit is 4.069 ppm (use with offset mode PCF8523_OneMinute)
  //int offset = round(deviation_ppm / drift_unit);
  // rtc.calibrate(PCF8523_TwoHours, offset); // Un-comment to perform calibration once drift (seconds) and observation period (seconds) are correct
  // rtc.calibrate(PCF8523_TwoHours, 0); // Un-comment to cancel previous calibration
  //Serial.print("RTC Offset is "); Serial.println(offset); // Print to control offset

  // turn on backlite
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // initialize TFT
  tft.init(135, 240); // Init ST7789 240x135
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  //canvas.setFont(&FreeSans12pt7b);
  //canvas.setTextColor(ST77XX_WHITE); 
  tft.setTextWrap(false);
  tft.setCursor(0, 0);tft.setTextColor(ST77XX_BLUE);tft.setTextSize(2);

  Serial.println(F("ST7789 TFT Initialized"));tft.println(F("ST7789 TFT Initialized"));

  while (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    tft.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    delay(2000);
  }
  Serial.print(F("Found MAX17048"));tft.print(F("Found MAX17048"));
  Serial.print(F(" with Chip ID: 0x"));tft.print(F(" with Chip ID: 0x"));
  Serial.println(maxlipo.getChipID(), HEX);tft.println(maxlipo.getChipID(), HEX);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");tft.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (! rtc.initialized() || rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");tft.println("RTC is NOT initialized, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  rtc.start();
  
  Serial.println("Initializing SD card...");tft.println("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("initialization failed!");tft.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");tft.println("initialization done.");
  pinMode(0, INPUT_PULLUP);
  pinMode(1, INPUT_PULLDOWN);
  pinMode(2, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(0), DetectButton0, FALLING);
  attachInterrupt(digitalPinToInterrupt(1), DetectButton1, RISING);
  attachInterrupt(digitalPinToInterrupt(2), DetectButton2, RISING);

  delay(2000);
}

void loop () {
  DateTime now = rtc.now();
  float cellVoltage = maxlipo.cellVoltage();
  //Serial.print(now.year(), DEC);Serial.print('/');Serial.print(now.month(), DEC);Serial.print('/');Serial.print(now.day(), DEC);
  //Serial.print(" (");Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);Serial.print(") ");
  //Serial.print(now.hour(), DEC);Serial.print(':');Serial.print(now.minute(), DEC);Serial.print(':');Serial.print(now.second(), DEC);Serial.println();

  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);tft.setCursor(0, 0);tft.setTextColor(ST77XX_GREEN);tft.setTextSize(2);
  tft.print(now.year(), DEC);tft.print('/');tft.print(now.month(), DEC);tft.print('/');tft.print(now.day(), DEC);
  tft.print(" (");tft.print(daysOfTheWeek[now.dayOfTheWeek()]);tft.println(") ");
  tft.print(now.hour(), DEC);tft.print(':');tft.print(now.minute(), DEC);/*tft.print(':');tft.print(now.second(), DEC)*/;tft.println();

  if (isnan(cellVoltage)) {
    Serial.println("Failed to read cell voltage, check battery is connected!");
    delay(2000);
    return;
  }
  //Serial.print(F("Batt Voltage: ")); Serial.print(cellVoltage, 3); Serial.println(" V");
  //Serial.print(F("Batt Percent: ")); Serial.print(maxlipo.cellPercent(), 1); Serial.println(" %");
  //Serial.println();
  
  tft.print(F("Batt: ")); tft.print(cellVoltage, 1); tft.print(" V,");
  tft.print(maxlipo.cellPercent(), 0); tft.println(" %");

  //print default profile by opening and reading default_profile_setting.txt
  myProfileFile=SD.open("default_profile_setting.txt");

  if (!myProfileFile){ // if there is no file called default_profile_setting.txt then create a blank one
    myProfileFile=SD.open("default_profile_setting.txt",FILE_WRITE);
    myProfileFile.println("profile1.csv");// comment out after 1st run, it is here just to help create the example.
    myProfileFile.close();
  }

  tft.print("Pfile=");
  myProfileFile=SD.open("default_profile_setting.txt");
  
  while (myProfileFile.available()){
    //read 1 char at a time and store it in char array for tft.print
    filename[reading_index]=myProfileFile.read();
    tft.print(filename[reading_index]);
    reading_index++;
  }
  reading_index=0;
  myProfileFile.close();

  printnavmenu();
  //tft.setTextWrap(true);tft.setTextColor(ST77XX_GREEN);
  //tft.setCursor(0,120);
  //tft.println("D0:up, D1:down, D2:select");

  //Serial.println(digitalRead(0));
  //Serial.println(digitalRead(1));
  //Serial.println(digitalRead(2));
  if (Button0Pressed) {
    Button0Pressed=0;delay(250);
    page3();
  }
  
  else if (Button1Pressed) {
    Button1Pressed=0;delay(250);
    page1();
  }
  else if (Button2Pressed) {
    Button2Pressed=0;delay(250);
    selectprofile();
  }
  Button0Pressed=0;/* this added because it seems the button pressed is debouncing and 
  the microcontroller still thinks the button 0 is still pressed and go to page3() all the time after exiting other pages like the selectprofile() function
  */
  delay(1000);
  /*Button0Pressed=0;/* this added because it seems the button pressed is debouncing and 
  the microcontroller still thinks the button 0 is still pressed and go to page3() all the time after exiting other pages like the selectprofile() function
  */

}

void page1() {
  // page 1; display digital pressure reading and time in text
  
  while (true) {
    tft.setTextColor(ST77XX_YELLOW);
    DateTime now = rtc.now();
    tft.fillScreen(ST77XX_BLACK);tft.setCursor(0,0);tft.setTextSize(4);
    readpressure();
    Serial.print("Pressure_sensor_V:");
    Serial.print(pressure_sensor_value);Serial.print(", ");
    Serial.print("Pressure_sensor_bar:");
    Serial.print(pressure_in_bar);Serial.print(", ");// option to pressure_in_bar
    Serial.print("Pressure_sensor_pa:");
    Serial.println(pressure_in_pa);
    tft.print(pressure_in_bar,1);tft.println(" bar");
    tft.print(pressure_in_pa,0);tft.println(" pa");
    tft.setTextColor(ST77XX_WHITE);
    tft.print(now.hour(), DEC);tft.print(':');tft.print(now.minute(), DEC);tft.print(':');tft.print(now.second(), DEC);tft.println();
    printnavmenu();
    delay(500);
    if (Button0Pressed) {
      //go back to main/start up menu
      Button0Pressed=0;
      return;
    }
    else if (Button1Pressed) {
      // if there is page2 go there
      Button1Pressed=0;
      page2();
      
    }
    else if (Button2Pressed) {
      Button2Pressed=0;
      subpage1_1();
      tft.setTextColor(ST77XX_YELLOW);
      tft.setTextSize(4);
      //delay (100);
    }
    else
    {

    }
  //delay (250);  
  }
}

void printnavmenu() {
  tft.setTextSize(2);
  tft.setTextWrap(true);tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(0,120);
  tft.println("D0:up, D1:dn, D2:Act");
  return;
}

void subpage1_1() {    

  while (true) {  // repeat this while loop until D0 is pressed then exit to page 1
    readpressure();

    if (Button1Pressed or (pressure_in_bar>0.25) and timer_run==0) {
     Button1Pressed=0;
     start_time=millis()/1000.0; // push D1 to start timer or increase pressure_in_bar > 0.25
     timer_run=1;   // indicate timer is running
     update_time=1;
    }
    else if (Button1Pressed and timer_run==1 and update_time==1) { //benefit of else if vs if is else if does not get processed if the condition in the if statement is true
      Button1Pressed=0;
      update_time=0; // if D1 is pushed again while timer_run=1; then pause the elapsed time, don't update anymore
    // but the timer actually continues to elapsed in the background, just not updating.
    }
    else if (Button1Pressed and timer_run==1 and update_time==0) {
      Button1Pressed=0;
      update_time=1; // if D1 is pushed again after timer is paused, it will unpaused and update the time to the latest or more recent time
    
    }
    else {
    //do nothing yet
    }

    if (Button2Pressed) { // push D2 to reset timer to 0
     Button2Pressed=0;
     timer_run=0; // indicate timer is not running
     elapsed_time=0; // display 0 and don't restart timer until D0 is pressed

    }
    
    tft.fillScreen(ST77XX_BLACK);tft.setCursor(0,0);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(6);
    tft.print(pressure_in_bar,1);
    tft.setTextSize(4);
    tft.setCursor(0,50);
    tft.print("bar");
    tft.setCursor(140,0);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(6);
    
    if (timer_run==1 and update_time==1) { // update timer display if timer running but not paused
      elapsed_time=millis()/1000-start_time; // print elapsed time since start of timer
    }

    // pause and then continue timer concept, must design so there is delay otherwise pause bit get set right away

    tft.print(elapsed_time,0);
    tft.setCursor(140,50);
    tft.setTextSize(4);
    tft.print("sec");
    //scale
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(0,90);
    tft.setTextSize(1);
    tft.println("0   1   2   3   4   5   6   7   8   9");
      tft.print("|...|...|...|...|...|...|...|...|...|...");
    bar_line=round(24*(pressure_in_bar-1)+24)*1;//x2 for temporary magnification, remove when installed
    Serial.print(pressure_in_bar);Serial.print(",");Serial.print(bar_line);Serial.println(update_time);
    tft.drawRect(bar_line,115,4,25,ST77XX_YELLOW);
    tft.fillRect(bar_line,115,4,25,ST77XX_YELLOW);  
    //bar=bar+0.1;
    delay(500);
    if (Button0Pressed){
      Button0Pressed=0;
      return;
    }
  }
  
  //return;
}

void page2() {
  while (true) {  // repeat this while loop until D0 is pressed then exit to page 1, no option to go to page 3 from here
    readpressure();    
    
    //D1 in this page is used to start timer and paused timer, D2 is to reset timer to 0
    if (Button1Pressed or (pressure_in_bar>0.25) and timer_run==0) {
     Button1Pressed=0;
     start_time=millis()/1000.0; // push D1 to start timer or increase pressure_in_bar to above 0.25
     timer_run=1;   // indicate timer is running
     update_time=1;
    }
    else if (Button1Pressed and timer_run==1 and update_time==1) { //benefit of else if vs if is else if does not get processed if the condition in the if statement is true
      Button1Pressed=0;
      update_time=0; // if D1 is pushed again while timer_run=1; then pause the elapsed time, don't update anymore
    // but the timer actually continues to elapsed in the background, just not updating.
    }
    else if (Button1Pressed and timer_run==1 and update_time==0) {
      Button1Pressed=0;
      update_time=1; // if D1 is pushed again after timer is paused, it will unpaused and update the time to the latest or more recent time
    }
    else {
    //do nothing yet
    }

    if (Button2Pressed) { // push D2 to reset timer to 0
     Button2Pressed=0;
     timer_run=0; // indicate timer is not running
     elapsed_time=0; // display 0 and don't restart timer until D0 is pressed
    }
    if (timer_run==1 and update_time==1) { // update timer display if timer running but not paused
      elapsed_time=millis()/1000-start_time; // print elapsed time since start of timer
    }
    
    //
    //start preparing page for plots for pressure_in_bar,elapsed_time
    tft.fillScreen(ST77XX_BLACK);tft.setCursor(0,0);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(4);
    tft.print(pressure_in_bar,1);
    tft.println(" bar");
    tft.setTextColor(ST77XX_WHITE);
    tft.print(elapsed_time);
    tft.println(" s");
    printnavmenu();
    delay(500);
    if (Button0Pressed) {
      Button0Pressed=0;
      return;
    }
  }
  
}

void page3() {
  int plot=0;
  int x_values=0;
  float min_bar_for_plot=0.5;
  int sampling_period=250; //in ms, this sampling period ties directly to a single point plotted so with
  // 240 max width, 240 x 500ms = 120,000 ms = 120s = 2 min length of record.
  // a good starting point as most shots will be less than 1 min.
  // a 1000 sampling period will mean 240 x 1000ms = 240,000 ms = 240s = 4 min, a bit too much/long
  // a sampling rate of 250, 240 x 250 ms = 60,000 ms = 60s = 1 min length of record.
  int Y_coord_bottom_black_screen=120;
  int Y_coord_bottom_zero_bar=118;
  dfilename="";
  page3print_y_axis_value ();
  printnavmenu();
  tft.fillRect(14,0,240,Y_coord_bottom_black_screen,ST77XX_BLACK);  
  readpressure();
  tft.setCursor(110,0);tft.setTextSize(2);tft.setTextColor(ST77XX_YELLOW);
  tft.fillRect(20,0,120,36,ST77XX_BLACK);  
  tft.print(pressure_in_bar);tft.println(" bar");
  tft.setCursor(110,15);
  myProfileFile=SD.open("default_profile_setting.txt");
  while (myProfileFile.available()){
    //read 1 char at a time and store it in char array for tft.print
    filename[reading_index]=myProfileFile.read();
    tft.print(filename[reading_index]);
    dfilename+=filename[reading_index];
    reading_index++;
  }
  Serial.println(dfilename);
  reading_index=0;
  myProfileFile.close();
      
  myFile = SD.open(dfilename); // open the file found in default profile 
  // plot the content
  int16_t sample_no; 
  float bars_stored, sensorsv_stored;
  myFile.seek(0);
  while (myFile.available()) {
    if (csvReadInt16(&myFile, &sample_no, delim) != delim 
    || csvReadFloat(&myFile, &bars_stored, delim) != delim 
    || csvReadFloat(&myFile, &sensorsv_stored, delim) != '\n') {
      Serial.println("read error");
      int ch;
      int nr = 0;
      // print part of file after error.
      while ((ch = myFile.read()) > 0 && nr++ < 10) {
        Serial.write(ch);
      }
      Serial.println();
      break;
    }
  tft.drawCircle(sample_no+15,Y_coord_bottom_zero_bar-round(bars_stored*10),1,ST77XX_GREEN);
  }    
  myFile.close();// after plotting data from shown default profileX.csv close the file,

  while ((pressure_in_bar<min_bar_for_plot) and (plot==0)) {
    Serial.println("Waiting for pressure in bar > min_bar_for_plot. Current pressure in bar = ");
    readpressure();    //just wait and do nothing until pressure is above min_bar_for_plot
    Serial.print("Pressure in bar ");Serial.println(pressure_in_bar);
    if (pressure_in_bar>=min_bar_for_plot) {
      plot=1;
      Serial.print("Min pressure to start plot met at ");Serial.println(pressure_in_bar);
      // create a file in SD card to store profile    
      myTempFile = SD.open("temp.csv",FILE_WRITE|O_TRUNC); // use temporary temp.csv file to store run time pressure
      if (myTempFile){ 
        Serial.println("Writing to profile file in SD card ready");
        //print headers in the temporary file
        //myTempFile.print("index");myTempFile.print(",");
        //myTempFile.print("Pbar");myTempFile.print(",");
        //myTempFile.println("SensorValue");
      }
    } 
    delay(500);
    if (Button0Pressed){
      Button0Pressed=0;
      return;// without saving 
    }
    delay(sampling_period);
  }
  

  while ((plot==1) and ((x_values+15)<=240)){ // IF PRESSURE ABOVE 0.25 START PLOTTING, IF D1 NOT PRESSED
    readpressure();
    Serial.println("Plotting now. Sampling is once/every second. Pressure is "); Serial.print(pressure_in_bar);Serial.print("--");
    Serial.println(Y_coord_bottom_zero_bar-round(pressure_in_bar*10));//remove *10 multiplier when maginfication not needed
    tft.drawCircle(x_values+15,Y_coord_bottom_zero_bar-round(pressure_in_bar*10),1,ST77XX_RED);
    tft.setCursor(20,0);tft.setTextSize(2);tft.setTextColor(ST77XX_YELLOW);
    tft.fillRect(20,0,120,36,ST77XX_BLACK);  
    tft.print(pressure_in_bar);tft.println(" bar");
    tft.setCursor(20,15);tft.print(dfilename);// reprint the loaded profile file name
    
    // store the reading values in the temporary file
    myTempFile.print(x_values);myTempFile.print(",");
    myTempFile.print(pressure_in_bar);myTempFile.print(",");
    myTempFile.println(pressure_sensor_value);
    x_values++;Serial.println(x_values);
    if (Button2Pressed){ //D2 TO RESET AND READY TO PLOT AGAIN
      Button2Pressed=0;
      x_values=0;
      tft.fillRect(14,0,240,Y_coord_bottom_black_screen,ST77XX_BLACK);
      Serial.println("D2 pressed, plot is reseted.");
      myTempFile.close();
      myTempFile = SD.open("temp.csv",FILE_WRITE|O_TRUNC); // use temporary temp.csv file to store run time pressure
      if (myTempFile){ 
        Serial.println("Writing to profile file in SD card ready");
        //print headers in the temporary file
        //myTempFile.print("index");myTempFile.print(",");
        //myTempFile.print("Pbar");myTempFile.print(",");
        //myTempFile.println("SensorValue");
      }
      //make it wait until min bar pressure is reach
      while (pressure_in_bar<min_bar_for_plot) {
        //after reset wait in this while loop and keep checking pressure until min pressure is met again
        Serial.println("Waiting for new reading to meet min bar, increase pressure or press D0 to exit to main menu");
        readpressure();
        delay(sampling_period);
        if (Button0Pressed){
          Button0Pressed=0;
          Serial.println("D0 is pressed, go back to main menu");
          return;
        }
      }
    }
    delay(500);
    if (Button0Pressed){
      Button0Pressed=0;
      return;
    }
    delay(sampling_period); // change this to smaller ms to sample at higher rate, 1000 mean sampling once every 1s.
  }

  while (x_values+15>240){
    Serial.println("Waiting at end of plot, D2 for reset and plot again or D0 to save profile and go back to main menu");
    // wait until d0 is pressed to exit the window or 
    
    if (Button0Pressed){ //D0 TO GO BACK TO main page and save profile to temp.csv
      Button0Pressed=0;
      Serial.println("D0 is pressed, back to main menu");
      myTempFile.close(); // after this figure out how to move temp.csv content to the default profile file
      SD.remove(dfilename); // delete the default profile name
      SD.rename("temp.csv",dfilename); // rename the temp.csv to the default file name
      Serial.println("Profile written to SD card.");
      tft.setCursor(20,30);tft.print("Profile written to SD card.");
      delay(2000);
      return;
    }
    else if (Button2Pressed){  // no saving of profile then, temp.csv does not get copied into default profile file.
      Button2Pressed=0;
      Serial.println("D2 is pressed after plot is finised so do nothing");
      tft.setCursor(20,30);tft.print("Profile not written to SD card.");
      //delay(2000);
      //reset 
      x_values=226;//anything to break the while condition 15+240=225, so 226 will be fine.
    }
    else if (Button1Pressed){
      Button1Pressed=0;
      tft.setCursor(20,30);tft.print("Profile not written to SD card.");
      //delay(2000);
      page2(); // no saving of profile also here.
    }
    else {
    // do nothing
    }
  }
}

void page3print_y_axis_value () {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(0,0);tft.print("12");
    tft.setCursor(0,9);tft.print("11");
    tft.setCursor(0,18);tft.print("10");
    tft.setCursor(0,27);tft.print("9");
    tft.setCursor(0,36);tft.print("8");
    tft.setCursor(0,45);tft.print("7");
    tft.setCursor(0,54);tft.print("6");
    tft.setCursor(0,63);tft.print("5");
    tft.setCursor(0,72);tft.print("4");
    tft.setCursor(0,81);tft.print("3");
    tft.setCursor(0,90);tft.print("2");
    tft.setCursor(0,99);tft.print("1");
    tft.setCursor(0,108);tft.print("0");
    //tft.setCursor(0,117);tft.print("2");
    //tft.setCursor(0,126);tft.print("1");
    //tft.setCursor(0,135);tft.print("0");
    return;
}

void readpressure() {
  //call this function everytime pressure need to be read from analog input
  // reading return an average of # sample
  int maxread=10;//higher number reduce jitters/transient noise but the delay is 5ms, so 10 x 5 = 50ms additional delay in reading
  int read=1;
  float total_read=0.0;
  pressure_sensor_value=0;  
  while (read<=maxread){
    Serial.print("readcount ");Serial.print(read);
    pressure_sensor_value=analogRead(pressure_sensor);
    Serial.print(" sensor_v=");Serial.print(pressure_sensor_value);
    total_read=total_read+pressure_sensor_value;
    Serial.print(" total read=");Serial.println(total_read);
    read++;
    delay(5);
  }
  pressure_sensor_value=total_read/maxread;
  Serial.print("Average pressure sensor v=");Serial.println(pressure_sensor_value);
    //pressure_sensor_value = analogRead(pressure_sensor);
  pressure_in_bar = (10.34-0)/(4500.0-500.0)*(pressure_sensor_value-500.0)+0.0;
  Serial.print("Average pressure in bar=");Serial.println(pressure_in_bar);
  pressure_in_pa = (150.0-0)/(4500.0-500.0)*(pressure_sensor_value-500)+0.0; 
  return;
}

void selectprofile(){
  delay(1000);
  int select=0;
  tft.fillScreen(ST77XX_BLACK);tft.setCursor(0, 0);tft.setTextColor(ST77XX_GREEN);tft.setTextSize(2);
  tft.println("Current default:");
  myProfileFile=SD.open("default_profile_setting.txt");
  
  while (myProfileFile.available()){
    //read 1 char at a time and store it in char array for tft.print
    filename[reading_index]=myProfileFile.read();
    tft.print(filename[reading_index]);
    reading_index++;
  }
  reading_index=0;
  myProfileFile.close();
  tft.println();
  tft.println("Change default profile file.");

   //currently profile file name is fixed to this format profileX.csv, X number 1-4.
  // scroll down with D1, and D2 to store
  while (true) {
  
    tft.setCursor(0,95);tft.setTextColor(ST77XX_RED);
    if (select==0 and Button1Pressed){
    Button1Pressed=0;  
    select=1;tft.fillRect(0,95,240,15,ST77XX_WHITE);tft.println("profile1.csv");delay(500);}
    else if ((select==1) and Button1Pressed){
    Button1Pressed=0;
    select=2;tft.fillRect(0,95,240,15,ST77XX_WHITE);tft.println("profile2.csv");delay(500);}
    else if ((select==2) and Button1Pressed){
    Button1Pressed=0;
    select=3;tft.fillRect(0,95,240,15,ST77XX_WHITE);tft.println("profile3.csv");delay(500);}
    else if ((select==3) and Button1Pressed){
    Button1Pressed=0;
    select=4;tft.fillRect(0,95,240,15,ST77XX_WHITE);tft.println("profile4.csv");delay(500);}
    else if ((select==4) and Button1Pressed){
    Button1Pressed=0;
    select=1;tft.fillRect(0,95,240,15,ST77XX_WHITE);tft.println("profile1.csv");delay(500);}
    else {}
    printnavmenu();
    if (Button2Pressed) {
      Button2Pressed=0;
      break;// exit this while loop
    }
    if (Button0Pressed) {
      Button0Pressed=0;
      return;
    }
  }
  

  tft.setTextColor(ST77XX_GREEN);
  
  switch (select){
    case 0:
      //do nothing
      break;
    case 1:
    myProfileFile=SD.open("default_profile_setting.txt",FILE_WRITE|O_TRUNC);// O_TRUNC option delete content of file
    myProfileFile.println("profile1.csv");break;
    case 2:
    myProfileFile=SD.open("default_profile_setting.txt",FILE_WRITE|O_TRUNC);
    myProfileFile.println("profile2.csv");break;
    case 3:
    myProfileFile=SD.open("default_profile_setting.txt",FILE_WRITE|O_TRUNC);
    myProfileFile.println("profile3.csv");break;
    case 4:
    myProfileFile=SD.open("default_profile_setting.txt",FILE_WRITE|O_TRUNC);
    myProfileFile.println("profile4.csv");break;
  }
  
  
  myProfileFile.close();
  delay(2000);
  //return;
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


int csvReadText(File* file, char* str, size_t size, char delim) {
  char ch;
  int rtn;
  size_t n = 0;
  while (true) {
    // check for EOF
    if (!file->available()) {
      rtn = 0;
      break;
    }
    if (file->read(&ch, 1) != 1) {
      // read error
      rtn = -1;
      break;
    }
    // Delete CR.
    if (ch == '\r') {
      continue;
    }
    if (ch == delim || ch == '\n') {
      rtn = ch;
      break;
    }
    if ((n + 1) >= size) {
      // string too long
      rtn = -2;
      n--;
      break;
    }
    str[n++] = ch;
  }
  str[n] = '\0';
  return rtn;
}
//------------------------------------------------------------------------------
int csvReadInt32(File* file, int32_t* num, char delim) {
  char buf[20];
  char* ptr;
  int rtn = csvReadText(file, buf, sizeof(buf), delim);
  if (rtn < 0) return rtn;
  *num = strtol(buf, &ptr, 10);
  if (buf == ptr) return -3;
  while (isspace(*ptr)) ptr++;
  return *ptr == 0 ? rtn : -4;
}
//------------------------------------------------------------------------------
int csvReadInt16(File* file, int16_t* num, char delim) {
  int32_t tmp;
  int rtn = csvReadInt32(file, &tmp, delim);
  if (rtn < 0) return rtn;
  if (tmp < INT_MIN || tmp > INT_MAX) return -5;
  *num = tmp;
  return rtn;
}
//------------------------------------------------------------------------------
int csvReadUint32(File* file, uint32_t* num, char delim) {
  char buf[20];
  char* ptr;
  int rtn = csvReadText(file, buf, sizeof(buf), delim);
  if (rtn < 0) return rtn;
  *num = strtoul(buf, &ptr, 10);
  if (buf == ptr) return -3;
  while (isspace(*ptr)) ptr++;
  return *ptr == 0 ? rtn : -4;
}
//------------------------------------------------------------------------------
int csvReadUint16(File* file, uint16_t* num, char delim) {
  uint32_t tmp;
  int rtn = csvReadUint32(file, &tmp, delim);
  if (rtn < 0) return rtn;
  if (tmp > UINT_MAX) return -5;
  *num = tmp;
  return rtn;
}
//------------------------------------------------------------------------------
int csvReadDouble(File* file, double* num, char delim) {
  char buf[20];
  char* ptr;
  int rtn = csvReadText(file, buf, sizeof(buf), delim);
  if (rtn < 0) return rtn;
  *num = strtod(buf, &ptr);
  if (buf == ptr) return -3;
  while (isspace(*ptr)) ptr++;
  return *ptr == 0 ? rtn : -4;
}
//------------------------------------------------------------------------------
int csvReadFloat(File* file, float* num, char delim) {
  double tmp;
  int rtn = csvReadDouble(file, &tmp, delim);
  if (rtn < 0)return rtn;
  // could test for too large.
  *num = tmp;
  return rtn;
}

