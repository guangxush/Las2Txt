/*
===============================================================================

  FILE:  srbuffertiles.h

  CONTENTS:

    Writes the raster points contained in a tile in whatever order they come
    in to a temporary file on disk from where they are expected to be loaded
    in later to be assembled into a raster grid. Uses buffering and difference
    coding to be more storage efficient.

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2007 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    WARNING: implementation is not complete

    15 June 2007 -- created after mark brought home loaves of bread & baguette

===============================================================================
*/
#ifndef SRBUFFER_TILES_H
#define SRBUFFER_TILES_H

#include <stdio.h>

#include "srbuffer.h"

class SRbufferTiles : public SRbuffer
{
public:

  // srbuffer interface function implementations

  bool prepare(int nrows, int ncols, int nbits);
  void write_raster(int row, int col, int value);
  int required_sort_buffer_size() const;
  void sort_and_output(void* sort_buffer, SRwriter* srwriter);

  // srbuffertiles function implementations

  void set_file_name(const char* file_name);

  SRbufferTiles();
  ~SRbufferTiles();

private:
  FILE* file;
  char* file_name;

  int nrows;
  int ncols;
  int nbits;

  int row_bits;
  int col_bits;
  int k_bits;

  unsigned int* row_occupancy;
  unsigned int* col_occupancy;

  void* buffer;
  int buffer_entries;
  int buffer_written;

  unsigned int bits_buffer;
  int bits_number;

  void write_bit(int bit);
  void write_bits(int nbits, unsigned int bits);
  void write_range(unsigned int range, unsigned int value);
  void output_value(unsigned int last_value, unsigned int value);
  int output_row(int entry, int row);
  int output_col(int entry, int col);
  void output_buffer();

  int read_bit();
  int read_bits(int nbits);
  int read_range(unsigned int range);
  int input_value(int last_value);
};

#endif
