/*
===============================================================================

  FILE:  srbuffersimple.cpp
  
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
#include "srbuffersimple.h"

#include "srwriter.h"

#include <stdlib.h>
#include <string.h>

typedef struct EntryCCC
{
  unsigned char row;
  unsigned char col;
  unsigned char value;
} EntryCCC;

typedef struct EntrySCC
{
  unsigned short row;
  unsigned char col;
  unsigned char value;
} EntrySCC;

typedef struct EntryCSC
{
  unsigned char row;
  unsigned short col;
  unsigned char value;
} EntryCSC;

typedef struct EntrySSC
{
  unsigned short row;
  unsigned short col;
  unsigned char value;
} EntrySSC;

typedef struct EntryCCS
{
  unsigned char row;
  unsigned char col;
  short value;
} EntryCCS;

typedef struct EntrySCS
{
  unsigned short row;
  unsigned char col;
  short value;
} EntrySCS;

typedef struct EntryCSS
{
  unsigned char row;
  unsigned short col;
  short value;
} EntryCSS;

typedef struct EntrySSS
{
  unsigned short row;
  unsigned short col;
  short value;
} EntrySSS;

typedef struct EntryCCI
{
  unsigned char row;
  unsigned char col;
  int value;
} EntryCCI;

typedef struct EntrySCI
{
  unsigned short row;
  unsigned char col;
  int value;
} EntrySCI;

typedef struct EntryCSI
{
  unsigned char row;
  unsigned short col;
  int value;
} EntryCSI;

typedef struct EntrySSI
{
  unsigned short row;
  unsigned short col;
  int value;
} EntrySSI;

static void quicksort_row_col(EntryCCC* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntryCCC w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntrySCC* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntrySCC w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntryCSC* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntryCSC w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntrySSC* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntrySSC w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntryCCS* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntryCCS w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntrySCS* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntrySCS w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntryCSS* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntryCSS w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntrySSS* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntrySSS w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntryCCI* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntryCCI w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntrySCI* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntrySCI w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntryCSI* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntryCSI w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_row_col(EntrySSI* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row, col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { EntrySSI w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

void SRbufferSimple::write_bits(int nbits, int bits)
{
  if (bits_number >= nbits)
  {
    bits_buffer = (bits_buffer << nbits) | bits;
    bits_number = bits_number - nbits;
    if (bits_number == 0)
    {
      fwrite(&bits_buffer, 4, 1, file);
      bits_number = 32;
    }
  }
  else
  {
    int bits_remaining = nbits-bits_number;
    bits_buffer = (bits_buffer << bits_number) | (bits >> bits_remaining);
    fwrite(&bits_buffer, 4, 1, file);
    bits_buffer = bits;
    bits_number = 32 - bits_remaining;
  }
}

int SRbufferSimple::read_bits(int nbits)
{
  int bits;
  if (bits_number >= nbits)
  {
    bits_number = bits_number - nbits;
    if (bits_number == 0)
    {
      bits = bits_buffer & ((1 << nbits) - 1);
      fread(&bits_buffer, 4, 1, file);
      bits_number = 32;
    }
    else
    {
      bits = (bits_buffer >> bits_number) & ((1 << nbits) - 1);
    }
  }
  else
  {
    int bits_remaining = nbits-bits_number;
    bits = bits_buffer & ((1 << bits_number) - 1);
    fread(&bits_buffer, 4, 1, file);
    bits_number = 32 - bits_remaining;
    bits = (bits << bits_remaining) | (bits_buffer >> bits_number);
  }
  return bits;
}

bool SRbufferSimple::prepare(int nrows, int ncols, int nbits)
{
  if (nrows <= 0 || ncols <= 0 || nrows > 65536 || ncols > 65536)
  {
    fprintf(stderr, "ERROR: nrows = %d and ncols = %d not supported by SRbufferSimple\n", nrows, ncols);
    return false;
  }
  if (nbits <= 0 || nbits > 32)
  {
    fprintf(stderr, "ERROR: nbits = %d not supported by SRbufferSimple\n", nbits);
    return false;
  }

  file = fopen(file_name, "wb");

  if (file == 0)
  {
    fprintf(stderr, "ERROR: cannot open file '%s' for write in SRbufferSimple\n", file_name);
    return false;
  }

  this->nrows = nrows;
  this->ncols = ncols;
  this->nbits = nbits;

  bits_buffer = 0;
  bits_number = 32;
    
  // compute the number of bits we need to store a row index
  this->row_bits = 0;
  nrows = nrows - 1;
  while (nrows)
  {
    this->row_bits++;
    nrows = nrows >> 1;
  }

  // compute the number of bits we need to store a col index
  this->col_bits = 0;
  ncols = ncols - 1;
  while (ncols)
  {
    this->col_bits++;
    ncols = ncols >> 1;
  }

  r_count = 0;
  r_clipped = 0;
  r_duplicate = 0;

  return true;
}

void SRbufferSimple::write_raster(int row, int col, int value)
{
  if (row < 0 || col < 0 || row >= nrows || col >= ncols)
  {
    r_clipped++;
    return;
  }
  write_bits(row_bits, row);
  write_bits(col_bits, col);
  write_bits(nbits, value);
  r_count++;
}

int SRbufferSimple::required_sort_buffer_size() const
{
  if (row_bits <= 8)
  {
    if (col_bits <= 8)
    {
      if (nbits <= 8)
      {
        return r_count*sizeof(EntryCCC); 
      }
      else if (nbits <= 16)
      {
        return r_count*sizeof(EntryCCS); 
      }
      else
      {
        return r_count*sizeof(EntryCCI); 
      }
    }
    else
    {
      if (nbits <= 8)
      {
        return r_count*sizeof(EntryCSC); 
      }
      else if (nbits <= 16)
      {
        return r_count*sizeof(EntryCSS); 
      }
      else
      {
        return r_count*sizeof(EntryCSI); 
      }
    }
  }
  else
  {
    if (col_bits <= 8)
    {
      if (nbits <= 8)
      {
        return r_count*sizeof(EntrySCC); 
      }
      else if (nbits <= 16)
      {
        return r_count*sizeof(EntrySCS); 
      }
      else
      {
        return r_count*sizeof(EntrySCI); 
      }
    }
    else
    {
      if (nbits <= 8)
      {
        return r_count*sizeof(EntrySSC); 
      }
      else if (nbits <= 16)
      {
        return r_count*sizeof(EntrySSS); 
      }
      else
      {
        return r_count*sizeof(EntrySSI); 
      }
    }
  }
}

void SRbufferSimple::sort_and_output(void* sort_buffer, SRwriter* srwriter)
{
  int r, row, col;
  // close the temporary file
  fclose(file);
  // are there any rasters
  if (r_count)
  {
    // re-open the temporary file for read
    file = fopen(file_name, "rb");
    if (file == 0)
    {
      fprintf(stderr, "ERROR: cannot re-open file '%s' for read in SRbufferSimple\n", file_name);
      exit(0);
    }
    // read the first 32 bits
    fread(&bits_buffer, 4, 1, file);
    bits_number = 32;
    // process the written rasters but switch depending on struct size
    if (row_bits <= 8)
    {
      if (col_bits <= 8)
      {
        if (nbits <= 8)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntryCCC*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntryCCC*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntryCCC*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntryCCC*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntryCCC*)sort_buffer)[r].row == row && ((EntryCCC*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntryCCC*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntryCCC*)sort_buffer)[r].row == row && ((EntryCCC*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else if (nbits <= 16)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntryCCS*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntryCCS*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntryCCS*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntryCCS*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntryCCS*)sort_buffer)[r].row == row && ((EntryCCS*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntryCCS*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntryCCS*)sort_buffer)[r].row == row && ((EntryCCS*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntryCCI*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntryCCI*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntryCCI*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntryCCI*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntryCCI*)sort_buffer)[r].row == row && ((EntryCCI*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntryCCI*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntryCCI*)sort_buffer)[r].row == row && ((EntryCCI*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
      }
      else
      {
        if (nbits <= 8)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntryCSC*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntryCSC*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntryCSC*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntryCSC*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntryCSC*)sort_buffer)[r].row == row && ((EntryCSC*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntryCSC*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntryCSC*)sort_buffer)[r].row == row && ((EntryCSC*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else if (nbits <= 16)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntryCSS*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntryCSS*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntryCSS*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntryCSS*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntryCSS*)sort_buffer)[r].row == row && ((EntryCSS*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntryCSS*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntryCSS*)sort_buffer)[r].row == row && ((EntryCSS*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntryCSI*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntryCSI*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntryCSI*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntryCSI*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntryCSI*)sort_buffer)[r].row == row && ((EntryCSI*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntryCSI*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntryCSI*)sort_buffer)[r].row == row && ((EntryCSI*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
      }
    }
    else
    {
      if (col_bits <= 8)
      {
        if (nbits <= 8)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntrySCC*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntrySCC*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntrySCC*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntrySCC*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntrySCC*)sort_buffer)[r].row == row && ((EntrySCC*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntrySCC*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntrySCC*)sort_buffer)[r].row == row && ((EntrySCC*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else if (nbits <= 16)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntrySCS*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntrySCS*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntrySCS*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntrySCS*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntrySCS*)sort_buffer)[r].row == row && ((EntrySCS*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntrySCS*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntrySCS*)sort_buffer)[r].row == row && ((EntrySCS*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntrySCI*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntrySCI*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntrySCI*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntrySCI*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntrySCI*)sort_buffer)[r].row == row && ((EntrySCI*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntrySCI*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntrySCI*)sort_buffer)[r].row == row && ((EntrySCI*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
      }
      else
      {
        if (nbits <= 8)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntrySSC*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntrySSC*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntrySSC*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntrySSC*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntrySSC*)sort_buffer)[r].row == row && ((EntrySSC*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntrySSC*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntrySSC*)sort_buffer)[r].row == row && ((EntrySSC*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else if (nbits <= 16)
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntrySSS*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntrySSS*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntrySSS*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntrySSS*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntrySSS*)sort_buffer)[r].row == row && ((EntrySSS*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntrySSS*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntrySSS*)sort_buffer)[r].row == row && ((EntrySSS*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
        else
        {
          for (r = 0; r < r_count; r++)
          {
            ((EntrySSI*)sort_buffer)[r].row = read_bits(row_bits);
            ((EntrySSI*)sort_buffer)[r].col = read_bits(col_bits);
            ((EntrySSI*)sort_buffer)[r].value = read_bits(nbits);
          }
          quicksort_row_col((EntrySSI*)sort_buffer, 0, r_count-1);
          r = 0;
          for (row = 0; row < nrows; row++)
          {
            for (col = 0; col < ncols; col++)
            {
              if (r < r_count && ((EntrySSI*)sort_buffer)[r].row == row && ((EntrySSI*)sort_buffer)[r].col == col)
              {
                srwriter->write_raster(((EntrySSI*)sort_buffer)[r].value);
                r++;
                while (r < r_count && ((EntrySSI*)sort_buffer)[r].row == row && ((EntrySSI*)sort_buffer)[r].col == col)
                {
                  fprintf(stderr, "DUPLICATE RASTER: r %d c %d (%d %d %d)\n", row, col, r, r_count, srwriter->r_count);
                  r_duplicate++;
                  r++;
                }
              }
              else
              {
                srwriter->write_nodata();
              }
            }
          }
        }
      }
    }
    // close the temporary file
    fclose(file);
  }
  else
  {
    // output only nodata rows
    for (row = 0; row < nrows; row++)
    {
      for (col = 0; col < ncols; col++)
      {
        srwriter->write_nodata();
      }
    }
  }
  // delete the temporary files
  remove(file_name);
}


void SRbufferSimple::set_file_name(const char* file_name)
{
  if (file_name == 0)
  {
    fprintf(stderr, "ERROR: file_name = NULL not supported by SRbufferSimple\n");
  }
  else
  {
    free (this->file_name);
    this->file_name = strdup(file_name);
  }
}

SRbufferSimple::SRbufferSimple()
{
  file = 0;
  bits_buffer = -1;
  bits_number = -1;
  row_bits = -1;
  col_bits = -1;
  file_name = strdup("temp.tmp");
}

SRbufferSimple::~SRbufferSimple()
{
  free(file_name);
}
