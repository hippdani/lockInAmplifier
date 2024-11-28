//all global variables of core1 should end in "1" to avoid changing parameters from core0 while running.

volatile uint32_t samplingDelay1; //in microseconds 
volatile uint32_t samplingDelayCl1; //in Clock cycles
volatile uint32_t intPeriodCl1; //= Clock cycles corresponding to 1 internal reference Periode. = clock_get_hz(clk_sys)/f_ref
//works only up to 32,3s period because of 32bit at 133 MHz
volatile uint32_t clockShift1; //offset of the clock in order to get clock%intPeriodCl1 to 0 at a REF_IN rising edge
volatile byte downsamplingFactor1 = 8; //defaults to 8

inline uint64_t hardwareCycleCount() 
{
  /*
   * Needs "systick_hw->csr = 0x7; systick_hw->rvr = 0x00FFFFFF;" in setup to run properly. (As found in rp2040Support.h)
   * Must be called at least every 60ms to stay accurate. If this is not done, the return value can drop by up to 2^24, but everything will increment normally from this point on.
   * Not in sync with any system Clock function. An alternative to this is implementing the timer in PIO, as is the RTOS backup timer in rp2040Support.h
   * Try to inline this function and see if it still works and if its faster -> faster, from 50cycle to 40cycles.
   */
  static uint64_t overflows = 0;
  static uint32_t lastTick;
  uint32_t thisTick;
  thisTick = 0b0001000000000000000000000000L - systick_hw->cvr;  // 0b0001000000000000000000000000 = (1<<24)
  if(thisTick < lastTick) overflows += 0b0001000000000000000000000000LL;
  lastTick = thisTick;
  return(thisTick + overflows);
}

void initHardwareCycleCount()
{
  systick_hw->csr = 0x7;
  systick_hw->rvr = 0x00FFFFFF;
}

int16_t quartersin[1025];
int16_t fullSin[4097];
void buildSin()
{
  for(int i = 0; i < 1025; i++)
  {
    quartersin[i] = 4096 * sin(3.1415926535897932384626433832*i/2048.0);
  }
  for(int i = 0; i < 4097; i++)
  {
    fullSin[i] = 4096 * sin(3.1415926535897932384626433832*i/2048.0);
  }
}

void setIntPeriodCl(uint32_t period)
{
  intPeriodCl1 = period;
}
void setSamplingDelayCl1(uint32_t delayCl)
{
  samplingDelayCl1 = delayCl;
}
void setDownsamplingFactor(int factor)
{
  downsamplingFactor1 = byte(factor);
}
inline int16_t sin12bit(int16_t angle)
{
  //if(angle > 4096 || angle < 0) Serial.println("Error: Sin/Cos argument out of bounds");
  /*if(angle < 1024) return(quartersin[angle]);
  if(angle < 2048) return(quartersin[2048 - angle]);
  if(angle < 3072) return(-quartersin[angle - 2048]);
  return(-quartersin[4096 - angle]);*/
  return(fullSin[angle]);
  
  /* constant run time version, however more complicated
  int sign = 1- 2*(angle>2048)
  angle = angle&2048
  int slope = 1 - 2*(angle > 1024)
  return(sign*quartersin[1024-slope*1024 + slope*angle]
   */
}

inline int16_t cos12bit(int16_t angle)
{
  if(angle > 3072) angle -= 4096;

  return(sin12bit(angle+1024)); //cos is sin with 90deg phase shift
}

inline void alignRef(uint64_t clockCycle) //can be called by interrupt or rising edge watcher. Interrupt on the right core and ?inline?
{
  static uint64_t lastClock = 0;
  intPeriodCl1 = clockCycle - lastClock; //time difference
  lastClock = clockCycle;
  clockShift1 = clockCycle%intPeriodCl1;  //is modulo slow?!?
}

inline int32_t getSample()
{
  return(analogRead(A1));
}

void setup1()
{
  initHardwareCycleCount();
  /*  
  systick_hw->csr = 0x7;
  systick_hw->rvr = 0x00FFFFFF;
  //This enables the use of the getCycle() functions on core1 by starting the hardware timer on this core.
  //I am not sure if this timer is Jitter free, as it may use the overflow exception of core0 timer to increment??!??
  //Lines found in RP2040Support.h
  //There is also the amount of FIFO memory!!
  */
  pinMode(A1, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(REF_IN, INPUT);
  pinMode(REF_OUT, OUTPUT);
  pinMode(SAMPLE_SYNC, OUTPUT);
  buildSin();
}


int32_t Xbuf1 = 0;
int32_t Ybuf1 = 0;
byte downsamplingIndex1 = 0;
uint64_t nextSampleTime1 = 0;
float phi;
int16_t phi12bit;
bool lastRefIn;
bool thisRefIn;

void resetFastCore()
{
  downsamplingIndex1 = 0;
  Xbuf1 = 0;
  Ybuf1 = 0;
  nextSampleTime1 = 0;
  lastRefIn = 0;
}

void loop1()   //should be loop1 to avoid all standard arduino interrupts, but loop1 has no working getCycleCount64()
{
  //static uint64_t startTime;
  //static uint64_t difTime;
  static uint64_t clockCyc;

  if(fastCoreState == 1) //internal reference
  {
    //startTime = micros();
    //phi = 6.28318530717958647692528 * (rp2040.getCycleCount64()%intPeriodCl1)/float(intPeriodCl1);  //measure floating point speed or substitute rad by 0-4096 and use lookup tables sin/cos
    clockCyc = hardwareCycleCount();
    //phi12bit = 4096L * (clockCyc%intPeriodCl1) / intPeriodCl1; //overflows a 16bit for sure, a 32 bit maybe-> if intPeriodCl1 > 500k equivalent to 3.7ms of ref period.
    //if(phi12bit > 2048) digitalWrite(REF_OUT,0);
    //else digitalWrite(REF_OUT,1);
    if(clockCyc > nextSampleTime1)
    {
      int32_t thisSample = getSample();
      digitalWrite(SAMPLE_SYNC,1);
      //delayMicroseconds(1);
      digitalWrite(SAMPLE_SYNC,0);
      nextSampleTime1 = clockCyc + samplingDelayCl1;
      phi12bit = 4096L * (clockCyc%intPeriodCl1) / intPeriodCl1;
      /*Xbuf1 += thisSample * cos(phi);  //buf += getSample() * sin (phi) * filterChunkPrefactor[downsamplingIndex1]; this allows for better lowpass filter in some filter architecture. filterChunkPrefactor Array must be calculated beforehand by filter design.
      Ybuf1 += thisSample * sin(phi); */
      Xbuf1 += thisSample * cos12bit(phi12bit);  //buf += getSample() * sin (phi) * filterChunkPrefactor[downsamplingIndex1]; this allows for better lowpass filter in some filter architecture. filterChunkPrefactor Array must be calculated beforehand by filter design.
      Ybuf1 += thisSample * sin12bit(phi12bit);
      downsamplingIndex1++;
     
      if(downsamplingIndex1 >= downsamplingFactor1)
      {
        downsamplingIndex1 = 0;
        rp2040.fifo.push_nb((uint32_t(Xbuf1) | 1) -1);//push Xbuf and Ybuf through FIFO;
        rp2040.fifo.push_nb(uint32_t(Ybuf1) | 1);     //LSB is 0 for X and 1 for Y. This should not actually hurt resolution, as multiplication of two 12bit ints has onl 12 significant bits on output, but we are saving 24-1
        //TODO: Errormessage if FIFO is full
        Xbuf1 = 0;
        Ybuf1 = 0;
        /*digitalWrite(LED_BUILTIN, HIGH);
        delay(10);
        digitalWrite(LED_BUILTIN, LOW);*/    
      }
    }
  }
  
  if(fastCoreState == 2) //external Reference
  {
    clockCyc = hardwareCycleCount();
    thisRefIn = digitalRead(REF_IN); //accelerate digitalRead by using machine code / register??
    if(thisRefIn > lastRefIn)
    {
      alignRef(clockCyc);
    }
    lastRefIn = thisRefIn;
    //phi12bit = 4096L * (clockCyc%intPeriodCl1) / intPeriodCl1; //overflows a 16bit for sure, a 32 bit maybe-> if intPeriodCl1 > 500k equivalent to 3.7ms of ref period.
    //if(phi12bit > 2048) digitalWrite(REF_OUT,0);
    //else digitalWrite(REF_OUT,1);
    if(clockCyc > nextSampleTime1)
    {
      int32_t thisSample = getSample();
      digitalWrite(SAMPLE_SYNC,1);
      //delayMicroseconds(1);
      digitalWrite(SAMPLE_SYNC,0);
      nextSampleTime1 = clockCyc + samplingDelayCl1;
      phi12bit = 4096L * ((clockCyc-clockShift1)%intPeriodCl1) / intPeriodCl1;
      Xbuf1 += thisSample * cos12bit(phi12bit);  //buf += getSample() * sin (phi) * filterChunkPrefactor[downsamplingIndex1]; this allows for better lowpass filter in some filter architecture. filterChunkPrefactor Array must be calculated beforehand by filter design.
      Ybuf1 += thisSample * sin12bit(phi12bit);
      downsamplingIndex1++;
     
      if(downsamplingIndex1 >= downsamplingFactor1)
      {
        downsamplingIndex1 = 0;
        rp2040.fifo.push_nb((uint32_t(Xbuf1) | 1) -1);//push Xbuf and Ybuf through FIFO;
        rp2040.fifo.push_nb(uint32_t(Ybuf1) | 1);     //LSB is 0 for X and 1 for Y. This should not actually hurt resolution, as multiplication of two 12bit ints has onl 12 significant bits on output, but we are saving 24-1
        //TODO: Errormessage if FIFO is full
        Xbuf1 = 0;
        Ybuf1 = 0;
        /*digitalWrite(LED_BUILTIN, HIGH);
        delay(10);
        digitalWrite(LED_BUILTIN, LOW);*/    
      }
    }
  }
}
