/*
===============================================================================

  FILE:  srbufferrowbands.cpp
  
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
#include "srbufferrowbands.h"

#include "srwriter.h"
#include "srbufferrows.h"

#include <stdlib.h>
#include <string.h>

bool SRbufferRowBands::prepare(int nrows, int ncols, int nbits)
{
  // check parameters
  if (nrows <= 0 || ncols <= 0)
  {
    fprintf(stderr, "ERROR: nrows = %d and ncols = %d not supported by SRbufferRowBands\n", nrows, ncols);
    return false;
  }
  if (nbits <= 0 || nbits > 32)
  {
    fprintf(stderr, "ERROR: nbits = %d not supported by SRbufferRowBands\n", nbits);
    return false;
  }

  // copy parameters
  this->nrows = nrows;
  this->ncols = ncols;
  this->nbits = nbits;
  
  // create band buffers
  char file_name[256];
  num_bands = nrows / rows_per_band;
  band_buffers = (SRbuffer**)malloc(sizeof(SRbuffer*)*(num_bands + (nrows % rows_per_band ? 1 : 0)));
  for (int b = 0; b < num_bands; b++)
  {
    band_buffers[b] = new SRbufferRows();
    sprintf(file_name, "%s%04d.tmp", file_name_base, b);
    ((SRbufferRows*)(band_buffers[b]))->set_file_name(file_name);
    if (((SRbufferRows*)(band_buffers[b]))->prepare(rows_per_band, ncols, nbits) == false)
    {
      return false;
    }
  }
  if (nrows % rows_per_band)
  {
    band_buffers[num_bands] = new SRbufferRows();
    sprintf(file_name, "%s%04d.tmp", file_name_base, num_bands);
    ((SRbufferRows*)(band_buffers[num_bands]))->set_file_name(file_name);
    if (((SRbufferRows*)(band_buffers[num_bands]))->prepare(nrows % rows_per_band, ncols, nbits) == false)
    {
      return false;
    }
    num_bands++;
  }

  r_count = 0;
  r_clipped = 0;
  r_duplicate = 0;

  return true;
}

void SRbufferRowBands::write_raster(int row, int col, int value)
{
  if (row < 0 || col < 0 || row >= nrows || col >= ncols)
  {
    r_clipped++;
    return;
  }
  band_buffers[row / rows_per_band]->write_raster(row % rows_per_band, col, value);
  r_count++;
}

int SRbufferRowBands::required_sort_buffer_size() const
{
  int sbs, sort_buffer_size = 0; 
  for (int b = 0; b < num_bands; b++)
  {
    sbs = band_buffers[b]->required_sort_buffer_size();
    if (sbs > sort_buffer_size) sort_buffer_size = sbs;
  }
  return sort_buffer_size;
}

void SRbufferRowBands::sort_and_output(void* sort_buffer, SRwriter* srwriter)
{
  for (int b = 0; b < num_bands; b++)
  {
    band_buffers[b]->sort_and_output(sort_buffer, srwriter);
    r_duplicate += band_buffers[b]->r_duplicate;
  }
}

void SRbufferRowBands::set_file_name_base(const char* file_name_base)
{
  if (file_name_base == 0)
  {
    fprintf(stderr, "ERROR: file_name_base = NULL not supported by SRbufferRowBands\n");
  }
  else
  {
    free (this->file_name_base);
    this->file_name_base = strdup(file_name_base);
  }
}

void SRbufferRowBands::set_rows_per_band(int rows_per_band)
{
  if (rows_per_band <= 0)
  {
    fprintf(stderr, "ERROR: rows_per_band = %d not supported by SRbufferRowBands\n", rows_per_band);
  }
  else
  {
    this->rows_per_band = rows_per_band;
  }
}

SRbufferRowBands::SRbufferRowBands()
{
  band_buffers = 0;
  num_bands = -1;
  rows_per_band = 64;
  file_name_base = strdup("temp");
}

SRbufferRowBands::~SRbufferRowBands()
{
  if (band_buffers) free(band_buffers);
  free(file_name_base);
}
