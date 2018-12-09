/*
===============================================================================

  FILE:  SRbuffer.h
  
  CONTENTS:
  
    Streaming Buffer for a stream of raster that represent a row-by-row orderd
    grid (e.g. an image or an elevation grid)
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2006  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    17 February 2006 -- created after skyping with jack for three hours 
  
===============================================================================
*/
#ifndef SRBUFFER_H
#define SRBUFFER_H

class SRwriter;

class SRbuffer
{
public:

  // buffer variables

  int nrows;
  int ncols;
  int nbits;
  int r_count;

  int r_clipped;
  int r_duplicate;

  // virtual functions

  virtual bool prepare(int nrows, int ncols, int nbits)=0;
  virtual void write_raster(int col, int row, int value)=0;
  virtual int required_sort_buffer_size() const=0;
  virtual void sort_and_output(void* sort_buffer, SRwriter* srwriter)=0;

  SRbuffer(){nrows = ncols = nbits = r_count = r_clipped = r_duplicate = -1;};
  virtual ~SRbuffer(){};
};

#endif
