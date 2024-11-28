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
const int N = 1000; //length of the X and Y rundspeicher arrays and therefore length off all core0 sample data structures
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
  HELP,
  BIG_HELP
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

  else if (newState == 'H') state = BIG_HELP;

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
  Serial.println("-type 'H' to show a help message to a specific function");
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
  int64_t input = Serial.parseInt(SKIP_ALL);
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

int64_t limitInt(long var, int64_t lowerLimit, int64_t upperLimit)
{
  if(var > upperLimit) var = upperLimit;
  if(var < lowerLimit) var = lowerLimit;
  return(var);
}

int64_t inputLimitInt(String inputMessage, int64_t lowerLimit, int64_t upperLimit)
{
  Serial.print(inputMessage);
  while (!Serial.available());
  int64_t input = Serial.parseInt(SKIP_ALL);
  if(input > upperLimit) input = upperLimit;
  if(input < lowerLimit) input = lowerLimit;
  Serial.println(input);
  clear_Serial_buff();
  return(input);
}

void saveStdParam()
{
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, stdSamplingFreq);
  EEPROM.put(4, stdPreDS);
  EEPROM.put(8, stdPostDS);
  EEPROM.put(12, stdFilterOd);
  EEPROM.put(16, stdTauLowpass);
  EEPROM.end();
}

void loadStdParam()
{
  //get PID values from EEPROM / Flash and store into (global) variables
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, stdSamplingFreq);
  EEPROM.get(4, stdPreDS);
  EEPROM.get(8, stdPostDS);
  EEPROM.get(12, stdFilterOd);
  EEPROM.get(16, stdTauLowpass);
  EEPROM.end();
}

void print_std_parameters()
{
  Serial.print("Sampling Freq.: "); Serial.print(stdSamplingFreq/1000.0); Serial.println(" kHz");
  Serial.print("pre-filter downsampling factor: "); Serial.println(stdPreDS); 
  Serial.print("post-filter downsampling factor: "); Serial.println(stdPostDS,5); 
  Serial.print("filter order: "); Serial.println(stdFilterOd,5); 
  Serial.print("Lowpass timeconstant in samples/pre filter downsampling OR ms?: "); Serial.println(stdTauLowpass); 
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

void changeStdParam(stateType returnState = WAITforINPUT)  //returns to returnState if successfull or WAITforINPUT if unsuccsessfull. standard returnState is also WAITforINPUT
{
  loadStdParam();
  print_std_parameters();
  Serial.print("Continue with existing filter & frequency settings? (y/n)");
  while (!Serial.available());
  char answ = Serial.read();
  Serial.print(" -> "); Serial.println(answ);
  clear_Serial_buff();
  while (Serial.available()) Serial.read();
  if(answ == 'n' || answ == 'N')
  {
    Serial.println("Parameters will be stored (not yet for all applications)");
    stdSamplingFreq = inputLimitInt("Enter sampling frequency in kHz (0-200): ",0,200)*1000;  //The value is saved in Hz! 200kHz is an arbitrary limit as of now!
    stdPreDS = inputLimitInt("Enter pre filter downsampling factor (1-255): ",1,255);
    stdPostDS = inputLimitInt("Enter post filter downsampling factor (1-255): ",1,255);
    stdFilterOd = inputLimitInt("Enter filter order (0-8): ",0,8);
    stdTauLowpass = inputLimitInt("Enter lowpass timeconstant in samples/pre filter downsampling (1-1000): ",1,N);  //limits to N, text has to be adapted by hand to N
    saveStdParam();
    state = returnState;
    serialTrigger("Press enter to continue");
  }
  else if(answ == 'y' || answ == 'Y')
  {
    state = returnState;
    serialTrigger("Press enter to continue");
  }
  else if(true)
  {
    Serial.println("Invalid input, parameter menu aborted.");
    state = WAITforINPUT;
  }
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

float makeAvg(int avgArray[],int n) //returns average (int) of first n avgArray entrys. avgArray has to have at least n elements
{
  int64_t sum=0;
  for(int j = 0; j< n; j++)
  {
    sum = sum + avgArray[j];
  }
  return(sum/(float)n);
}
