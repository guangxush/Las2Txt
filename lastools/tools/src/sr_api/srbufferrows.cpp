/*
===============================================================================

  FILE:  srbufferrows.cpp
  
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
#include "srbufferrows.h"

#include "srwriter.h"

#include <stdlib.h>
#include <string.h>

#define ROW_MAX 64
#define ROW_MAX_BITS 6

typedef struct RowEntrySC
{
  unsigned short col;
  char value;
} RowEntrySC;

typedef struct RowEntrySS
{
  unsigned short col;
  short value;
} RowEntrySS;

typedef struct RowEntrySI
{
  unsigned short col;
  int value;
} RowEntrySI;

typedef struct RowBufferSC
{
  int last_col;
  int num_entries;
  int written;
  int last_value;
  RowEntrySC entries[ROW_MAX];
} RowBufferSC;

typedef struct RowBufferSS
{
  int last_col;
  int num_entries;
  int written;
  int last_value;
  RowEntrySS entries[ROW_MAX];
} RowBufferSS;

typedef struct RowBufferSI
{
  int last_col;
  int num_entries;
  int written;
  int last_value;
  RowEntrySI entries[ROW_MAX];
} RowBufferSI;

static void quicksort_row_entries(RowEntrySC* a, int i, int j)
{
  int in_i = i, in_j = j;
  int col = a[(i+j)/2].col;
  do
  {
    while ( a[i].col < col ) i++;
    while ( a[j].col > col ) j--;
    if (i<j) { RowEntrySC w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_entries(a, in_i, j);
  if (i<in_j) quicksort_row_entries(a, i, in_j);
}

static void quicksort_row_entries(RowEntrySS* a, int i, int j)
{
  int in_i = i, in_j = j;
  int col = a[(i+j)/2].col;
  do
  {
    while ( a[i].col < col ) i++;
    while ( a[j].col > col ) j--;
    if (i<j) { RowEntrySS w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_entries(a, in_i, j);
  if (i<in_j) quicksort_row_entries(a, i, in_j);
}

static void quicksort_row_entries(RowEntrySI* a, int i, int j)
{
  int in_i = i, in_j = j;
  int col = a[(i+j)/2].col;
  do
  {
    while ( a[i].col < col ) i++;
    while ( a[j].col > col ) j--;
    if (i<j) { RowEntrySI w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_entries(a, in_i, j);
  if (i<in_j) quicksort_row_entries(a, i, in_j);
}

void SRbufferRows::write_bit(int bit)
{
  bits_buffer = (bits_buffer << 1) | bit;
  bits_number--;
  if (!bits_number)
  {
    fwrite(&bits_buffer, 4, 1, file);
    bits_number = 32;
  }
}

void SRbufferRows::write_bits(int nbits, int bits)
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

int SRbufferRows::read_bit()
{
  int bit;
  bits_number--;
  if (bits_number == 0)
  {
    bit = bits_buffer & 1;
    fread(&bits_buffer, 4, 1, file);
    bits_number = 32;
  }
  else
  {
    bit = bits_buffer & (1 << bits_number);
  }
  return bit;
}

int SRbufferRows::read_bits(int nbits)
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

void SRbufferRows::output_value(int last_value, int value)
{
  if (value == last_value)
  {
    // write the numbers are identical
    write_bit(0);
  }
  else
  {
    // write the numbers are different 
    write_bit(1);

    int diff;
    if (value < last_value)
    {
      // write the value is smaller 
      write_bit(1);
      diff = last_value - value;
    }
    else
    {
      // write the value is bigger 
      write_bit(0);
      diff = value - last_value;
    }
    // calculate how many low-order bits are different
    int k = 0;
    int d = diff >> 1;
    while (d)
    {
      k = k + 1;
      d = d >> 1;
    }
    // write how many low-order bits are different
    write_bits(k_bits,k);
    // write those low-order bits that are different
    write_bits(k+1,diff);
  }
}

int SRbufferRows::input_value(int last_value)
{
  if (read_bit())
  {
    int sign = read_bit();
    // read how many low-order bits are different
    int k = read_bits(k_bits);
    // read those bits
    int diff = read_bits(k+1);
    // xor the difference back into the value
    if (sign)
    {
      return last_value - diff;
    }
    else
    {
      return last_value + diff;
    }
  }
  else
  {
    // the numbers are identical
    return last_value;
  }
}

void SRbufferRows::output_row(int row)
{
  if (nbits <= 8)
  {
    RowBufferSC* buffer = ((RowBufferSC*)row_buffer) + row;
    RowEntrySC* entries = buffer->entries;
    // write which row it is
    write_bits(row_bits, row);
    // sort entries in increasing col order
    quicksort_row_entries(entries, 0, ROW_MAX-1);
    // output the row in that order
    int entry = 0;
    int last_value = buffer->last_value;
    while (entry < ROW_MAX)
    {
      // write which col it is
      write_bits(col_bits, entries[entry].col);
      // how many consecutive entries are there
      int next_entry = entry + 1;
      while ((next_entry < ROW_MAX) && (entries[next_entry].col == entries[next_entry-1].col+1))
      {
        next_entry++;
      }
      // write how many additional consecutive entries there are
      write_bits(ROW_MAX_BITS, next_entry-entry-1);
      // write out all consecutive values (at least one, at most ROW_MAX)
      while (entry < next_entry)
      {
        // write difference to last value
        output_value(last_value, entries[entry].value);
        // set last value to current value
        last_value = entries[entry].value;
        // next entry
        entry++;
      }
    }
    buffer->last_value = last_value;
  }
  else if (nbits <= 16)
  {
    RowBufferSS* buffer = ((RowBufferSS*)row_buffer) + row;
    RowEntrySS* entries = buffer->entries;
    // write which row it is
    write_bits(row_bits, row);
    // sort entries in increasing col order
    quicksort_row_entries(entries, 0, ROW_MAX-1);
    // output the row in that order
    int entry = 0;
    int last_value = buffer->last_value;
    while (entry < ROW_MAX)
    {
      // write which col it is
      write_bits(col_bits, entries[entry].col);
      // how many consecutive entries are there
      int next_entry = entry + 1;
      while ((next_entry < ROW_MAX) && (entries[next_entry].col == entries[next_entry-1].col+1))
      {
        next_entry++;
      }
      // write how many additional consecutive entries there are
      write_bits(ROW_MAX_BITS, next_entry-entry-1);
      // write out all consecutive values (at least one, at most ROW_MAX)
      while (entry < next_entry)
      {
        // write difference to last value
        output_value(last_value, entries[entry].value);
        // set last value to current value
        last_value = entries[entry].value;
        // next entry
        entry++;
      }
    }
    buffer->last_value = last_value;
  }
  else
  {
    RowBufferSI* buffer = ((RowBufferSI*)row_buffer) + row;
    RowEntrySI* entries = buffer->entries;
    // write which row it is
    write_bits(row_bits, row);
    // sort entries in increasing col order
    quicksort_row_entries(entries, 0, ROW_MAX-1);
    // output the row in that order
    int entry = 0;
    int last_value = buffer->last_value;
    while (entry < ROW_MAX)
    {
      // write which col it is
      write_bits(col_bits, entries[entry].col);
      // how many consecutive entries are there
      int next_entry = entry + 1;
      while ((next_entry < ROW_MAX) && (entries[next_entry].col == entries[next_entry-1].col+1))
      {
        next_entry++;
      }
      // write how many additional consecutive entries there are
      write_bits(ROW_MAX_BITS, next_entry-entry-1);
      // write out all consecutive values (at least one, at most ROW_MAX)
      while (entry < next_entry)
      {
        // write difference to last value
        output_value(last_value, entries[entry].value);
        // set last value to current value
        last_value = entries[entry].value;
        // next entry
        entry++;
      }
    }
    buffer->last_value = last_value;
  }
}

bool SRbufferRows::prepare(int nrows, int ncols, int nbits)
{
  if (nrows <= 0 || ncols <= 0)
  {
    fprintf(stderr, "ERROR: nrows = %d and ncols = %d not supported by SRbufferRows\n", nrows, ncols);
    return false;
  }
  if (nbits <= 0 || nbits > 32)
  {
    fprintf(stderr, "ERROR: nbits = %d not supported by SRbufferRows\n", nbits);
    return false;
  }

  file = fopen(file_name, "wb");

  if (file == 0)
  {
    fprintf(stderr, "ERROR: cannot open file '%s' for write in SRbufferRows\n", file_name);
    return false;
  }

  this->nrows = nrows;
  this->ncols = ncols;
  this->nbits = nbits;

  // create the bit buffer 
  bits_buffer = 0;
  bits_number = 32;

  // create the row buffers
  if (nbits <= 8)
  {
    row_buffer = malloc(sizeof(RowBufferSC)*nrows);
    for (int row = 0; row < nrows; row++)
    {
      ((RowBufferSC*)row_buffer)[row].last_value = 0;
      ((RowBufferSC*)row_buffer)[row].last_col = 0;
      ((RowBufferSC*)row_buffer)[row].num_entries = 0;
      ((RowBufferSC*)row_buffer)[row].written = 0;
    }
  }
  else if (nbits <= 16)
  {
    row_buffer = malloc(sizeof(RowBufferSS)*nrows);
    for (int row = 0; row < nrows; row++)
    {
      ((RowBufferSS*)row_buffer)[row].last_value = 0;
      ((RowBufferSS*)row_buffer)[row].last_col = 0;
      ((RowBufferSS*)row_buffer)[row].num_entries = 0;
      ((RowBufferSS*)row_buffer)[row].written = 0;
    }
  }
  else
  {
    row_buffer = malloc(sizeof(RowBufferSI)*nrows);
    for (int row = 0; row < nrows; row++)
    {
      ((RowBufferSI*)row_buffer)[row].last_value = 0;
      ((RowBufferSI*)row_buffer)[row].last_col = 0;
      ((RowBufferSI*)row_buffer)[row].num_entries = 0;
      ((RowBufferSI*)row_buffer)[row].written = 0;
    }
  }

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

  // compute the number of bits we need to store the number k of diff bits
  this->k_bits = 0;
  nbits = nbits - 1;
  while (nbits)
  {
    this->k_bits++;
    nbits = nbits >> 1;
  }

  r_count = 0;
  r_clipped = 0;
  r_duplicate = 0;

  return true;
}

void SRbufferRows::write_raster(int row, int col, int value)
{
  if (row < 0 || col < 0 || row >= nrows || col >= ncols)
  {
    r_clipped++;
    return;
  }
  if (nbits <= 8)
  {
    RowBufferSC* buffer = ((RowBufferSC*)row_buffer) + row;
    buffer->entries[buffer->num_entries].col = col;
    buffer->entries[buffer->num_entries].value = value;
    buffer->num_entries++;
    if (buffer->num_entries == ROW_MAX)
    {
      // output a full row
      output_row(row);
      // empty row
      buffer->num_entries = 0;
      // increment written buffer count
      buffer->written++;
    }
  }
  else if (nbits <= 16)
  {
    RowBufferSS* buffer = ((RowBufferSS*)row_buffer) + row;
    buffer->entries[buffer->num_entries].col = col;
    buffer->entries[buffer->num_entries].value = value;
    buffer->num_entries++;
    if (buffer->num_entries == ROW_MAX)
    {
      // output a full row
      output_row(row);
      // empty row
      buffer->num_entries = 0;
      // increment written buffer count
      buffer->written++;
    }
  }
  else
  {
    RowBufferSI* buffer = ((RowBufferSI*)row_buffer) + row;
    buffer->entries[buffer->num_entries].col = col;
    buffer->entries[buffer->num_entries].value = value;
    buffer->num_entries++;
    if (buffer->num_entries == ROW_MAX)
    {
      // output a full row
      output_row(row);
      // empty row
      buffer->num_entries = 0;
      // increment written buffer count
      buffer->written++;
    }
  }
  r_count++;
}

int SRbufferRows::required_sort_buffer_size() const
{
  if (nbits <= 8)
  {
    return r_count*sizeof(RowEntrySC); 
  }
  else if (nbits <= 16)
  {
    return r_count*sizeof(RowEntrySS); 
  }
  else
  {
    return r_count*sizeof(RowEntrySI); 
  }
}

void SRbufferRows::sort_and_output(void* sort_buffer, SRwriter* srwriter)
{
  int b,row,col;
  // output any remaining bits
  if (bits_number < 32)
  {
    bits_buffer = (bits_buffer << bits_number);
    fwrite(&bits_buffer, 4, 1, file);
  }
  // close the temporary file
  fclose(file);
  // are there any rasters
  if (r_count)
  {
    // re-open the temporary file for read
    file = fopen(file_name, "rb");
    if (file == 0)
    {
      fprintf(stderr, "ERROR: cannot re-open file '%s' for read in SRbufferRows\n", file_name);
      exit(0);
    }
    // read the first 32 bits
    fread(&bits_buffer, 4, 1, file);
    bits_number = 32;
    // calculate the number of total entries and written buffers
    int total_entries = 0;
    int written_buffers = 0;
    int* start_entries = new int[nrows+1];
    // switch depending on struct size
    if (nbits <= 8)
    {
      RowBufferSC* row_buffer = (RowBufferSC*)(this->row_buffer);
      for (row = 0; row < nrows; row++)
      {
        start_entries[row] = total_entries;
        written_buffers += row_buffer[row].written;
        total_entries += row_buffer[row].written * ROW_MAX + row_buffer[row].num_entries;
        // also set last value back to zero
        row_buffer[row].last_value = 0;
        row_buffer[row].last_col = 0;
      }
      start_entries[nrows] = total_entries;
      // read entries to the right place in the array
      RowEntrySC* buffer = (RowEntrySC*)sort_buffer;
      // are there any written buffers
      if (written_buffers)
      {
        // entries are read row_buffer by row_buffer from the file
        for (b = 0; b < written_buffers; b++)
        {
          // read which row it is
          row = read_bits(row_bits);
          // output the row in that order
          int next_entry, entry = 0;
          int last_value = row_buffer[row].last_value;
          while (entry < ROW_MAX)
          {
            // write which col it is
            col = read_bits(col_bits);
            // how many consecutive entries are there
            next_entry = read_bits(ROW_MAX_BITS) + entry + 1;
            // read all consecutive entries
            while (entry < next_entry)
            {
              buffer[start_entries[row]].col = col;
              buffer[start_entries[row]].value = input_value(last_value);
              last_value = buffer[start_entries[row]].value;
              start_entries[row]++;
              col++;
              entry++;
            }
          }
          row_buffer[row].last_value = last_value;
        }
        //close the file for good
        fclose(file);
      }
      // add whatever was never written to a file
      for (row = 0; row < nrows; row++)
      {
        for (b = 0; b < row_buffer[row].num_entries; b++)
        {
          buffer[start_entries[row]].col = row_buffer[row].entries[b].col;
          buffer[start_entries[row]].value = row_buffer[row].entries[b].value;
          start_entries[row]++;
        }
      }
      // sort and output the values in each row
      total_entries = 0;
      for (row = 0; row < nrows; row++)
      {
        // do we have any entries for this row
        if (total_entries < start_entries[row])
        {
          // sort row entries by col
          quicksort_row_entries(&(buffer[total_entries]), 0,  start_entries[row]-total_entries-1);
          for (col = 0; col < ncols; col++)
          {
            if (col == buffer[total_entries].col  && total_entries < start_entries[row])
            {
              srwriter->write_raster(buffer[total_entries].value);
              total_entries++;
              while (col == buffer[total_entries].col && total_entries < start_entries[row])
              {
//              fprintf(stderr, "%d,%d DUPLICATE RASTER: r %d c %d %d %d\n", total_entries, start_entries[row], row, col, buffer[total_entries].value, buffer[total_entries-1].value);
                r_duplicate++;
                total_entries++;
              }
            }
            else
            {
              srwriter->write_nodata();
            }
          }
        }
        else
        {
          for (col = 0; col < ncols; col++)
          {
            srwriter->write_nodata();
          }
        }
      }
    }
    else if (nbits <= 16)
    {
      RowBufferSS* row_buffer = (RowBufferSS*)(this->row_buffer);
      for (row = 0; row < nrows; row++)
      {
        start_entries[row] = total_entries;
        written_buffers += row_buffer[row].written;
        total_entries += row_buffer[row].written * ROW_MAX + row_buffer[row].num_entries;
        // also set last value back to zero
        row_buffer[row].last_value = 0;
        row_buffer[row].last_col = 0;
      }
      start_entries[nrows] = total_entries;
      // read entries to the right place in the array
      RowEntrySS* buffer = (RowEntrySS*)sort_buffer;
      // are there any written buffers
      if (written_buffers)
      {
        // entries are read row_buffer by row_buffer from the file
        for (b = 0; b < written_buffers; b++)
        {
          // read which row it is
          row = read_bits(row_bits);
          // output the row in that order
          int next_entry, entry = 0;
          int last_value = row_buffer[row].last_value;
          while (entry < ROW_MAX)
          {
            // write which col it is
            col = read_bits(col_bits);
            // how many consecutive entries are there
            next_entry = read_bits(ROW_MAX_BITS) + entry + 1;
            // read all consecutive entries
            while (entry < next_entry)
            {
              buffer[start_entries[row]].col = col;
              buffer[start_entries[row]].value = input_value(last_value);
              last_value = buffer[start_entries[row]].value;
              start_entries[row]++;
              col++;
              entry++;
            }
          }
          row_buffer[row].last_value = last_value;
        }
        //close the file for good
        fclose(file);
      }
      // add whatever was never written to a file
      for (row = 0; row < nrows; row++)
      {
        for (b = 0; b < row_buffer[row].num_entries; b++)
        {
          buffer[start_entries[row]].col = row_buffer[row].entries[b].col;
          buffer[start_entries[row]].value = row_buffer[row].entries[b].value;
          start_entries[row]++;
        }
      }
      // sort and output the values in each row
      total_entries = 0;
      for (row = 0; row < nrows; row++)
      {
        // do we have any entries for this row
        if (total_entries < start_entries[row])
        {
          // sort row entries by col
          quicksort_row_entries(&(buffer[total_entries]), 0,  start_entries[row]-total_entries-1);
          for (col = 0; col < ncols; col++)
          {
            if (col == buffer[total_entries].col  && total_entries < start_entries[row])
            {
              srwriter->write_raster(buffer[total_entries].value);
              total_entries++;
              while (col == buffer[total_entries].col && total_entries < start_entries[row])
              {
//              fprintf(stderr, "%d,%d DUPLICATE RASTER: r %d c %d %d %d\n", total_entries, start_entries[row], row, col, buffer[total_entries].value, buffer[total_entries-1].value);
                r_duplicate++;
                total_entries++;
              }
            }
            else
            {
              srwriter->write_nodata();
            }
          }
        }
        else
        {
          for (col = 0; col < ncols; col++)
          {
            srwriter->write_nodata();
          }
        }
      }
    }
    else
    {
      RowBufferSI* row_buffer = (RowBufferSI*)(this->row_buffer);
      for (row = 0; row < nrows; row++)
      {
        start_entries[row] = total_entries;
        written_buffers += row_buffer[row].written;
        total_entries += row_buffer[row].written * ROW_MAX + row_buffer[row].num_entries;
        // also set last value back to zero
        row_buffer[row].last_value = 0;
        row_buffer[row].last_col = 0;
      }
      start_entries[nrows] = total_entries;
      // read entries to the right place in the array
      RowEntrySI* buffer = (RowEntrySI*)sort_buffer;
      // are there any written buffers
      if (written_buffers)
      {
        // entries are read row_buffer by row_buffer from the file
        for (b = 0; b < written_buffers; b++)
        {
          // read which row it is
          row = read_bits(row_bits);
          // output the row in that order
          int next_entry, entry = 0;
          int last_value = row_buffer[row].last_value;
          while (entry < ROW_MAX)
          {
            // write which col it is
            col = read_bits(col_bits);
            // how many consecutive entries are there
            next_entry = read_bits(ROW_MAX_BITS) + entry + 1;
            // read all consecutive entries
            while (entry < next_entry)
            {
              buffer[start_entries[row]].col = col;
              buffer[start_entries[row]].value = input_value(last_value);
              last_value = buffer[start_entries[row]].value;
              start_entries[row]++;
              col++;
              entry++;
            }
          }
          row_buffer[row].last_value = last_value;
        }
        //close the file for good
        fclose(file);
      }
      // add whatever was never written to a file
      for (row = 0; row < nrows; row++)
      {
        for (b = 0; b < row_buffer[row].num_entries; b++)
        {
          buffer[start_entries[row]].col = row_buffer[row].entries[b].col;
          buffer[start_entries[row]].value = row_buffer[row].entries[b].value;
          start_entries[row]++;
        }
      }
      // sort and output the values in each row
      total_entries = 0;
      for (row = 0; row < nrows; row++)
      {
        // do we have any entries for this row
        if (total_entries < start_entries[row])
        {
          // sort row entries by col
          quicksort_row_entries(&(buffer[total_entries]), 0,  start_entries[row]-total_entries-1);
          for (col = 0; col < ncols; col++)
          {
            if (col == buffer[total_entries].col  && total_entries < start_entries[row])
            {
              srwriter->write_raster(buffer[total_entries].value);
              total_entries++;
              while (col == buffer[total_entries].col && total_entries < start_entries[row])
              {
//              fprintf(stderr, "%d,%d DUPLICATE RASTER: r %d c %d %d %d\n", total_entries, start_entries[row], row, col, buffer[total_entries].value, buffer[total_entries-1].value);
                r_duplicate++;
                total_entries++;
              }
            }
            else
            {
              srwriter->write_nodata();
            }
          }
        }
        else
        {
          for (col = 0; col < ncols; col++)
          {
            srwriter->write_nodata();
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

void SRbufferRows::set_file_name(const char* file_name)
{
  if (file_name == 0)
  {
    fprintf(stderr, "ERROR: file_name = NULL not supported by SRbufferRows\n");
  }
  else
  {
    free (this->file_name);
    this->file_name = strdup(file_name);
  }
}

SRbufferRows::SRbufferRows()
{
  file = 0;
  row_buffer = 0;
  bits_buffer = -1;
  bits_number = -1;
  row_bits = -1;
  col_bits = -1;
  k_bits = -1;
  file_name = strdup("temp.tmp");
}

SRbufferRows::~SRbufferRows()
{
  if (row_buffer) free(row_buffer);
  free(file_name);
}
