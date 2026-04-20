/* transferring board to a Feather from the UNO
IDE 2.3.4
Board esp32 by Espressif v 2.0.17
arduinoFFT.h v2.0.4
Adafruit NeoPixel 1.15.4
Adafruit_ST7789 1.11.0
Adafruit_GFX 1.12.4
v6 adding graphic
v7 adding navigation cross hair on the screen.
v10 adding adjustable frequencies window for crack sound frequency, adjustable gain/magnitude/loudness
v11 can you add zoom feature? can you add counter for crack sound within a few seconds?
v11 128 double is a version that use 128 samples but double type variable for values, 
v11 256 float is a version that use 256 samples but float type variable for values, not able to start properly, need to see if removing #define variables help
v12 128 float continues with attempting to count how many cracks and recording subsequent crack freq*/

#include <Adafruit_NeoPixel.h>
#include "arduinoFFT.h"

#include <Adafruit_ST7789.h> //v1.11.0
#include <Adafruit_GFX.h> //1.12.4


#define PIN_NEOPIXEL 33      // Onboard NeoPixel pin
#define NEOPIXEL_POWER 34    // Power pin for the NeoPixel
#define NUMPIXELS 1

//#define SD_CS_PIN 10 // if using the feather esp32-s3 use 10 for Chip Select Pin

#define SAMPLES 256             // Must be a power of 2, sampling higher than 128 but it seems to crash on bootup. SAMPLING FREQ/SAMPLES=16000/128=125Hz.
// 256 can work but may have to reset a couple times. with 256, the bucket is 62.5
#define SAMPLING_FREQUENCY 16000 // 16kHz sampling = 8kHz max audible frequency


//int Y_MIN=0;                 // Fixed bottom of graph
//int Y_MAX=4095;               // Fixed top of graph (Adjust based on loudness)
int CRACK_THRESHOLD=1000;     // Magnitude threshold to trigger a "Crack" detection
int MAXCRACK_THRESHOLD=4095; // Max Magnitude threshold for scaling
int STEPCHANGECRACK=100;
int AMBIENT_NOISE=500; // Ambient Noise must be lower than crack threshold

ArduinoFFT<float> FFT = ArduinoFFT<float>();
float vReal[SAMPLES];
float vImag[SAMPLES];
unsigned int sampling_period_us;
int locrackf=6000, hicrackf=8000; // this helps display the frequency ranges you are interested in seeing in serial plotter, serialplotter limited to 7 different items
float maxvReal=4095; // max analog output value on this microcontroller

Adafruit_NeoPixel pixel(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

//defining display
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
int TFTmaxY=135;
int TFTmaxX=240;
int barwidth; // 2 for 128 samples. 1 for 256 samples. calculated auto by iF function later
int LFR=100, RFR=125; // ideally use integer that is multiple of barwidth variable.
int clearh=130, clearv=TFTmaxY; // calculate clearh by (SAMPlES/2*barwidth)+barwidth, use 130 for samples =128
int mode=3, operation=0; //variables for moving marking lines
int bucketsize; // bucketsize is 125Hz for 128 samples, and 62.25Hz(round to 63 for integer) for 256 samples.
int crackcount,counttime,delaytime,delaydelta,delaymax,delaymin;
bool newscan,isthis1stcount,isthis2ndcount,isthis3rdcount;
// button interrupts
volatile int Button0Pressed,Button1Pressed,Button2Pressed;

void setup() {
  Serial.begin(9600); // faster if you want to see update in real time on the serial plotter, otherwise not necessary
  bucketsize=SAMPLING_FREQUENCY/SAMPLES;
  delaytime=3000;delaydelta=1000;
  delaymax=30000;delaymin=1000;
  newscan=1;
  if (SAMPLES==128){
    barwidth=2;
  }
  else {
    barwidth=1;
  }
  ST7789DisplaySetup();
  PrintSettings();
  crackcount=0;
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));

  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
  pixel.begin();

  // standard Feather front push button setups
  pinMode(0, INPUT_PULLUP);pinMode(1, INPUT_PULLDOWN);pinMode(2, INPUT_PULLDOWN); 
  attachInterrupt(digitalPinToInterrupt(0), DetectButton0, FALLING);
  attachInterrupt(digitalPinToInterrupt(1), DetectButton1, RISING);
  attachInterrupt(digitalPinToInterrupt(2), DetectButton2, RISING);


  display.fillRect(clearh,0,44,16,ST77XX_BLACK);
  display.setCursor(clearh+1,0);display.setTextSize(2);
  display.setTextColor(ST77XX_GREEN);
  display.print("CT");

  display.fillRect(clearh,18,44,16,ST77XX_BLACK);
  display.setCursor(clearh+1,18);display.setTextColor(ST77XX_WHITE);display.setTextSize(2);
  display.print(" ");

  PrintVLINELFREQMarker(LFR);
  PrintVLINERFREQMarker(RFR);
  PrintHLineCrackMarker(CRACK_THRESHOLD);
} //end of setup

void loop() {
  unsigned long microseconds,recordmillis1,recordmillis2, recordmillis3;
  
  for (int i = 0; i < SAMPLES; i++) {
    microseconds = micros();
    vReal[i] = analogRead(A0); // Read your Adafruit Mic
    if (vReal[i] >= 4095 || vReal[i] <= 0) { 
      pulseLEDBLUE();// if it pulse Blue it means turn CCW for decrease gain, CW increase gain
    }// Pulse your NeoPixel Blue to indicate "Gain Too High / Clipping"
    vImag[i] = 0;
    // Maintain precise sampling timing
    while (micros() < (microseconds + sampling_period_us)) { /* wait */ }
  }

  //FFT Processing Phase
  FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HANN, FFT_FORWARD);
  FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);
  //Serial.print("YMIN:");
  //Serial.print(Y_MIN);
  //Serial.print(",");
  //Serial.print("YMAX:");
  //Serial.print(Y_MAX);
  //Serial.print(" "); 
  


 
  for (int i = 2; i <=(SAMPLES / 2); i++) {
    float freq = (i * 1.0 * SAMPLING_FREQUENCY) / SAMPLES;
    
    PrintvRealOnTFT(i,vReal[i]); // vReal[i] now content the magnitude of each freq buckets, you have 64 buckets if samples=128, you have 128 buckets if samples = 256
    
    // If frequency is in the "First Crack" range && volume is above ambient noise level print them
    if (freq >= locrackf && freq <= hicrackf && vReal[i]>500) {
    // IF only interested in seeing the plot in the crackf range and only when vREAL is above threshold then enable the next 3 lines, 
        //Serial.print(freq,0);
        //Serial.print("Hz:");
        //Serial.print(vReal[i]);
       // PrintvRealOnTFT(i,vReal[i]);// only display if it is in the selected freq window
      //If noise level is aboce crack threshold, pulse the neopixel,
      // a future advance feature is to count how many cracks happened within a few seconds
      //start counting, and if first 1, then print, and send 1 to the PrintDetected to determine row to print
      //then increment count by 1, if detected again, send 2 to the PrintDetected (along with the vREAL[i])
      // the counter keep counting until 6, and reset back to 1.
      if (vReal[i] > CRACK_THRESHOLD) {  
        if (crackcount<=4){
          crackcount++; // increase crack count for printing row offset purposes only
        }
        else {
          crackcount=0; // reset crack count, max count 6, but variable start from 0,1,2,3,4,5... using this so it can be used to offset print location
        }
        if (isthis1stcount==0 && newscan==1){ // if isthis1stcrack==0 then it means this is the 1st time FC is heard
          recordmillis1=millis(); // then remembered time
          isthis1stcount=1; counttime++;newscan=0; // set to 1 and so next loop it does not remember a new time
        }
        else if(isthis1stcount==1 && isthis2ndcount==0 && newscan==1) {
          recordmillis2=millis();
          isthis2ndcount=1;newscan=0;counttime++;
        }
        else if (isthis2ndcount==1 && isthis3rdcount==0 && newscan==1){
          recordmillis3=millis();
          isthis3rdcount=1;counttime++;newscan=0;
        }
        PrintDetectedFeq(i,crackcount,counttime); // then print the freq
        pulseLEDGREEN(); // cract detected so pulse the neopixel
      }
      //PrintTime(recordmillis);
    }//end of if loop for checking if freq is in the bandwidth of interest
    //if (i < (SAMPLES / 2)) {
    //  Serial.print(" ");
    //}
  }
   
  newscan=1; // variable so that only marking count once every entire freq scan

  if(isthis1stcount==1 && isthis3rdcount==1 && (recordmillis3-recordmillis1)<=delaytime) {
    if (counttime<=3){
      pulseLEDRED();
      counttime=0;
    }
  }
  else if(isthis1stcount==1 && isthis3rdcount==1 && (recordmillis3-recordmillis1)>delaytime) { // if it the time elapsed is longer than delaytime ms then reset the remembered time, counter and also reset to make the crack sound the 1st crack again
    recordmillis1=0;isthis1stcount=0;isthis2ndcount=0;isthis3rdcount=0;recordmillis3=0;counttime=0;
  }

  //Serial.println(" End");
  //display.fillRect(ST77XX_BLACK);
  PrintFreqMagThresholdLine(CRACK_THRESHOLD);
  PrintLeftFreqRange(LFR);
  PrintRightFreqRange(RFR);
  display.fillRect(0,0,clearh,clearv,ST77XX_BLACK); //clear graph area only

  //defining modes for moving vertical and horizontal lines
  // CT_mode +,-
  // LFR_mode +,-
  // RFR_mode +,-
  // use D0 to change mode h->vl->vr
  // use D1 to change mode from +, -
  // use D2 to execute
  if (Button0Pressed) {
    display.fillRect(clearh,0,44,16,ST77XX_BLACK);
    display.setCursor(clearh+1,0);display.setTextSize(2);
    if (mode==0){
      mode=1;
      display.setTextColor(ST77XX_YELLOW);
      display.print("LFR");
    }  
    else if (mode==1){
      mode=2;
      display.setTextColor(ST77XX_ORANGE);
      display.print("RFR");
    }
    else if (mode==2){
      mode=3;
      display.setTextColor(ST77XX_GREEN);
      display.print("CT");
    }
    else if (mode==3){
      mode=0;
      display.setTextColor(ST77XX_GREEN);
      display.print("TD");
    }
    Button0Pressed=0;
  }    
  
  if (Button1Pressed) {
    display.fillRect(clearh,18,44,16,ST77XX_BLACK);
    display.setCursor(clearh+1,18);display.setTextColor(ST77XX_WHITE);display.setTextSize(2);
    if (operation==0){
      operation=1;display.print("+");
    }
    else if (operation==1){
      operation=2;display.print("-");
    }
    else if (operation==2){
      operation=0;display.print(" ");
    }
    Button1Pressed=0;
  }
  
  if (Button2Pressed) {
      if (mode==3 && operation==1 && (CRACK_THRESHOLD+STEPCHANGECRACK)<=MAXCRACK_THRESHOLD) {
          CRACK_THRESHOLD=CRACK_THRESHOLD+STEPCHANGECRACK; PrintHLineCrackMarker(CRACK_THRESHOLD);
      }
      else if (mode==3 && operation==2 && (CRACK_THRESHOLD-STEPCHANGECRACK)>=0){
          CRACK_THRESHOLD=CRACK_THRESHOLD-STEPCHANGECRACK; PrintHLineCrackMarker(CRACK_THRESHOLD);
      }
      else if (mode==1 && operation==1 && (LFR+barwidth)<=clearh) {
          LFR = LFR+barwidth;      PrintVLINELFREQMarker(LFR); locrackf=LFR*bucketsize;
      }
      else if (mode==1 && operation==2 && (LFR-barwidth)>=0) {
          LFR = LFR-barwidth;      PrintVLINELFREQMarker(LFR); locrackf=LFR*bucketsize;
      }
      else if (mode==2 && operation==1 && (RFR+barwidth)<=clearh) {
          RFR = RFR+barwidth;      PrintVLINERFREQMarker(RFR); hicrackf=RFR*bucketsize;
      }
      else if (mode==2 && operation==2 && (RFR-barwidth)>=0) {
          RFR = RFR-barwidth;      PrintVLINERFREQMarker(RFR); hicrackf=RFR*bucketsize;
      }
      else if (mode==0 && operation==1 && (delaytime+delaydelta)<=delaymax) {
          delaytime = delaytime+delaydelta;     PrintSettings();
      }
      else if (mode==0 && operation==2 && (delaytime-delaydelta)>=delaymin) {
          delaytime = delaytime-delaydelta;      PrintSettings();
      }

    Button2Pressed=0;
  }
} // end of loop

void  PrintVLINELFREQMarker(int LFR) {
  int lf;
  display.setTextSize(2);
  display.fillRect(clearh,34,44,16,ST77XX_BLACK);
  display.setCursor(clearh+1,34);display.setTextColor(ST77XX_YELLOW);
  lf=LFR/barwidth*bucketsize;
  display.print(lf);
  return;
}
void  PrintVLINERFREQMarker(int RFR) {
  int rf;
  display.setTextSize(2);
  display.fillRect(clearh,50,44,16,ST77XX_BLACK);
  display.setCursor(clearh+1,50);display.setTextColor(ST77XX_ORANGE);
  rf=RFR/barwidth*bucketsize;
  display.print(rf);
  return;
}

void PrintHLineCrackMarker(int CRACK_THRESHOLD){
  int ct;
  display.setTextSize(2);
  display.fillRect(clearh,66,50,16,ST77XX_BLACK);
  display.setCursor(clearh+1,66);display.setTextColor(ST77XX_GREEN);
  ct=CRACK_THRESHOLD;
  display.print(ct);
  return;
}

void PrintTime(unsigned long time) {
  display.setTextSize(2);
  display.fillRect(clearh+51,66,47,16,ST77XX_BLACK);
  display.setCursor(clearh+51,66);
  display.print(time/1000);
  return;
}

void PrintDetectedFeq (int i,int crackcount, int counttime) {
  int freq, offset;
  freq=bucketsize*i;
  offset=crackcount*16;
  display.setTextSize(2);
  display.setTextColor(ST77XX_GREEN);
  display.fillRect(clearh+69,66,32,16,ST77XX_BLACK);
  display.setCursor(clearh+69,66);
  display.print(counttime);
  display.setTextColor(ST77XX_RED);
  if (crackcount<3){
    offset=crackcount*16;
    display.fillRect(clearh,82+offset,47,16,ST77XX_BLACK);
    display.setCursor(clearh+1,82+offset);
    display.print(freq);
  }
  else {
    offset=(crackcount-3)*16;
    display.fillRect(clearh+51,82+offset,46,16,ST77XX_BLACK);
    display.setCursor(clearh+51,82+offset);
    display.print(freq);
  }
}

void pulseLEDGREEN(){ // indication of one crack detected
  int count=1;
  while(count>=0){
    pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // green
    pixel.show(); 
    delay(10);
    pixel.setPixelColor(0, pixel.Color(0, 0, 0)); // Off
    pixel.show();
    delay(10);
    count--;
  }
  return;
}

void pulseLEDRED(){ // indication of three crack detected in X seconds
  int count=1;
  while(count>=0){
    pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // green
    pixel.show(); 
    delay(500);
    pixel.setPixelColor(0, pixel.Color(0, 0, 0)); // Off
    pixel.show();
    delay(10);
    count--;
  }
  return;
}


void pulseLEDBLUE(){ // indication of analog signal is too high, reduce gain
  int count=1;
  while(count>=0){
    pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // blue
    pixel.show(); 
    delay(10);
    pixel.setPixelColor(0, pixel.Color(0, 0, 0)); // Off
    pixel.show();
    delay(10);
    count--;
  }
  return;
}

void ST7789DisplaySetup() {
    //setting up TFT
  const char* fileName = strrchr("/" __FILE__, '/') + 1;
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // initialize TFT
  display.init(TFTmaxY, TFTmaxX); // Init ST7789 240x135
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);
  display.setTextWrap(true);
  display.setCursor(0, 0);display.setTextColor(ST77XX_WHITE);display.setTextSize(1);

  //Serial.println(F("ST7789 TFT Initialized"));
  display.println(F("ST7789 TFT Initialized"));
  display.println(F(fileName));
  delay(1000); 
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

//PrintvRealOnTFT(i,vReal[i]);
void PrintvRealOnTFT(int i, double vReal) {  
  int x,y,barheight;
  x= barwidth*(i-1);
  y=TFTmaxY-int(vReal/maxvReal*(TFTmaxY));
  barheight=int(vReal/maxvReal*(TFTmaxY));
  if (vReal<=CRACK_THRESHOLD){
    display.fillRect(x,y,barwidth,barheight,ST77XX_BLUE); 
    //display.drawFastVLine(x,y,barheight,ST77XX_BLUE);
  }
  else {
    display.fillRect(x,y,barwidth,barheight,ST77XX_RED);
    //display.drawFastVLine(x,y,barheight,ST77XX_RED); 
  }
  return;
}

void PrintFreqMagThresholdLine(int ct) {
  int y1 = TFTmaxY-( (uint32_t)ct * TFTmaxY ) / MAXCRACK_THRESHOLD;
  display.drawFastHLine(0,y1,clearh,ST77XX_GREEN);
  return;
}

void PrintLeftFreqRange(int lf) { 
  display.drawFastVLine(lf,0,TFTmaxY,ST77XX_YELLOW);
  return;
}

void PrintRightFreqRange(int rf) {
  display.drawFastVLine(rf,0,TFTmaxY,ST77XX_ORANGE);
  return;
}

void PrintSettings(){
  
  display.fillRect(clearh+44,0,68,16,ST77XX_BLACK);display.setTextSize(2);
  display.setCursor(clearh+46,0);
  display.print(SAMPLING_FREQUENCY);
  display.setCursor(clearh+69,18);
  display.print(SAMPLES);
  display.setCursor(clearh+69,34);
  display.print(bucketsize);
  display.fillRect(clearh+69,50,68,16,ST77XX_BLACK);
  display.setTextColor(ST77XX_CYAN);
  display.setCursor(clearh+69,50);
  display.print(delaytime/1000);display.print("s");display.setTextColor(ST77XX_WHITE);
  return;
}


