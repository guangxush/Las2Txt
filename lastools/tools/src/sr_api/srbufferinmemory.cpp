/*
===============================================================================

  FILE:  srbufferinmemory.cpp
  
  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "srbufferinmemory.h"

#include "srwriter.h"

#include <stdlib.h>
#include <string.h>

void SRbufferInMemory::write_raster(int row, int col, int value)
{
  if (row < 0 || col < 0 || row >= nrows || col >= ncols)
  {
    r_clipped++;
    return;
  }
  if (nodata[(row*ncols+col)/32] & (1 << ((row*ncols+col)%32)))
  {
    r_duplicate++;
  }
  else
  {
    nodata[(row*ncols+col)/32] |= (1 << ((row*ncols+col)%32));
  }
  if (nbits <= 8)
  {
    ((unsigned char*)buffer)[row*ncols+col] = (unsigned char)value;
  }
  else if (nbits <= 16)
  {
    ((short*)buffer)[row*ncols+col] = (short)value;
  }
  else
  {
    buffer[row*ncols+col] = value;
  }
  r_count++;
}

bool SRbufferInMemory::prepare(int nrows, int ncols, int nbits)
{
  // check parameters
  if (nrows <= 0 || ncols <= 0)
  {
    fprintf(stderr, "ERROR: nrows = %d and ncols = %d not supported by SRbufferInMemory\n", nrows, ncols);
    return false;
  }
  if (nbits <= 0 || nbits > 32)
  {
    fprintf(stderr, "ERROR: nbits = %d not supported by SRbufferInMemory\n", nbits);
    return false;
  }

  // copy parameters
  this->nrows = nrows;
  this->ncols = ncols;
  this->nbits = nbits;
  
  // create value buffer
  if (nbits <= 8)
  {
    buffer = (int*)malloc(sizeof(unsigned char)*nrows*ncols);
  }
  else if (nbits <= 16)
  {
    buffer = (int*)malloc(sizeof(short)*nrows*ncols);
  }
  else
  {
    buffer = (int*)malloc(sizeof(int)*nrows*ncols);
  }
  if (!buffer)
  {
    fprintf(stderr, "ERROR: failed memory alloc for %d rasters in SRbufferInMemory\n", nrows*ncols);
    return false;
  }

  // create nodata buffer
  nodata = (unsigned int*)malloc(sizeof(unsigned int)*(nrows*ncols/32 + 1));
  if (!nodata)
  {
    fprintf(stderr, "ERROR: failed memory alloc for %d nodata flags in SRbufferInMemory\n", nrows*ncols);
  }
  for (int i = 0; i <= nrows*ncols/32; i++) nodata[i] = 0;

  r_count = 0;
  r_clipped = 0;
  r_duplicate = 0;

  return true;
}

int SRbufferInMemory::required_sort_buffer_size() const
{
  return 0;
}

void SRbufferInMemory::sort_and_output(void* sort_buffer, SRwriter* srwriter)
{
  if (nbits <= 8)
  {
    for (int i = 0; i < nrows*ncols; i++)
    {
      if (nodata[i/32] & (1 << (i%32)))
      {
        srwriter->write_raster(((unsigned char*)buffer)[i]);
      }
      else
      {
        srwriter->write_nodata();
      }
    }
  }
  else if (nbits <= 16)
  {
    for (int i = 0; i < nrows*ncols; i++)
    {
      if (nodata[i/32] & (1 << (i%32)))
      {
        srwriter->write_raster(((short*)buffer)[i]);
      }
      else
      {
        srwriter->write_nodata();
      }
    }
  }
  else
  {
    for (int i = 0; i < nrows*ncols; i++)
    {
      if (nodata[i/32] & (1 << (i%32)))
      {
        srwriter->write_raster(buffer[i]);
      }
      else
      {
        srwriter->write_nodata();
      }
    }
  }
}

SRbufferInMemory::SRbufferInMemory()
{
  buffer = 0;
  nodata = 0;
}

SRbufferInMemory::~SRbufferInMemory()
{
  if (buffer) free(buffer);
  if (nodata) free(nodata);
}
