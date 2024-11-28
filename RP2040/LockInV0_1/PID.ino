#define DN 5 //Number of measurements to include into Averaging Array
#define CHANGELIM 100
#define ILIM 100000

long long iTerm = 0;
int eArray[DN];
int eIndex = 0;
float eAvg;
float eAvg_old;
long Zpos = 0;
double virtualZpos = 0;

long pid(float kp, float ki, float kd, unsigned int setpoint, int timestep, bool startup)  //timestep was in ms, now in us!
{
  int error = get_actual_value() - setpoint;
  iTerm += error*timestep;

  //Anti Wind up missing -> crude Anti Wind up
  if(iTerm > ILIM) iTerm = ILIM;
  if(iTerm < -ILIM) iTerm = -ILIM;
  
  if(startup)
  {
    for(int j = 0; j < DN; j++)   
    {
      eArray[j] = error;
    }
    eAvg_old = error;
  }
  eArray[eIndex] = error; //save e into 'rundspeicher' array
  eIndex ++;
  if(eIndex >= DN) eIndex = 0;
  eAvg = makeAvg(eArray, DN);  //compute the average of the updatet positional errors
  float dTerm = (eAvg - eAvg_old); // /(timestep/1000.0); division through timestep can be omitted, because the multiplication with timestep is omitted when calculating out_change (they cancel out)
  //dTerm limiter may be necesarry
  
  eAvg_old = eAvg;
  //Serial.println(); Serial.print("P:"); Serial.print(error); Serial.print(" i:"); Serial.print(iTerm);Serial.print(" d:"); Serial.print(dTerm);
  float out_change = (kp*error + ki*(iTerm/1000.0))*timestep/1000.0 + kd*dTerm; //division "/1000.0" to preserve scaling of ms timesteps, dTerm excluded from multiplication because it cancels out, see above
  //change limiter:
  if(out_change > CHANGELIM) out_change = CHANGELIM;
  if(out_change < -CHANGELIM) out_change = -CHANGELIM;
  
  virtualZpos = virtualZpos + out_change;
  if(virtualZpos > 0xFFFF) virtualZpos = 0xFFFF;
  if(virtualZpos < 0) virtualZpos = 0;
  Zpos = virtualZpos + 0.5; //add 0.5 so casting obeys the correct rounding norm
  if(Zpos > 0xFFFF) Zpos = 0xFFFF;
  if(Zpos < 0) Zpos = 0;

  if(SIM == 0) dac_set(0, Zpos);
  return(Zpos);
}

void pid_reset()
{
  iTerm = 0;
  for(int j = 0; j < DN; j++)   
  {
    eArray[j] = 0;
  }
  eAvg_old = 0;
  Zpos = 0; //Not sure if this one is useful to rest?!? It sure is for the Simulation
  virtualZpos = 0; //It is indeed useful to reset, but now also (or only) the virtual component!
}

unsigned int get_actual_value()
{
  return(analogRead(QTF_AMP_PIN));
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

float makeAvg(int avgArray[],int n) //returns average (int) of first n avgArray entrys. avgArray has to have at least n elements
{
  int64_t sum=0;
  for(int j = 0; j< n; j++)
  {
    sum = sum + avgArray[j];
  }
  return(sum/(float)n);
}
