//axis names: z=0, x=1, y=2

/*****************************************************
 * Adapted code from:
 *
 * Paul van Haastrecht
 * Original Creation date: February 2018 / version 1.0
 *
 * This example demonstrates the basic setting of a DAC output.
 * The starting level is asked and this written and read back from the
 * AD57xx. If DAC output is connected to the defined analog input the
 * expected voltage level is shown.
 *
 * The hardware pin connection on AD57xx
 *
 * Pin 1, 15, 18, 19, 20 21       GND
 * Pin 2, 4,  6,  12, 13, 22      not connected
 * Pin 5, 10, 11, 14, 24          +5V    //pin 10 = GND !!!!!!!!
 * Pin 17                         RFIN    //2.5V Reference TL431
 *
 *                   --------        --------
 *      +5V ---------| 1K   |----.---| 1K   |----- GND
 *                   --------     |  --------
 *                                |
 *                               REFIN
 *
 * default setting
 * AD57xx   Arduino Uno
 * Pin 3    A0          DAC-A out
 * Pin 7     9          Sync
 * Pin 8    13          clock
 * Pin 9    11          SDIN
 * Pin 16   12          SDO
 * pin 23   A1          DAC-B out
 *
 * This  code is released under the MIT license
 *
 * Distributed as-is: no warranty is given
 ******************************************************/

#include <stdint.h>
#include <AD57mod2.h>
// MUST : select the kind of AD57xx you have:
// AD5722 or AD5732 or AD5752
uint8_t DacType = AD5752;
// MUST : select the pin to which SYNC .... CS is connected
//char slave_pin_DAC = ADC_FSYNC_PIN;  not used anymore, has to be defined in AD57Class.begin(...) as last Argument
#define output_range p5V
//AD57Class AD57; //not needed because its in the librarys .h file

AD57Class AD57_Q(SPI);
AD57Class AD57_P(SPI1);


//Problem: DacType and slave_pin_DAC is a public variable, so only one DAC can be addressed!!!?! 
//TODO: AD57.setSPIspeed();, Get SPI1 working with library

void dac_init()
{
  #ifndef __AVR_ATmega328P__
    SPI.setRX(MISO0); //MISO?
    SPI.setTX(MOSI0); //MOSI?
    SPI.setSCK(SPICLK0);//SPI Clock

    SPI1.setRX(MISO1); //MISO  //Setting Pins for Second hardware SPI interface
    SPI1.setTX(MOSI1); //MOSI?
    SPI1.setSCK(SPICLK1);//SPI Clock
  #endif
  // set output voltage range
  AD57_Q.begin(DAC_A, output_range, ADC_Q_FSYNC_PIN);
  AD57_Q.setRange(DAC_B, output_range);
  // enable therminal shutdown and overcurrent protection
  // also set make sure that CLR is NOT set, starting from 0v after OP_CLEAR op_code)
  AD57_Q.setControl(OP_INSTR,(SET_TSD_ENABLE |SET_CLAMP_ENABLE|STOP_CLR_SET|SET_SDO_DISABLE));
  // clear the DAC outputs
  AD57_Q.setControl(OP_CLEAR);

  AD57_P.begin(DAC_A, output_range, ADC_P_FSYNC_PIN);
  AD57_P.setRange(DAC_B, output_range);
  // enable therminal shutdown and overcurrent protection
  // also set make sure that CLR is NOT set, starting from 0v after OP_CLEAR op_code)
  AD57_P.setControl(OP_INSTR,(SET_TSD_ENABLE |SET_CLAMP_ENABLE|STOP_CLR_SET|SET_SDO_DISABLE));
  // clear the DAC outputs
  AD57_P.setControl(OP_CLEAR);

  
  dac_powerup();
}

void dac_set(byte axis, unsigned int target)
{
  target = limitInt(target, 0, 65535);
  if(axis == 0)
  {
    AD57_P.setDac(DAC_A, target);
  } 
  if(axis == 1)
  {
    AD57_Q.setDac(DAC_A, target);
  }
  if(axis == 2)
  {
    AD57_P.setDac(DAC_B, target);
  }
}

void dac_clear()
{
  AD57_Q.setControl(OP_CLEAR);
  AD57_P.setControl(OP_CLEAR);
}

void dac_shutdown()
{
  AD57_Q.setPower(STOP_PUA | STOP_PUB);
  AD57_P.setPower(STOP_PUA | STOP_PUB);
}

void dac_powerup()
{
  AD57_Q.setPower(SET_PUA | SET_PUB);
  AD57_P.setPower(SET_PUA | SET_PUB);
}

void dac_soft_return(long currentPos, int axis)
{
  currentPos = limitInt(currentPos, 0, 65535);
  if(SIM == 0)
  {
    for(int i = currentPos; i >= 0; i--) //Slowly return Z-Channel to 0V
    {
      dac_set(axis, i);
      delayMicroseconds(100);
    }
  }
  Serial.print("\n Channel Nr. ");Serial.print(axis); Serial.print(" at 0\n");
}

bool adc_check_status(bool disp)   //adapt for two DACs!!!!!!!!!!!!!!!!!!!!!
{
    uint16_t stat = AD57_Q.getStatus();
    bool ret = 0;
    
    if (stat & stat_err_TO)
    {
      Serial.println(F("Therminal overheat shutdown"));
      ret = 1;
    }
    
    if (stat & stat_err_CA)
    {
      Serial.println(F("DAC - A overcurrent detected"));
      ret = 1;
    }

    if (stat & stat_err_CB)
    {
      Serial.println(F("DAC - B overcurrent detected"));
      ret = 1;
    }

    if (disp == 1)
    {
      Serial.println(F("Display settings\n"));
      
      if (stat & stat_TS)
         Serial.println(F("Therminal shutdown protection enabled"));
      else
         Serial.println(F("Therminal shutdown protection disabled"));

      if (stat & stat_CLR)
        Serial.println(F("DAC output set to midscale or fully negative with OP_CLEAR command"));
      else
        Serial.println(F("DAC output set to 0V with OP_CLEAR command"));
      
      if (stat & stat_CLAMP)
         Serial.println(F("Overcurrent protection enabled"));
      else
         Serial.println(F("Overcurrent protection disabled"));

      if (stat & stat_SDO) // you will never see this one :-) //You thought dear Paul, but I saw it myself!
         Serial.println(F("Serial data out disabled"));
      else          
         Serial.println(F("Serial data out enabled"));

      Serial.println();
    }

    return(ret);
}

//AD57.loadDac();
