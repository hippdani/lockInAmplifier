#include "globals.h"
//This code is based on The AFM code Version 8.1
//It uses the same PC interface structure and DDS as well as DAC may be valuable output options for Ref Out and X/ Y Out at some point.
//TODO: check why USB COM port open introduces jitter in fast loop. Or find workaround (Seerial 1 and UART to USB converter maybe?)
//DONE: get rid off getCycleCount64() in fast loop, takes 60 cycles to complete! 3times = 1.5 us! Instead use 24bit hw timer of Cortex-M. increment something to get at least to 32bit length = 40sec and swap back core1 / core0.
//TODO: Use volatile on cross core variables.
//TODO: force phi12bit calculation into 64bit mode (dividing brings the value below 16bit in the end, but it goes up to 44bit in between). IS modulo 64bit & fast?
//TODO: does rp2040 support SIMD on Cortex-M?
//TODO: increase sin / cos 12 bit accuracy according to python test skript and comments
//TODO:  which means: check if picoMicros does overflow, check used librarys for compatibility, check if per pin power OUTPUT is sufficient.
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
  //loadStdPID();//Load PID parameters & Resonace freq from EEPROM / flash
}

void loop() 
{
  static uint64_t timingStart;
  static uint64_t timingDif;
  static uint32_t message;
  const static int N = 1000; //length of the X and Y rundspeicher arrays
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
  static long LOCK_INT_REFFreq;
  static long linescanMin;
  static long linescanMax;
  static long scanStep;
  static long scanTimestep;
  static long lastStep;
  static unsigned long long timeNow;
  static unsigned long long timeOld;
  static char answ;

  int32_t msg;
  uint32_t uMsg;
  int32_t sMsg;

  
  switch (state)
  {
    case WAITforINPUT: //Waiting for a Serial message and checking if it is a valid command
      {
        if(Serial.available())
        {
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
      intRefFreq = inputInt("Enter reference frequency in Hz(max 10k): ");;
      samplingFreq = inputInt("Enter Sampling Rate in Hz(max 100k): ");
      filterOrder = inputInt("Enter order of filter (1-8): ");
      TauLowpass = inputInt("Enter timeconstant of lowpass in ms (1-1000): ");
      downsamplingFactorInput = inputInt("Enter downsampling factor (1-255): ");
      
      intRefFreq = limitInt(intRefFreq, 1, 10000);
      samplingFreq = limitInt(samplingFreq, 0, 100000);
      filterOrder = limitInt(filterOrder, 0, 8);
      TauLowpass = limitInt(TauLowpass, 1, 1000);
      Serial.println();
      serialTrigger("Press enter to start");
      //calculate filter coefficients and timing parameters
      rp2040.idleOtherCore();
      setIntPeriodCl(clock_get_hz(clk_sys) / intRefFreq);
      setSamplingDelayCl1(clock_get_hz(clk_sys) / samplingFreq);
      a[0] = TauLowpass;//dummycoefficients except a[0]
      setDownsamplingFactor(limitInt(downsamplingFactorInput,1,255));
      resetFastCore();
      //send setup information to core 1 and let it do the sampling & downsampling
      //clear FIFO and sync both cores?
      while(rp2040.fifo.pop_nb(NULL)) {} //empty FIFO into NULL pointer
      NcounterX = 0;
      NcounterY = 0;
      arrayReset(Xarray, N);
      arrayReset(Yarray, N);
      rp2040.resumeOtherCore();
      fastCoreActive = 1;
      state = LOCK_INT_REF;
      break;
    case LOCK_INT_REF:
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
            Serial.println(); Serial.print(lowpass(Xarray, N, NcounterY)); Serial.print(", "); Serial.print(lowpass(Yarray, N, NcounterY));  //reduce to one counter (use only Y because i is incremented after a complete sample
            NcounterY++;      //Serial.print(lowpass(Xarray[NcounterY]); Serial.print(", "); Serial.print(Yarray[NcounterY]); //
            if(NcounterY >= N) NcounterY = 0;
          }
        }
        //abort statement:
        if(Serial.read() == 'e')
        {
          Serial.print("\n Lock In measurement with internal reference aborted!");
          clear_Serial_buff();
          state = WAITforINPUT;
          fastCoreActive = 0;
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
      
    case ENTER_MAN_APPR:
      Serial.println();
      Serial.println("Manual approach mode. QTF will be excited at a fixed frequency & Z-PID will be activated");
      print_std_parameters();
      Serial.print("Continue with existing PID & frequency settings? (y/n)");
      while (!Serial.available());
      answ = Serial.read();
      Serial.print(" -> "); Serial.println(answ);
      clear_Serial_buff();
      while (Serial.available()) Serial.read();
      if(answ == 'n' || answ == 'N')
      {
        Serial.println("Parameters will be stored for all applications");

        stdFreq = inputInt("Enter QTF frequency in Hz: ");
        stdKP = inputFloat("Enter KP (float): ");
        stdKI = inputFloat("Enter KI (float): ");
        stdKD = inputFloat("Enter KD (float): ");
        stdSetpoint = inputInt("Enter Setpoint: ");
        saveStdPID();
        
        serialTrigger("Press enter to start");
        dds_set_freq(stdFreq);
        delay(100);
        pid(stdKP, stdKI, stdKP, stdSetpoint, 1, 1);
        timeOld = picoMicros();
        state = MAN_APPR;
      }
      else if(answ == 'y' || answ == 'Y')
      {
        serialTrigger("Press enter to start");
        dds_set_freq(stdFreq);
        delay(100);
        pid(stdKP, stdKI, stdKD, stdSetpoint, 1, 1);
        timeOld = picoMicros();
        state = MAN_APPR;
      }
      else if(true)
      {
        Serial.println("Invalid input, manual approach aborted.");
        state = WAITforINPUT;
      }
      break;

    case CHANGE_PID:
      Serial.println();
      Serial.println("PID-Parameters and QTF excitation frequency:");
      print_std_parameters();
      Serial.println("Enter new Parameters");
      Serial.println("Parameters will be stored for all applications");

      stdFreq = inputInt("Enter QTF frequency in Hz: ");
      stdKP = inputFloat("Enter KP (float): ");
      stdKI = inputFloat("Enter KI (float): ");
      stdKD = inputFloat("Enter KD (float): ");
      stdSetpoint = inputInt("Enter Setpoint: ");
      saveStdPID();
      state = WAITforINPUT;
      break;
 
      
    case HELP:
      help_message();
      state = WAITforINPUT;
      break;
      
    default:
      Serial.println("Command not yet implemented or state variable off limit");
      state = WAITforINPUT;
      break;
      
  }
  

}
