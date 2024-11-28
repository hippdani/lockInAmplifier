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
// all these functions assume an Array of length N (global variable)
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

int32_t lowpassIIR(int32_t data[],  uint32_t counter)
{
  //calculations are done in 64 bit, because a & b are stored as int64_t. should never overflow, because a and b are only 16 bit precision and measured data is 31bit max.
  //a[0] should always be 32768 = 2^15 which is a reformatted 1 (format: Q17.15?)
  //by dividing by a[0] the result should be within 32bit (i hope).
  //possible improvement: reduce input to 16 bit and make all calculations int32 -> up to 20x faster. Save room for the additions!
  static int32_t c = 0;
  // lowercase x and y DO NOT represent X and Y (cos and sin) signal, but are filter input(x) and filter output(y)
  static int32_t xArr[MAX_FILTER_OD+1]; //contains the last measured values, rundspeicher. These are o copy of the last elements from the big measured data array that is not necessary for IIR
  static int32_t yArr[MAX_FILTER_OD+1]; //contains the last filtered values for self reference by the IIR function
  xArr[Ishift(0,c)] = data[counter]; //read newest measurement value
  yArr[Ishift(0,c)] = (xArr[Ishift(0,c)] * b[0] + xArr[Ishift(-1,c)] * b[1] + xArr[Ishift(-2,c)] * b[2] - yArr[Ishift(-1,c)] * a[1] - yArr[Ishift(-2,c)] * a[2])/a[0];
  /*
  // Apply 8th order IIR filter
  yArr[Ishift(0, c)] = (xArr[Ishift(0, c)] * b[0] +
                        xArr[Ishift(-1, c)] * b[1] +
                        xArr[Ishift(-2, c)] * b[2] +
                        xArr[Ishift(-3, c)] * b[3] +
                        xArr[Ishift(-4, c)] * b[4] +
                        xArr[Ishift(-5, c)] * b[5] +
                        xArr[Ishift(-6, c)] * b[6] +
                        xArr[Ishift(-6, c)] * b[7] +
                        xArr[Ishift(-7, c)] * b[8] -
                        yArr[Ishift(-1, c)] * a[1] -
                        yArr[Ishift(-2, c)] * a[2] -
                        yArr[Ishift(-3, c)] * a[3] -
                        yArr[Ishift(-4, c)] * a[4] -
                        yArr[Ishift(-5, c)] * a[5] -
                        yArr[Ishift(-6, c)] * a[6] -
                        yArr[Ishift(-6, c)] * a[7] -
                        yArr[Ishift(-7, c)] * a[8]) / a[0];
  */
  int32_t result = yArr[Ishift(0,c)]; //necessary for return becaus c gets incremented aferwards. Solvable by moving incrementation to begin of code?
  c++;
  if(c >= MAX_FILTER_OD+1)
  {
    c = 0;
  }
  return(result);
}

inline int32_t Ishift(int32_t index, int32_t counter) //works on xArr/yArr. takes the corresponding counter variable (rundspeicher) and the desired index and returns the actual (memory = array) index. index are 0 or negative.
{
  int32_t i = index+counter;
  if(i<0) i+= MAX_FILTER_OD+1;
  if(i<0 || i > MAX_FILTER_OD) return(0); //something went wrong. This line should be unneccesary but makes the program more memory safe
  return(i);
}
