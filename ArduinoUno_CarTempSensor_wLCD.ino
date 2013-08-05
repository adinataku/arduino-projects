
// include the library code:
// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define WHITE 0x7


//TMP36 Pin Variables

int sensorPin = 0; //the analog pin the TMP36's Vout (sense) pin is connected to
                        //the resolution is 10 mV / degree centigrade with a
                        //500 mV offset to allow for negative temperatures

float presetF = 70.0; // later this variable will be user changeable.
float tolerance = 2.0;
float max_presetF = 80.0;
float min_presetF = 65.0;
float max_tolerance = 5.0;
float min_tolerance = 1.0;

// define external reference voltage
#define aref_voltage 5000.0
 
/*
 * setup() - this function runs once when you turn your Arduino on
 * We initialize the serial connection with the computer
 */
void setup()
{
  Serial.begin(9600);  //Start the serial connection with the computer
                       //to view the result open the serial monitor 
  
  // if using an external analog reference enable the next line then use the ref voltage in the formula below
  //analogReference(EXTERNAL);
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  lcd.setBacklight(WHITE);
}

uint8_t i=0; 
void loop()                     // run over and over again
{

  
  uint8_t buttons = lcd.readButtons();
  
  //navigation buttons setup
  if (buttons) 
  {
    lcd.clear();
    if (buttons & BUTTON_UP) {      
      presetF=checkrange(presetF+0.5,max_presetF,min_presetF);
      lcd.setCursor(13, 0);
      lcd.print(presetF);
    }
    if (buttons & BUTTON_DOWN) {
      presetF=checkrange(presetF-0.5,max_presetF,min_presetF);
      lcd.setCursor(13, 0);
      lcd.print(presetF);
    }
    if (buttons & BUTTON_LEFT) {
      tolerance=checkrange(tolerance-0.5,max_tolerance,min_tolerance);
      lcd.setCursor(9, 1);
      lcd.print("Tol ");lcd.print(tolerance);
    }
    if (buttons & BUTTON_RIGHT) {
      tolerance=checkrange(tolerance+0.5,max_tolerance,min_tolerance);
      lcd.setCursor(9, 1);
      lcd.print("Tol ");lcd.print(tolerance);
    }
    if (buttons & BUTTON_SELECT) {
      //
    }
  }
  
  //getting the voltage reading from the temperature sensor
 int reading = analogRead(sensorPin);  
 
 // print out the analogreading
// Serial.print(reading);Serial.println(" analogread");
 
 // converting that reading to voltage, for 3.3v arduino due use 3300.0
  // converting that reading to voltage, for 5v arduino uno use 5000.0
  // another way is to use aref_voltage define in the beginning of the program
 float voltage = reading * aref_voltage;
 voltage /= 1024.0; 
 
 // print out the voltage
 //Serial.print(voltage); Serial.println(" milivolts");
 
 // now print out the temperature
 float temperatureC = (voltage - 500.0) / 10.0 ;  //converting from 10 mv per degree wit 500 mV offset
                                               //to degrees ((volatge - 500mV) times 100)
 //Serial.print(temperatureC); Serial.println(" degrees C");
 
 // now convert to Fahrenheight
 float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
 Serial.print(temperatureF); Serial.println(" degrees F");
 
  // set the cursor to column 0, line 0
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 0);
  lcd.print(temperatureF); lcd.print("F. Set ");lcd.print(presetF);
  lcd.setCursor(0, 1);
 if (temperatureF > (presetF+tolerance) )
 {
   Serial.println("turn on AC");
  
  lcd.print("AC on.");
  
  lcd.setCursor(9, 1);
  lcd.print("Tol ");lcd.print(tolerance);
 }
 else
 {
   if (temperatureF<= (presetF-tolerance))
   {
     Serial.println("turn on Heat");
     
     lcd.print("Heat on.");
     lcd.setCursor(9, 1);
      lcd.print("Tol ");lcd.print(tolerance);
   }
   else
   {
     Serial.println("Ambient temperature within tolerance");
     
     lcd.print("Perfect.");
     lcd.setCursor(9, 1);
     lcd.print("Tol ");lcd.print(tolerance);
   }
 }
 
 delay(500);                                     //waiting 5 seconds before loop repeat
}

// function to check values and make sure only proper value within ranges is sent back
float checkrange(float value,float maxval, float minval) {
  if (value>maxval)
  {
    return maxval;
  }
  if (value<minval)
  {
    return minval;
  }
  if (value<=maxval && value>=minval)
  {
    return value;
  }
}
