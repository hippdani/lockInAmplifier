#include "globals.h"
//This code is based on The AFM code Version 8.1
//It uses the same PC interface structure and DDS as well as DAC may be valuable output options for Ref Out and X/ Y Out at some point.
//TODO: Test overvoltage protction of LM393 and apply shcmitt trigger circuit
//DONE: make sure sin does not point out of the array when using ext reference and the frequency is dropping. -> solution now is sin continous at set frequency infdefinitely until next ref signal arrives
//TEST: Lowpass filter, print fillFIResponse and watch out for Tail 
//TEST: Übersprechen von TRIG in SIG verhindern: virtual GND stabilisieren mit >10 uF oder LM393 nutzen
//TEST: Write int64_t inputLimitInt(String, min, max) in order to see if a limit has been taken imediately.
//TODO: Time modulo and search for replacement if its slow
//TEST: merge INT_REF into stdParameter interface
//TODO: enable ADC output, (reenable REF out?, maybe via DDS chip?)
//TODO: implement phase shift as input parameter and in calculation
//TEST: implement fast core state 2 with ext reference.
//TODO: check why USB COM port open introduces jitter in fast loop. Or find workaround (Serial 1 and UART to USB converter maybe?)
//DONE: get rid off getCycleCount64() in fast loop, takes 60 cycles to complete! 3times = 1.5 us! Instead use 24bit hw timer of Cortex-M. increment something to get at least to 32bit length = 40sec and swap back core1 / core0.
//TODO: Use volatile on cross core variables. / use no cross core vaiables
//TODO: force phi12bit calculation into 64bit mode (dividing brings the value below 16bit in the end, but it goes up to 44bit in between). IS modulo 64bit & fast?
//TODO: does rp2040 support SIMD on Cortex-M?
//TODO: increase sin / cos 12 bit accuracy according to python test skript and comments
//Done: find 4MHz generator, check all chips for 3V3 compatibility, deactivate noisy Low Power mode, make it RP2040 fit, static longs do not work! (behave like final) -> Solution: Do not use inside switch statement, 
//Done: check SPI protocoll -> only difference: Set pins MOSI MISO CLK according to datasheet before SPI.begin, ADC resolution setttings?!?
void setup() 
{
  delay(1000);
  Serial.begin(2000000);
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH); //Set the SMPS to PWM mode to reduce ripple on the 3V3 Rail
  analogReadResolution(12);

  //dds_init();
  //dac_init();;
  //dds_set_freq(0); //if the Communication between controller and DDS has been interrupted and no Power On Reset has occured yet, two messages should restore communication and set the output to constant = 0Hz
  //dds_set_freq(0);

  help_message();

  Serial.setTimeout(SERIAL_TO); //Timeout for parsing the incoming stream for numbers (and other things, but these are not relevant).
  loadStdParam();//Load filter param and sampling freq etc. from EEPROM / flash
}

void loop() 
{
  static uint16_t postDScounter=0;
  static stateType returnState = WAITforINPUT;
  static uint64_t timingStart;
  static uint64_t timingDif;
  static uint32_t message;
  static int32_t Xarray[N];
  static int32_t Yarray[N];
  static uint32_t NcounterX;
  static uint32_t NcounterY;
  static long fixedFreq;
  static long intRefFreq;
  static long samplingFreq;
  static int filterOrder;
  static int TauLowpass;
  static int downsamplingFactorInput;
  static unsigned long long timeNow;
  static unsigned long long timeOld;
//variables for TEST
  int32_t msg;
  uint32_t uMsg;
  int32_t sMsg;

  
  switch (state)
  {
    case WAITforINPUT: //Waiting for a Serial message and checking if it is a valid command
      {
        if(Serial.available())
        {
          returnState = WAITforINPUT;  //make sure that CHANGE_PID returns to WAITforINPUT if called directly
          if(!setState(Serial.read()))  //if there was an valid character sent through Serial, the state is changed accordingly. Otherwise it stays unchanged.
          {
            Serial.println("Not a valid command. Try 'h' to get some help");
          }
          else clear_Serial_buff();
        }
      }
      break;
      
    case ENTER_INT_REF: 
      Serial.println(); Serial.println("Lock In Mode with internal reference frequency");
      intRefFreq = inputInt("Enter reference frequency in Hz(max 10k): ");
      intRefFreq = limitInt(intRefFreq, 1, 10000);
      changeStdParam(); //no return stae set, because it goes on in this state anyway
//      samplingFreq = inputInt("Enter Sampling Rate in Hz(max 100k): ");
//      filterOrder = inputInt("Enter order of filter (1-8): ");
//      TauLowpass = inputInt("Enter timeconstant of lowpass in ms (1-1000): ");
//      downsamplingFactorInput = inputInt("Enter downsampling factor (1-255): ");
      //calculate filter coefficients and timing parameters
      fillFIResponse(); //uses stdTauLowpass and N
      rp2040.idleOtherCore();
      setIntPeriodCl(clock_get_hz(clk_sys) / intRefFreq);
      setSamplingDelayCl1(clock_get_hz(clk_sys) / stdSamplingFreq);
      a[0] = stdTauLowpass;//dummycoefficients except a[0]
      setDownsamplingFactor(stdPreDS);
      resetFastCore();
      //send setup information to core 1 and let it do the sampling & downsampling
      //clear FIFO and sync both cores?
      while(rp2040.fifo.pop_nb(NULL)) {} //empty FIFO into NULL pointer
      NcounterX = 0;
      NcounterY = 0;
      arrayReset(Xarray, N);
      arrayReset(Yarray, N);
      postDScounter = 0;
      rp2040.resumeOtherCore();
      fastCoreState = 1;
      state = LOCK_IN;
      break;
    case LOCK_IN:
      while(true)
      {
        //pull downsampled data from core1 
        //when available, read and safe to rundspeicher, advance index, calculate FIR / IIR (function), print X&Y, idle
        if(rp2040.fifo.pop_nb(&message))
        {
          if((message & 1) == 0) 
          {
            Xarray[NcounterX] = int32_t(message); 
            NcounterX++;
            if(NcounterX >= N) NcounterX = 0;
          }
          else
          {
            Yarray[NcounterY] = int32_t(message-1); //-1 to Remove LSB that identifies X / Y, reform from uint to int.
            postDScounter++;//Serial.println(); Serial.print(lowpass(Xarray, N, NcounterY)); Serial.print(", "); Serial.print(lowpass(Yarray, N, NcounterY));  //reduce to one counter (use only Y because i is incremented after a complete sample
            NcounterY++;      //Serial.print(lowpass(Xarray[NcounterY]); Serial.print(", "); Serial.print(Yarray[NcounterY]); //
            if(NcounterY >= N) NcounterY = 0;
          }
          if(postDScounter >= stdPostDS)
          {
            if(stdFilterOd == 1)
            {
              Serial.println(); Serial.print(lowpassFIR(Xarray, NcounterY)); Serial.print(", "); Serial.print(lowpassFIR(Yarray, NcounterY));
            }
            else //order 0, moving average
            {
              Serial.println(); Serial.print(lowpass(Xarray, N, NcounterY)); Serial.print(", "); Serial.print(lowpass(Yarray, N, NcounterY));
            }
            postDScounter = 0;             
          }
        }
        //abort statement:
        if(Serial.read() == 'e')
        {
          Serial.print("\n Lock In measurement with internal reference aborted!");
          clear_Serial_buff();
          state = WAITforINPUT;
          fastCoreState = 0;
          break;
        }
      }
      state = WAITforINPUT;
      break;
    case ENTER_TEST:
      Serial.println("The test skript will be run...");
      state = TEST;
      break;

    case TEST:
      /*
      for(int i = 0; i < 4097; i++)
      {
        Serial.println();
        Serial.print(i); Serial.print(", "); Serial.print(sin12bit(i)); Serial.print(", "); Serial.print(cos12bit(i));
      }
      */
      /*timingStart = rp2040.getCycleCount64();
      for(int i = 0; i < 100; i++)
      {
        
      }
      timingDif = rp2040.getCycleCount64() - timingStart;
      Serial.print("100 repetitions took "); Serial.print(timingDif); Serial.print("clock cycles. One routine takes approx ");
      Serial.print(timingDif/13300.0); Serial.println(" µs (microseconds)");*/
      msg = inputInt("number to cast: ");
      uMsg = msg;
      sMsg = uMsg;
      Serial.println(sMsg);
      state = WAITforINPUT;
      break;
      
    case ENTER_EXT_REF_1:
      Serial.println();
      Serial.println("Lock-In with external TTL reference");
      changeStdParam(ENTER_EXT_REF_2);
      break;

    case ENTER_EXT_REF_2:
      /*serialTrigger("Press enter to continue");
      dds_set_freq(stdSamplingFreq);
      delay(100);
      pid(stdPreDS, stdPostDS, stdFilterOd, stdTauLowpass, 1, 1);
      timeOld = picoMicros();
      */
      rp2040.idleOtherCore();
      //setIntPeriodCl(clock_get_hz(clk_sys) / intRefFreq); //only for INT_REF
      setSamplingDelayCl1(clock_get_hz(clk_sys) / stdSamplingFreq);
      a[0] = stdTauLowpass;//dummycoefficients except a[0]
      setDownsamplingFactor(stdPreDS);
      resetFastCore();
      //send setup information to core 1 and let it do the sampling & downsampling
      //clear FIFO and sync both cores?
      while(rp2040.fifo.pop_nb(NULL)) {} //empty FIFO into NULL pointer
      NcounterX = 0;
      NcounterY = 0;
      arrayReset(Xarray, N);
      arrayReset(Yarray, N);
      rp2040.resumeOtherCore();
      fastCoreState = 2;
      state = LOCK_IN;
      break;

    case CHANGE_STD:
      changeStdParam();
      break;
 
      
    case HELP:
      help_message();
      state = WAITforINPUT;
      break;
    case BIG_HELP:
      Serial.println("Enter a command to get a detailed description");
      while(!Serial.available()) {} //Waits until there is a input
      
      if(!setState(Serial.read()))  //if there was an valid character sent through Serial, the state is changed accordingly. Otherwise it stays unchanged.
      {
        Serial.println("Not a valid command. Try 'h' to get some help");
        //implement something that makes this return to state = WAITforINPUT immediately
      }
      clear_Serial_buff();
      if(state == ENTER_INT_REF) 
      {
        Serial.println("This function starts a Lock In Measurement, where the demodulation is done with a sinusoidal a cosine signal generated at a set frequency.");
        Serial.println("The pre-filter downsampling is necessary at high sampling frequencies to give core0 time to compute the lowpass filter. A factor N sums up N samples before sending it to core 0. N = 1 is no downsampling");
        Serial.println("The post-filter downsampling reduces the amount of data sent to the PC. A factor N drops N-1 samples before sending it to the PC. N = 1 is no downsampling");  
      }
      else if(state == ENTER_EXT_REF_1) 
      {
        Serial.println("This function starts a Lock In Measurement, where the demodulation is done with a sinusoidal a cosine signal generated at the frequency present at GPIO pin REF_IN. Preferrably use a TTL Signal. The HIGH threshhold is ~0.8V. On the rising edge, the sinus (Y) will be set to 0 and the cos (x) will be 1");
        Serial.println("The pre-filter downsampling is necessary at high sampling frequencies to give core0 time to compute the lowpass filter. A factor N sums up N samples before sending it to core 0. N = 1 is no downsampling");  
        Serial.println("The post-filter downsampling reduces the amount of data sent to the PC. A factor N drops N-1 samples before sending it to the PC. N = 1 is no downsampling");
      }
      else if(state == CHANGE_STD) 
      {
        Serial.println("Allows to change some parameters shared between the internal and external reference mode.");
        Serial.println("These parameters will be stored to EEPROM and will therefore also be available at the next startup. However changing these parameters inside the internal or external reference mode overwrites the parameters set here in RAM and EEPROM");
      }
      else Serial.println("No help message available for this command");
      state = WAITforINPUT;
      break;
      
    default:
      Serial.println("Command not yet implemented or state variable off limit");
      state = WAITforINPUT;
      break;
      
  }
  

}
