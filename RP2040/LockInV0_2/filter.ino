//implementation of different filters: Try IIR and FIR


/*void setFilterCoeff(int32_t A[8], int32_t B[8])
{
  for(int i = 0; i < 8; i++)
  {
    a[i] = A[i];
    b[i] = B[i];
  }
}*/

int32_t FIResponse[N];

void fillFIResponse()//coefficients could be argument? // higher order filters possible with more than 1 argument
{
  //implementation of a 1st order Lowpass similar to RC lowpass
  for (int i = 0; i < N; i++)
  {
    FIResponse[i] = (1<<16) * exp(float(-i)/float(stdTauLowpass)); //16 bit precision exponential decay  //stdTauLowpass not in ms but in pre filter samples (after core1 downsampling)
  }
  int tailLength = 10; // must be larger than 0. tailLength = 1 does nothing.
  if(N > tailLength)  //remove hard cutoff at the tail of the exponential decay
  {
    int32_t tailStep = FIResponse[N-tailLength];
    for (int i = 1; i < tailLength; i++)
    {
      FIResponse[N-i] = tailStep*i/tailLength; //linear decay to 0
    }
  }
}

int32_t lowpass(int32_t data[], uint32_t len, uint32_t counter)
{
  //this should implement an IIR structure with coefficients from a public array.
  int64_t avg = 0;
  //for now this will contain just a moving average over X samples
  int i = 0; 
  int32_t X = a[0];
  while(i < X)
  {
    avg += data[counter];
    counter++;
    i++;
    if(counter >= len) counter = 0;
  }
  return(avg/X);
}

int32_t lowpassFIR(int32_t data[],  uint32_t counter)
{
  //this should implement an IIR structure with coefficients from a public array.
  int64_t avg = 0;
  //for now this will contain just a moving average over X samples
  int i = 0; 
  uint32_t X = stdTauLowpass;
  while(i < X)
  {
    avg += data[counter]*FIResponse[counter];
    counter++;
    i++;
    if(counter >= N) counter = 0;
  }
  return(avg/X);
}
