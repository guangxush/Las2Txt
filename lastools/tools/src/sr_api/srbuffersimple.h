/*
===============================================================================

  FILE:  srbuffersimple.h

  CONTENTS:

    Writes the raster points as they come in to a temporary file on disk using 
    ceil(log2(nrows))+ceil(log2(ncols))+nbits per raster. For the final quicksort
    in memory that brings the rasters into the row-by-row order it uses as few
    bytes per row / col / value entry as possible.

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    17 June 2007 -- completely reworked on the day of Alice's Summerfest

===============================================================================
*/
#ifndef SRBUFFER_SIMPLE_H
#define SRBUFFER_SIMPLE_H

#include <stdio.h>

#include "srbuffer.h"

class SRbufferSimple : public SRbuffer
{
public:

  // srbuffer interface function implementations

  bool prepare(int nrows, int ncols, int nbits);
  void write_raster(int row, int col, int value);
  int required_sort_buffer_size() const;
  void sort_and_output(void* sort_buffer, SRwriter* srwriter);

  // srbuffersimple function implementations

  void set_file_name(const char* file_name);

  SRbufferSimple();
  ~SRbufferSimple();

private:
  FILE* file;
  char* file_name;

  int row_bits;
  int col_bits;

  unsigned int bits_buffer;
  int bits_number;

  void write_bits(int nbits, int bits);
  int read_bits(int nbits);
};

#endif
