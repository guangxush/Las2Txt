/*
===============================================================================

  FILE:  srbufferinmemory.h

  CONTENTS:

    Stores the raster points in memory until they are output in to a file. This
    is obviously only recommended for small images and for test purposes.

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    18 June 2006 -- after having finally signed the back-sale of mom's kangoo

===============================================================================
*/
#ifndef SRBUFFER_MEMORY_H
#define SRBUFFER_MEMORY_H

#include <stdio.h>

#include "srbuffer.h"

class SRbufferInMemory : public SRbuffer
{
public:

  // srbuffer interface function implementations

  bool prepare(int nrows, int ncols, int nbits);
  void write_raster(int row, int col, int value);
  int required_sort_buffer_size() const;
  void sort_and_output(void* sort_buffer, SRwriter* srwriter);

  // srbufferrows function implementations

  SRbufferInMemory();
  ~SRbufferInMemory();

private:
  int* buffer;
  unsigned int* nodata;
};

#endif
