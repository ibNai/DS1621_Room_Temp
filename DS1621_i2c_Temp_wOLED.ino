#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

int y = 0;
int x = 0;

// SDA pin is Analog4
// SCL pin is Analog5
// DS1621 has A2, A1, and A0 pins connected to GND


// device ID and address
String inString = "";
String inStringF = "";
float myfloat;

#define DEV_TYPE   0x90 >> 1                    // shift required by wire.h
#define DEV_ADDR   0x00                         // DS1621 address is 0
#define SLAVE_ID   DEV_TYPE | DEV_ADDR

// DS1621 Registers & Commands

#define RD_TEMP    0xAA                         // read temperature register
#define ACCESS_TH  0xA1                         // access high temperature register
#define ACCESS_TL  0xA2                         // access low temperature register
#define ACCESS_CFG 0xAC                         // access configuration register
#define RD_CNTR    0xA8                         // read counter register
#define RD_SLOPE   0xA9                         // read slope register
#define START_CNV  0xEE                         // start temperature conversion
#define STOP_CNV   0X22                         // stop temperature conversion

// DS1621 configuration bits

#define DONE       B10000000                    // conversion is done
#define THF        B01000000                    // high temp flag
#define TLF        B00100000                    // low temp flag
#define NVB        B00010000                    // non-volatile memory is busy
#define POL        B00000010                    // output polarity (1 = high, 0 = low)
#define ONE_SHOT   B00000001                    // 1 = one conversion; 0 = continuous conversion


void setup()
{

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)

  // Clear the buffer.
  display.clearDisplay();
  
 Wire.begin();                                 // connect I2C
 startConversion(false);                       // stop if presently set to continuous
 setConfig(POL | ONE_SHOT);                    // Tout = active high; 1-shot mode
 setThresh(ACCESS_TH, 27);                     // high temp threshold = 80F
 setThresh(ACCESS_TL, 24);                     // low temp threshold = 75F

 int tHthresh = getTemp(ACCESS_TH);
 int tLthresh = getTemp(ACCESS_TL);
}


void loop()
{

  display.setTextSize(2);
  display.setTextColor(WHITE);

  display.setCursor(x, y);
  display.print("Room Temp!");
  display.display();
  //display.clearDisplay();
  delay(1);
   
 int tC, tFrac;

 tC = getHrTemp();                             // read high-resolution temperature

 if (tC < 0) {
   tC = -tC;                                   // fix for integer division
 }

 tFrac = tC % 100;                             // extract fractional part
 tC /= 100;                                    // extract whole part

 if (tFrac < 10)
   inString = String(tC);
 inString = String(tC)+"."+String(tFrac);

 x = 0;
 y = 5;
 display.setCursor(x, y);
 display.println();

 inStringF = String((inString.toFloat()*1.8)+32);

 display.print(inString + " C" + "\n" + inStringF + " F");
 display.display();
 delay(1);

 delay(2500);
 inString = "";
 inStringF = "";
 myfloat = 0.0;
 x = 0;
 y = 0;
 display.clearDisplay();
}


// Set configuration register

void setConfig(byte cfg)
{
 Wire.beginTransmission(SLAVE_ID);
 Wire.write(ACCESS_CFG);
 Wire.write(cfg);
 Wire.endTransmission();
 delay(15);                                    // allow EE write time to finish
}


// Read a DS1621 register

byte getReg(byte reg)
{
 Wire.beginTransmission(SLAVE_ID);
 Wire.write(reg);                               // set register to read
 Wire.endTransmission();
 Wire.requestFrom(SLAVE_ID, 1);
 byte regVal = Wire.read();
 return regVal;
}


// Sets temperature threshold
// -- whole degrees C only
// -- works only with ACCESS_TL and ACCESS_TH

void setThresh(byte reg, int tC)
{
 if (reg == ACCESS_TL || reg == ACCESS_TH) {
   Wire.beginTransmission(SLAVE_ID);
   Wire.write(reg);                             // select temperature reg
   Wire.write(byte(tC));                        // set threshold
   Wire.write(0);                               // clear fractional bit
   Wire.endTransmission();
   delay(15);
 }
}


// Start/Stop DS1621 temperature conversion

void startConversion(boolean start)
{
 Wire.beginTransmission(SLAVE_ID);
 if (start == true)
   Wire.write(START_CNV);
 else
   Wire.write(STOP_CNV);
 Wire.endTransmission();
}


// Reads temperature or threshold
// -- whole degrees C only
// -- works only with RD_TEMP, ACCESS_TL, and ACCESS_TH

int getTemp(byte reg)
{
 int tC;

 if (reg == RD_TEMP || reg == ACCESS_TL || reg == ACCESS_TH) {
   byte tVal = getReg(reg);
   if (tVal >= B10000000) {                    // negative?
     tC = 0xFF00 | tVal;                       // extend sign bits
   }
   else {
     tC = tVal;
   }
   return tC;                                  // return threshold
 }
 return 0;                                     // bad reg, return 0
}


// Read high resolution temperature
// -- returns temperature in 1/100ths degrees
// -- DS1620 must be in 1-shot mode

int getHrTemp()
{
 startConversion(true);                        // initiate conversion
 byte cfg = 0;
 while (cfg < DONE) {                          // let it finish
   cfg = getReg(ACCESS_CFG);
 }

 int tHR = getTemp(RD_TEMP);                   // get whole degrees reading
 byte cRem = getReg(RD_CNTR);                  // get counts remaining
 byte slope = getReg(RD_SLOPE);                // get counts per degree

 if (tHR >= 0)
   tHR = (tHR * 100 - 25) + ((slope - cRem) * 100 / slope);
 else {
   tHR = -tHR;
   tHR = (25 - tHR * 100) + ((slope - cRem) * 100 / slope);
 }
 return tHR;
}
