#include <SPI.h>
#include <EEPROM.h>

#define REF_IN 13
#define REF_OUT 15
#define SAMPLE_SYNC 14
#define MISO0 16
#define MOSI0 19
#define SPICLK0 18
#define MISO1 8
#define MOSI1 11
#define SPICLK1 10
#define DDS_FSYNC_PIN 20
#define CLOCK_REF_PIN_PICO 22
#define QTF_AMP_PIN 27 //GP27_A1
#define Z_VOLT_PIN 26 //GP26_A0 connected to 1:2.66 Voltage divider of Z piezo => Factor 3.66
#define ADC_Q_FSYNC_PIN 21
#define ADC_P_FSYNC_PIN 9 //DONT KNOW????!???

#define VERSION "0.1"
#define SIM 0
#define SERIAL_TO 100   //Timeout for parsing the incoming stream for numbers (and other things, but these are not relevant).
#define EEPROM_SIZE 4096

uint32_t stdSamplingFreq;
uint32_t stdPreDS;
uint32_t stdPostDS;
uint32_t stdFilterOd;
uint32_t stdTauLowpass;
int32_t a[8]; //IIR filter coefficients (global)
int32_t b[8];
byte fastCoreState = 0;


enum stateType 
{
  WAITforINPUT,
  ENTER_INT_REF,
  LOCK_IN,
  ENTER_EXT_REF_1,
  ENTER_EXT_REF_2,
  EXT_REF,
  ENTER_FIXFREQ,
  FIXFREQ,
  ENTER_SCAN,
  SCAN,
  ENTER_TEST,
  TEST,
  CHANGE_STD,
  ENTER_LINESCAN,
  LINESCAN,
  HELP
};

stateType state = WAITforINPUT;

bool setState(char newState) // sets the state variable according to some defined characters
{
  if (newState == 'i') state = ENTER_INT_REF;

  else if (newState == 'r') state = ENTER_EXT_REF_1;

  else if (newState == 'f') state = ENTER_FIXFREQ;
  
  else if (newState == 'c') state = ENTER_SCAN;

  else if (newState == 't') state = ENTER_TEST;

  else if (newState == 'p') state = CHANGE_STD;

  else if (newState == 'l') state = ENTER_LINESCAN;

  else if (newState == 'h') state = HELP;

  else if (newState == 'e') state = WAITforINPUT;  

  else if (true) return(false);  //returns false if the given character corresponds to no state
  return(true);                 //returns true if the state has been changed succesfully
}

void help_message()
{
  Serial.println();
  #if defined(__AVR_ATmega328P__)
    Serial.println("Platform: Arduino UNO or Clone, THIS PLATFORM IS NOT FAST ENOUGH!!!!!! Change to PiPico"); 
  #else
    Serial.println("Platform: PiPico"); 
  #endif
  Serial.print("Digital LockIn Firmware Version "); Serial.println(VERSION);
  Serial.println("This is a project effort to build a Lock In Amplifier that samples at 1MSPS started in March 2024");
  //Serial.println("-type 'a' to enter the Z piezo into manual approach mode = active pid");  //move Z piezo to top position and activate pid to avoid crashing into qtf once its approaching. Giving visual feedback over COM port
  Serial.println("-type 's' to sweep the internal reference frequency while measuring");
  Serial.println("-type 'r' to Lock in with EXTERNAL reference");
  Serial.println("-type 'i' to Lock in with INTERNAL reference");
  Serial.println("-type 'c' to show PLL frequency and jitter");
  Serial.println("-type 'l' to start a lock-in-measurement");
  Serial.println("-type 't' to start the test skript");
  Serial.println("-type 'p' to show / change filter parameters"); //Makes only sense later on when parameters are stored non-volatile
  Serial.println("-type 'h' to show this message again");
  Serial.println("-type 'e' to exit any application (well, just some at the moment)"); //TODO!!!!!
}

void clear_Serial_buff()
{
  while(Serial.available()) 
  {
    Serial.read();
  }
}

int64_t inputInt(String inputMessage)
{
  Serial.print(inputMessage);
  while (!Serial.available());
  long input = Serial.parseInt(SKIP_ALL);
  Serial.println(input);
  clear_Serial_buff();
  return(input);
}

float inputFloat(String inputMessage)
{
  Serial.print(inputMessage);
  while (!Serial.available());
  float input = Serial.parseFloat(SKIP_ALL);
  Serial.println(input,5);
  clear_Serial_buff();
  return(input);
}

long limitInt(long var, long lowerLimit, long upperLimit)
{
  if(var > upperLimit) var = upperLimit;
  if(var < lowerLimit) var = lowerLimit;
  return(var);
}

void print_std_parameters()
{
  Serial.print("Sampling Freq.: "); Serial.print(stdSamplingFreq/1000.0); Serial.println(" kHz");
  Serial.print("pre-filter downsampling factor: "); Serial.println(stdPreDS); 
  Serial.print("post-filter downsampling factor: "); Serial.println(stdPostDS,5); 
  Serial.print("filter order: "); Serial.println(stdFilterOd,5); 
  Serial.print("Lowpass timeconstant in samples/pre filter downsampling OR ms?: "); Serial.println(stdTauLowpass); 
}

unsigned long long picoMicros() //a unsigned long long version of micros(), which should have no overflow in any reasonable time
{
  return(rp2040.getCycleCount64()/(clock_get_hz(clk_sys)/1000000));  //works only with FIXED CORE FREQUENCY!
}


void arrayReset(int32_t arr[], uint32_t len)
{
  for(int i = 0; i < len; i++)
  {
    arr[i] = 0;
  }
}

/* Paul van Haastrecht
 * Original Creation date: February 2018 / version 1.0
 * serialTrigger prints a message, then waits for something
 * to come in from the serial port.
 * This  code (snippet) is released under the MIT license
 */
void serialTrigger(String message)
{
  Serial.println();
  Serial.println(message);
  Serial.println();

  while (!Serial.available());

  while (Serial.available())
    Serial.read();
}
