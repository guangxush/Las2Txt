/*
===============================================================================

  FILE:  srbuffertiles.cpp
  
  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2007  martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "srbuffertiles.h"

#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

#define NO_BIL_FILE_OUTPUT
#undef NO_BIL_FILE_OUTPUT

typedef struct Raster
{
  unsigned short row;
  unsigned short col;
  int value;
} Raster;

static void quicksort_row_col(Raster* a, int i, int j)
{
  int in_i = i, in_j = j;
  int row = a[(i+j)/2].row;
  int col = a[(i+j)/2].col;
  do
  {
    while ( a[i].row < row || (a[i].row == row && a[i].col < col) ) i++;
    while ( a[j].row > row || (a[j].row == row && a[j].col > col) ) j--;
    if (i<j) { Raster w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_row_col(a, in_i, j);
  if (i<in_j) quicksort_row_col(a, i, in_j);
}

static void quicksort_col_row(Raster* a, int i, int j)
{
  int in_i = i, in_j = j;
  int col = a[(i+j)/2].col;
  int row = a[(i+j)/2].row;
  do
  {
    while ( a[i].col < col || (a[i].col == col && a[i].row < row) ) i++;
    while ( a[j].col > col || (a[j].col == col && a[j].row > row) ) j--;
    if (i<j) { Raster w = a[i]; a[i] = a[j]; a[j] = w; }
  } while (++i<=--j);
  if (i == j+3) { i--; j++; }
  if (j>in_i) quicksort_col_row(a, in_i, j);
  if (i<in_j) quicksort_col_row(a, i, in_j);
}

void SRbufferTiles::write_bit(int bit)
{
  bits_buffer = (bits_buffer << 1) | bit;
  bits_number--;
  if (!bits_number)
  {
    fwrite(&bits_buffer, 4, 1, file);
    bits_number = 32;
  }
}

void SRbufferTiles::write_bits(int nbits, unsigned int bits)
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

void SRbufferTiles::write_range(unsigned int range, unsigned int bits)
{
  int nbits = 0;
  while (range > 256)
  {
    nbits += 8;
    range = range >> 8;
  }
  while (range > 64)
  {
    nbits += 4;
    range = range >> 4;
  }
  while (range > 4)
  {
    nbits += 2;
    range = range >> 2;
  }
  while (range)
  {
    nbits += 1;
    range = range >> 1;
  }
  write_bits(nbits, bits);
}

int SRbufferTiles::read_bit()
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

int SRbufferTiles::read_bits(int nbits)
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

int SRbufferTiles::read_range(unsigned int range)
{
  int nbits = 0;
  while (range > 256)
  {
    nbits += 8;
    range = range >> 8;
  }
  while (range > 64)
  {
    nbits += 4;
    range = range >> 4;
  }
  while (range > 4)
  {
    nbits += 2;
    range = range >> 2;
  }
  while (range)
  {
    nbits += 1;
    range = range >> 1;
  }
  return read_bits(nbits);
}

void SRbufferTiles::output_value(unsigned int last_value, unsigned int value)
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

    unsigned int diff;
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
    unsigned int d = diff >> 1;
    while (d)
    {
      k = k + 1;
      d = d >> 1;
    }
    // write how many low-order bits are different // could be 3 (nbits = 8), 4 (nbits = 16), 5 (nbits = 32)
    write_bits(4,k);
    // write those low-order bits that are different
    write_bits(k+1,diff);
  }
}

int SRbufferTiles::input_value(int last_value)
{
  if (read_bit())
  {
    int sign = read_bit();
    // read how many low-order bits are different
    int k = read_bits(4);
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

int SRbufferTiles::output_row(int entry, int row)
{
  // output the row in that order
  int last_value = 0;
  int last_col = 0;
  while (((Raster*)buffer)[entry].row == row)
  {
    // write which col it is
    write_range(ncols-last_col, ((Raster*)buffer)[entry].col-last_col);
    last_col = ((Raster*)buffer)[entry].col;
    // how many consecutive entries are there
    int next_entry = entry + 1;
    while (((Raster*)buffer)[next_entry].row == row && (((Raster*)buffer)[next_entry].col == ((Raster*)buffer)[next_entry-1].col+1))
    {
      next_entry++;
    }
    // write how many additional consecutive entries there are
    write_range(ncols-last_col, ((Raster*)buffer)[next_entry-1].col);
    last_col = ((Raster*)buffer)[next_entry-1].col;
    // write out all consecutive values
    while (entry < next_entry)
    {
      // write difference to last value
      output_value(last_value, ((Raster*)buffer)[entry].value);
      // set last value to current value
      last_value = ((Raster*)buffer)[entry].value;
      // next entry
      entry++;
    }
  }
  return entry;
}

int SRbufferTiles::output_col(int entry, int col)
{
  // output the col in that order
  int last_value = 0;
  int last_row = 0;
  while (((Raster*)buffer)[entry].col == col)
  {
    // write which row it is
    write_range(nrows-last_row, ((Raster*)buffer)[entry].row-last_row);
    last_row = ((Raster*)buffer)[entry].row;
    // how many consecutive entries are there
    int next_entry = entry + 1;
    while (((Raster*)buffer)[next_entry].col == col && (((Raster*)buffer)[next_entry].row == ((Raster*)buffer)[next_entry-1].row+1))
    {
      next_entry++;
    }
    // write how many additional consecutive entries there are
    write_range(nrows-last_row, ((Raster*)buffer)[next_entry-1].row);
    last_row = ((Raster*)buffer)[next_entry-1].row;
    // write out all consecutive values
    while (entry < next_entry)
    {
      // write difference to last value
      output_value(last_value, ((Raster*)buffer)[entry].value);
      // set last value to current value
      last_value = ((Raster*)buffer)[entry].value;
      // next entry
      entry++;
    }
  }
  return entry;
}

void SRbufferTiles::output_buffer()
{
  int i;
  // decide whether to write by row or by column
  int occupied_rows = 0;
  for (i = 0; i <= (nrows/32); i++)
  {
    while (row_occupancy[i])
    {
      if (row_occupancy[i] & 1) occupied_rows++;
      row_occupancy[i] = row_occupancy[i] >> 1;
    }
  }
  int occupied_cols = 0;
  for (i = 0; i <= (ncols/32); i++)
  {
    while (col_occupancy[i])
    {
      if (col_occupancy[i] & 1) occupied_cols++;
      col_occupancy[i] = col_occupancy[i] >> 1;
    }
  }

  if (occupied_rows < occupied_cols)
  {
    // write that we process the buffer row by row
    write_bits(1, 0);
    // sort the buffer first by row, then by col
    quicksort_row_col((Raster*)buffer, 0, BUFFER_SIZE-1);
    // write rows
    int entry = 0;
    int curr_row = 0;
    int last_row = 0;
    do
    {
      last_row = curr_row;
      curr_row = ((Raster*)buffer)[entry].row;
      // write which row it is
      write_range(nrows-last_row, curr_row-last_row);
      // write row
      entry = output_row(entry, curr_row);
    }
    while (entry < BUFFER_SIZE);
  }
  else
  {
    // write that we process the buffer col by col
    write_bits(1, 1);
    // sort the buffer first by col, then by row
    quicksort_col_row((Raster*)buffer, 0, BUFFER_SIZE-1);
    // write rows
    int entry = 0;
    int curr_col = 0;
    int last_col = 0;
    do
    {
      last_col = curr_col;
      curr_col = ((Raster*)buffer)[entry].col;
      // write which col it is
      write_range(ncols-last_col, curr_col-last_col);
      // write col
      entry = output_col(entry, curr_col);
    }
    while (entry < BUFFER_SIZE);
  }
}

bool SRbufferTiles::prepare(int nrows, int ncols, int nbits)
{
  if (nrows <= 0 || ncols <= 0 || nrows > 65534 || ncols > 65534)
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

  // create the tile buffer
  buffer_entries = 0;
  buffer_written = 0;
  buffer = malloc(sizeof(Raster)*(BUFFER_SIZE+1));
  ((Raster*)buffer)[BUFFER_SIZE].row = 65535;
  ((Raster*)buffer)[BUFFER_SIZE].col = 65535;

  // create the row versus col occupancy bitfield
  int i;
  row_occupancy = (unsigned int*)malloc((nrows/32)+1);
  for (i = 0; i <= (nrows/32); i++) row_occupancy[i] = 0;
  col_occupancy = (unsigned int*)malloc((ncols/32)+1);
  for (i = 0; i <= (ncols/32); i++) col_occupancy[i] = 0;

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

void SRbufferTiles::write_raster(int row, int col, int value)
{
  if (row < 0 || col < 0 || row >= nrows || col >= ncols)
  {
    r_clipped++;
    return;
  }
  ((Raster*)buffer)[buffer_entries].row = row;
  ((Raster*)buffer)[buffer_entries].col = col;
  ((Raster*)buffer)[buffer_entries].value = value;
  buffer_entries++;
  row_occupancy[row/32] |= (1<<(row%32));
  col_occupancy[col/32] |= (1<<(col%32));
  if (buffer_entries == BUFFER_SIZE)
  {
    // output the buffer
    output_buffer();
    // empty row
    buffer_entries = 0;
    // increment written buffer count
    buffer_written++;
  }
}

int SRbufferTiles::required_sort_buffer_size() const
{
  if (r_count <= nrows*ncols/8)
  {
//    fprintf(stderr, "mem %d %d %d 8\n",sizeof(Raster),nrows,ncols);
    return (sizeof(Raster)*(nrows*ncols/8));
  }
  else if (r_count <= nrows*ncols/4)
  {
//    fprintf(stderr, "mem %d %d %d 4\n",sizeof(Raster),nrows,ncols);
    return (sizeof(Raster)*(nrows*ncols/4));
  }
  else if (r_count <= nrows*ncols/2)
  {
//    fprintf(stderr, "mem %d %d %d 2\n",sizeof(Raster),nrows,ncols);
    return (sizeof(Raster)*(nrows*ncols/2));
  }
  else if (r_count <= nrows*ncols)
  {
//    fprintf(stderr, "mem %d %d %d \n",sizeof(Raster),nrows,ncols);
    return (sizeof(Raster)*(nrows*ncols));
  }
  else
  {
    return (sizeof(Raster)*(r_count));
  }
}

void SRbufferTiles::sort_and_output(void* sort_buffer, SRwriter* srwriter)
{
/*
  int i;
  // output any remaining bits
  if (bits_number < 32)
  {
    bits_buffer = (bits_buffer << bits_number);
    fwrite(&bits_buffer, 4, 1, file);
  }
  // close the temporary file
  fclose(file);
  // calculate the number of total entries and written buffers
  int total_entries = 0;
  int written_buffers = 0;
  int* start_entries = new int[nrows+1];
  RowBuffer* row_buffer = (RowBuffer*)(this->row_buffer);
  for (i = 0; i < nrows; i++)
  {
    start_entries[i] = total_entries;
    written_buffers += row_((Raster*)buffer)[i].written;
    total_entries += row_((Raster*)buffer)[i].written * ROW_MAX + row_((Raster*)buffer)[i].num_entries;
    // also set last value back to zero
    row_((Raster*)buffer)[i].last_value = 0;
    row_((Raster*)buffer)[i].last_col = 0;
  }
  start_entries[nrows] = total_entries;
  // are there any entries?
  if (total_entries)
  {
    // read entries to the right place in the array
    Raster* buffer = (Raster*)sort_buffer;
    // are there any written buffers
    if (written_buffers)
    {
      // recreate the file name
      char temp[256];
      sprintf(temp, "%s_%04d.tmp", file_name_base, file_name_num);
      // re-open the file for read
      file = fopen(temp, "rb");
      if (file == 0)
      {
        fprintf(stderr, "ERROR: cannot re-open file '%s' for read in SRbufferTiles\n", temp);
        exit(0);
      }
      // read the first 32 bits
      fread(&bits_buffer, 4, 1, file);
      bits_number = 32;
      // entries are read row_buffer by row_buffer from the file
      for (i = 0; i < written_buffers; i++)
      {
        // read which row it is
        int row = read_bits(row_bits);
        // output the row in that order
        int entry = 0;
        int last_value = row_((Raster*)buffer)[row].last_value;
        while (entry < ROW_MAX)
        {
          // write which col it is
          int col = read_bits(col_bits);
          // how many consecutive entries are there
          int next_entry = read_bits(ROW_MAX_BITS) + entry + 1;
          // read all consecutive entries
          while (entry < next_entry)
          {
            ((Raster*)buffer)[start_entries[row]].col = col;
            last_value = input_value(last_value);
            ((Raster*)buffer)[start_entries[row]].value = last_value;
            start_entries[row]++;
            col++;
            entry++;
          }
        }
        row_((Raster*)buffer)[row].last_value = last_value;
      }
      //close the file for good
      fclose(file);
    }
    // add whatever was never written to a file
    for (i = 0; i < nrows; i++)
    {
      for (int j = 0; j < row_((Raster*)buffer)[i].num_entries; j++)
      {
        ((Raster*)buffer)[start_entries[i]].col = row_((Raster*)buffer)[i].entries[j].col;
        ((Raster*)buffer)[start_entries[i]].value = row_((Raster*)buffer)[i].entries[j].value;
        start_entries[i]++;
      }
    }

    // sort and output the values in each row
    short no_data = no_data_value;

    total_entries = 0;

    for (i = 0; i < nrows; i++)
    {
      // do we have any entries for this row
      if (total_entries < start_entries[i])
      {
        // sort row entries by col
        quicksort_row_entries(&(((Raster*)buffer)[total_entries]), 0,  start_entries[i]-total_entries-1);
        for (int col = 0; col < ncols; col++)
        {
          if (col == ((Raster*)buffer)[total_entries].col  && total_entries < start_entries[i])
          {

#ifndef NO_BIL_FILE_OUTPUT
            fwrite(&(((Raster*)buffer)[total_entries].value), 2, 1, raster_file);
#endif

            total_entries++;

            while (col == ((Raster*)buffer)[total_entries].col && total_entries < start_entries[i])
            {
//              fprintf(stderr, "%d,%d DUPLICATE RASTER: r %d c %d %d %d\n", total_entries, start_entries[i], row_band+i, col, ((Raster*)buffer)[total_entries].value, ((Raster*)buffer)[total_entries-1].value);
              r_duplicate++;
              total_entries++;
            }
          }
          else
          {
#ifndef NO_BIL_FILE_OUTPUT
            fwrite(&no_data, 2, 1, raster_file);
#endif
          }
        }
      }
      else
      {
        for (int col = 0; col < ncols; col++)
        {
#ifndef NO_BIL_FILE_OUTPUT
          fwrite(&no_data, 2, 1, raster_file);
#endif
        }
      }
    }
  }
  else
  {
    // output a no_data_value band to the bil file 
    short no_data = no_data_value;
    for (int row = 0; row < nrows; row++)
    {
      for (int col = 0; col < ncols; col++)
      {
#ifndef NO_BIL_FILE_OUTPUT
        fwrite(&no_data, 2, 1, raster_file);
#endif
      }
    }
  }
  // delete the temporary files
  remove(file_name);
  return r_duplicate;
*/
}

void SRbufferTiles::set_file_name(const char* file_name)
{
  if (file_name == 0)
  {
    fprintf(stderr, "ERROR: file_name = NULL not supported by SRbufferTiles\n");
  }
  else
  {
    free (this->file_name);
    this->file_name = strdup(file_name);
  }
}

SRbufferTiles::SRbufferTiles()
{
  file = 0;
  buffer = 0;
  bits_buffer = -1;
  bits_number = -1;
  row_bits = -1;
  col_bits = -1;
  k_bits = -1;
  file_name = strdup("temp.tmp");
}

SRbufferTiles::~SRbufferTiles()
{
  if (buffer) free(buffer);
  free(file_name);
}
