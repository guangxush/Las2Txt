/*
===============================================================================

  FILE:  srbufferrows.h

  CONTENTS:

    Writes the raster points contained in a band of grid rows in some order to
    disk that is storage efficient.

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    17 June 2006 -- created after missing the Alice Summerfest in GG Park

===============================================================================
*/
#ifndef SRBUFFER_ROWS_H
#define SRBUFFER_ROWS_H

#include <stdio.h>

#include "srbuffer.h"

class SRbufferRows : public SRbuffer
{
public:

  // srbuffer interface function implementations

  bool prepare(int nrows, int ncols, int nbits);
  void write_raster(int row, int col, int value);
  int required_sort_buffer_size() const;
  void sort_and_output(void* sort_buffer, SRwriter* srwriter);

  // srbufferrows function implementations

  void set_file_name(const char* file_name);

  SRbufferRows();
  ~SRbufferRows();

private:
  FILE* file;
  char* file_name;

  void* row_buffer;

  int row_bits;
  int col_bits;
  int k_bits;

  unsigned int bits_buffer;
  int bits_number;

  void write_bit(int bit);
  void write_bits(int nbits, int bits);
  void output_value(int last_value, int value);
  void output_row(int row);

  int read_bit();
  int read_bits(int nbits);
  int input_value(int last_value);
};

#endif
