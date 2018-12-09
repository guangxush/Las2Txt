/*
===============================================================================

  FILE:  srbufferrowbands.h

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
#ifndef SRBUFFER_ROW_BANDS_H
#define SRBUFFER_ROW_BANDS_H

#include <stdio.h>

#include "srbuffer.h"

class SRbufferRowBands : public SRbuffer
{
public:

  // srbuffer interface function implementations

  bool prepare(int nrows, int ncols, int nbits);
  void write_raster(int row, int col, int value);
  int required_sort_buffer_size() const;
  void sort_and_output(void* sort_buffer, SRwriter* srwriter);

  // srbufferrows function implementations

  void set_file_name_base(const char* file_name_base);
  void set_rows_per_band(int rows_per_band);

  SRbufferRowBands();
  ~SRbufferRowBands();

private:
  SRbuffer** band_buffers;
  int num_bands;
  int rows_per_band;
  char* file_name_base;
};

#endif
