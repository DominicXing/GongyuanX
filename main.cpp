#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"

using namespace std;

#include "rtdsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

struct Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  long long cycStart, cycStop;

  cycStart = rdtscll();


  //Calculated value of filterSize outside of for loop
  //Avoid recalculating value in every iteration
  int filterSize = filter -> getSize();

  //Same idea s above, take calculations of input_width and input_height
  //outside of loop
  int input_width = input -> width;
  int input_height = input -> height;

  //Take calculation of filterDivisor out of loop
  int filterDivisor = filter -> getDivisor();

  output -> width = input_width;
  output -> height = input_height;
  
  /*
  Reorder cols and rows in order to match the RowMajor ordering of matrices in C
  By finishing the elements of a row first before finishing the column, we allow
  the program to fetch elements continguously from memory.
 
  Unroll the loop 4-fold:
  Calculate 4 values for "value" with each iteration instead of just one. 
  Increment for loops by 4 with each iteration, progress through images 4 
  times faster. However, not necassarily execute overall program 4 times faster
  becauses of overhead of initializing extra variables and using extra registers
*/
 for(int plane = 0; plane < 3; plane++) {
    for(int row = 1; row < (input_height) - 4; row+=4) {
      for(int col = 1; col < (input_width) - 4 ; col+=4) {
      

  int value = 0;
  int value2 = 0;
  int value3 = 0;
  int value4 = 0;
	for (int j = 0; j < filterSize-3; j+=4) {
	  for (int i = 0; i < filterSize-3; i+=4) {
      
	  value = value +  input -> color[plane][row + i - 1][col + j - 1] * filter -> get(i, j);
      value2 = value2 + input ->color[plane][row + i][col + j] * filter -> get(i+1,j+1); 
      value3 = value3 + input ->color[plane][row + i + 1][col + j + 1] * filter -> get(i+2,j+2); 
      value4 = value4 + input ->color[plane][row + i + 2][col + j + 2] * filter -> get(i+3,j+4); 
	  }
	}
	value = value / filterDivisor;
	if ( value  < 0 ) { value = 0; }
	if ( value  > 255 ) { value = 255; }
	output -> color[plane][row][col] = value;

    value2 = value2 / filterDivisor;
    if ( value2  < 0 ) { value2 = 0; }
    if ( value2  > 255 ) { value2 = 255; }
    output -> color[plane][row+1][col+1] = value2;

    value3 = value3 / filterDivisor;
    if ( value3  < 0 ) { value3 = 0; }
    if ( value3  > 255 ) { value3 = 255; }
    output -> color[plane][row+2][col+2] = value3;

    value4 = value4 / filterDivisor;
    if ( value4  < 0 ) { value4 = 0; }
    if ( value4  > 255 ) { value4 = 255; }
    output -> color[plane][row+3][col+3] = value4;

      }
    }
  }

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}