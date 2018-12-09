/*
===============================================================================

  FILE:  SRwriter_bil_fseek.cpp
  
  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006  martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "srwriter_bil_fseek.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLLECT_STATISTICS
//#undef COLLECT_STATISTICS

#define NO_CHOKE_STUPID_3DEM_VIEWER
#undef NO_CHOKE_STUPID_3DEM_VIEWER

#ifdef COLLECT_STATISTICS
static int total_rasters = 0;
static int clipped_rasters = 0;
static int duplicate_rasters = 0;
#endif

#if defined(BUFFER_ROWS) || defined(BUFFER_ROWS_DONT_INITIALIZE_NODATA)

#define ROW_BUFFER_ENTRIES 1024

typedef struct RowEntry
{
  short col;
  short value;
} RowEntry;

typedef struct RowBuffer
{
  int num_entries;
  RowEntry entries[ROW_BUFFER_ENTRIES];
} RowBuffer;

static void quicksort_row_buffer(RowEntry* a, int i, int j)
{
  int in_i = i;
  int in_j = j;
  RowEntry w;
  short col = a[(i+j)/2].col;
  do
  {
    while ( a[i].col > col ) i++;
    while ( a[j].col < col ) j--;
    if (i<j)
    {
      w = a[i];
      a[i] = a[j];
      a[j] = w;
    }
  } while (++i<=--j);
  if (i == j+3)
  {
    i--;
    j++;
  }
  if (j>in_i) quicksort_row_buffer(a, in_i, j);
  if (i<in_j) quicksort_row_buffer(a, i, in_j);
}

void SRwriter_bil_fseek::row_buffered_write(int row, int col, short value)
{
  RowBuffer* buffer = ((RowBuffer*)row_buffer) + row;
  buffer->entries[buffer->num_entries].col = col;
  buffer->entries[buffer->num_entries].value = value;
  buffer->num_entries++;
  if (buffer->num_entries == ROW_BUFFER_ENTRIES)
  {
    output_row_buffer(row);
  }
}

void SRwriter_bil_fseek::output_row_buffer(int row)
{
  RowBuffer* buffer = ((RowBuffer*)row_buffer) + row;
  quicksort_row_buffer(buffer->entries, 0, buffer->num_entries-1);
  short write_buffer[ROW_BUFFER_ENTRIES];
  int entry = buffer->num_entries-1;
  while (entry >= 0)
  {
    int col = buffer->entries[entry].col;
    write_buffer[0] = buffer->entries[entry].value;
    int num = 1;
    entry--;
    while (entry >= 0 && buffer->entries[entry].col == buffer->entries[entry+1].col+1)
    {
      write_buffer[num] = buffer->entries[entry].value;
      num++;
      entry--;
    }
    fseek(file_bil, (row*ncols+col)*nbands*2, SEEK_SET);
    fwrite(&write_buffer, 2, num, file_bil);
  }
  buffer->num_entries = 0;
}

#endif

bool SRwriter_bil_fseek::open(FILE* file_bil, FILE* file_hdr, FILE* file_blw)
{
  if (file_bil == 0)
  {
    fprintf(stderr, "ERROR: zero file pointer not supported by SRwriter_bil\n");
    return false;
  }

  this->file_hdr = file_hdr;
  this->file_blw = file_blw;
  this->file_bil = file_bil;

  nbands = 1;
  nbits = 16;

  r_count = 0;

  return true;
}

void SRwriter_bil_fseek::write_header()
{
  urx = llx+stepx*ncols;
  ury = lly+stepy*nrows;

  // write header file

  if (file_hdr)
  {
    fprintf(file_hdr, "nrows %d\012", nrows);
    fprintf(file_hdr, "ncols %d\012", ncols);
    fprintf(file_hdr, "nbands %d\012", nbands);
    fprintf(file_hdr, "nbits %d\012", nbits);
    fprintf(file_hdr, "layout bil\012");
    fprintf(file_hdr, "nodata %d\012", nodata);
#if defined(_WIN32)   // if little endian machine (i == intel, m == motorola)
    fprintf(file_hdr, "byteorder I\012");
#else                 // else big endian machine
    fprintf(file_hdr, "byteorder M\012");
#endif
    // we also output the following info (although it may be specified in the world file)
    if (llx != -1.0f) fprintf(file_hdr, "ulxmap %g\012", llx);
    if (lly != -1.0f) fprintf(file_hdr, "ulymap %g\012", ury);
    if (stepx != -1.0f) fprintf(file_hdr, "xdim %g\012", stepx);
    if (stepy != -1.0f) fprintf(file_hdr, "ydim %g\012", stepy);

    // write world file

    if (file_blw)
    {
#ifdef NO_CHOKE_STUPID_3DEM_VIEWER
      fprintf(file_blw, "0.01666360294118\012");
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "0.01666360294118\012");
      fprintf(file_blw, "-90.0\012");
      fprintf(file_blw, "50.0\012");
#else
      fprintf(file_blw, "%g\012", stepx);
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "%g\012", -stepy);
      fprintf(file_blw, "%g\012", llx);
      fprintf(file_blw, "%g\012", lly + ncols * stepy);
#endif
    }
  }

  // initialize buffers and output file

#if defined(BUFFER_ROWS)

  fprintf(stderr, "BUFFER_ROWS\n");

  if (nbits == 16)
  {
    row_buffer = malloc(sizeof(RowBuffer)*nrows);
    for (int i = 0; i < nrows; i++) ((RowBuffer*)row_buffer)[i].num_entries = 0;
    short nodata = this->nodata;
    for (  i = nrows*ncols*nbands - 1; i >= 0; i--) fwrite(&nodata, 2, 1, file_bil);
  }
  else
  {
    fprintf(stderr, "ERROR: nbits == %d not supported by BUFFER_ROWS\n", nbits);
    exit(1);
  }

#elif defined(BUFFER_ROWS_DONT_INITIALIZE_NODATA)

  fprintf(stderr, "BUFFER_ROWS and DONT_INITIALIZE_NODATA\n");

  if (nbits == 16)
  {
    row_buffer = malloc(sizeof(RowBuffer)*nrows);
    for (int i = 0; i < nrows; i++) ((RowBuffer*)row_buffer)[i].num_entries = 0;
    short nodata = this->nodata;
    fseek(file_bil, (ncols*nrows*nbands-1)*2, SEEK_SET);
    fwrite(&nodata, 2, 1, file_bil);
  }
  else
  {
    fprintf(stderr, "ERROR: nbits == %d not supported by BUFFER_ROWS_DONT_INITIALIZE_NODATA\n", nbits);
    exit(1);
  }

#elif defined (BUFFER_NOTHING)

  fprintf(stderr, "BUFFER_NOTHING\n");

  // init the grid file with the value "nodata" for each raster
  if (nbits == 16)
  {
    short nodata = this->nodata;
    for (int i = nrows*ncols*nbands - 1; i >= 0; i--) fwrite(&nodata, 2, 1, file_bil);
  }
  else
  {
    for (int i = nrows*ncols*nbands - 1; i >= 0; i--) fwrite(&nodata, 4, 1, file_bil);
  }

#elif defined (BUFFER_NOTHING_DONT_INITIALIZE_NODATA)

  fprintf(stderr, "BUFFER_NOTHING and DONT_INITIALIZE_NODATA\n");

  // create an uninitialized grid file simple by writing the last raster
  if (nbits == 16)
  {
    short nodata = this->nodata;
    fseek(file_bil, (ncols*nrows*nbands-1)*2, SEEK_SET);
    fwrite(&nodata, 2, 1, file_bil);
  }
  else
  {
    fseek(file_bil, (ncols*nrows*nbands-1)*4, SEEK_SET);
    fwrite(&nodata, 4, 1, file_bil);
  }
#endif
}

void SRwriter_bil_fseek::write_raster(int col, int row, float value)
{
  if (row < 0 || col < 0 || row >= nrows || col >= ncols)
  {
#ifdef COLLECT_STATISTICS
    clipped_rasters++;
#endif
    // this point is being clipped
    return;
  }

#if defined (BUFFER_ROWS) || defined(BUFFER_ROWS_DONT_INITIALIZE_NODATA)

  row_buffered_write(row, col, (short)(value + 0.5f));

#elif defined (BUFFER_NOTHING) || defined (BUFFER_NOTHING_DONT_INITIALIZE_NODATA)

  if (nbits == 16)
  {
    short raster_value = (short)(value + 0.5f);
    fseek(file_bil, (row*ncols+col)*nbands*2, SEEK_SET);
    fwrite(&raster_value, 2, 1, file_bil);
  }
  else
  {
    int raster_value = (int)(value + 0.5f);
    fseek(file_bil, (row*ncols+col)*nbands*4, SEEK_SET);
    fwrite(&raster_value, 4, 1, file_bil);
  }

#endif

  r_count++;
}

void SRwriter_bil_fseek::write_raster(int raster)
{
  if (nbits == 16)
  {
    short r = (short)raster;
    fwrite(&r, 2, 1, file_bil);
  }
  else if (nbits == 32)
  {
    fwrite(&raster, 4, 1, file_bil);
  }
  else
  {
    fprintf(stderr, "ERROR: nbits %d not supported by SRwriter_bil_fseek\n",nbits);
  }
  r_count++;
}

void SRwriter_bil_fseek::write_nodata()
{
  if (nbits == 16)
  {
    short nodata = (short)this->nodata;
    fwrite(&nodata, 2, 1, file_bil);
  }
  else if (nbits == 32)
  {
    fwrite(&nodata, 4, 1, file_bil);
  }
  else
  {
    fprintf(stderr, "ERROR: nbits %d not supported by SRwriter_bil_fseek\n",nbits);
  }
  r_count++;
}

void SRwriter_bil_fseek::close()
{
#if defined (BUFFER_ROWS) || defined(BUFFER_ROWS_DONT_INITIALIZE_NODATA)

  for (int i = 0; i < nrows; i++)
  {
    if (((RowBuffer*)row_buffer)[i].num_entries)
    {
      output_row_buffer(i);
    }
  }

#endif

#ifdef COLLECT_STATISTICS
  fprintf(stderr,"surviving rasters %d clipped rasters %d duplicate rasters %d\n", r_count, clipped_rasters, duplicate_rasters);
#else
  fprintf(stderr,"raster count %d\n",r_count);
#endif

  r_count = -1;
}

SRwriter_bil_fseek::SRwriter_bil_fseek()
{
  // init of SRwriter_bil_fseek interface
  file_bil = 0;
  file_hdr = 0;
  file_blw = 0;

#if defined (BUFFER_ROWS) || defined(BUFFER_ROWS_DONT_INITIALIZE_NODATA)
  row_buffer = 0;
#endif
}

SRwriter_bil_fseek::~SRwriter_bil_fseek()
{
  // clean-up for SRwriter_bil_fseek interface
#if defined (BUFFER_ROWS) || defined(BUFFER_ROWS_DONT_INITIALIZE_NODATA)
  if (row_buffer) free(row_buffer);
#endif
  // clean-up for SRwriter interface
  if (r_count != -1)
  {
    close(); // user must have forgotten to close the writer
  }
}
