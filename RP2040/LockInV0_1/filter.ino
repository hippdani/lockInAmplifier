//implementation of different filters: Try IIR and FIR


/*void setFilterCoeff(int32_t A[8], int32_t B[8])
{
  for(int i = 0; i < 8; i++)
  {
    a[i] = A[i];
    b[i] = B[i];
  }
}*/

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
    if(counter > len) counter = 0;
  }
  return(avg/X);
}
