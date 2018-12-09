/*
===============================================================================

  FILE:  SRwriter_bil_fseek.h

  CONTENTS:

    Writes a raster (e.g. a Digital Elevation Map) to a binary BIL file using
    fseek instead of the buffering schemes that the SRwriter_bil class uses.
    
    This is really just old cold that i did not want to delete yet because one
    may want to try out how bad/good this scheme really works.
    
    The compile flaf BUFFER_NOTHING directly writes each raster to the appropriate
    place in the file ... this can make the head of the hard-drive jump around all
    the time. BUFFER_ROWS also writes rasters directly to their position in the
    file, but buffers and orders, for example, 64 rasters of each row to make the
    write access more sequential.

    If you want to look at the produced *.BIL files with the 3DEM viewer you need
    to hack the *.bwl file to contain some bogus values that prevent that stupid
    3DEM viewer from choking ...
    
  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    22 June 2007 -- cleaned up code for release
    17 February 2006 -- created on cold Friday despite an early lit fire

===============================================================================
*/
#ifndef SRWRITER_BIL_FSEEK_H
#define SRWRITER_BIL_FSEEK_H

#include "srwriter.h"

#include <stdio.h>

/* one (but only one) of these COMPILE SWITCHES must be enabled */

#define BUFFER_NOTHING
//#define BUFFER_NOTHING_DONT_INITIALIZE_NODATA
//#define BUFFER_ROWS
//#define BUFFER_ROWS_DONT_INITIALIZE_NODATA

class SRwriter_bil_fseek : public SRwriter
{
public:

  // srwriter interface function implementations

  void write_header();

  void write_raster(int col, int row, float value);
  void write_raster(int raster);
  void write_nodata();

  void close();

  // srwriter_bil functions

  bool open(FILE* file_bil, FILE* file_hdr=0, FILE* file_blw=0);

  SRwriter_bil_fseek();
  ~SRwriter_bil_fseek();

private:
  FILE* file_bil;
  FILE* file_hdr;
  FILE* file_blw;

#if defined (BUFFER_ROWS) || defined (BUFFER_ROWS_DONT_INITIALIZE_NODATA)
  void* row_buffer;
  void row_buffered_write(int row, int col, short value);
  void output_row_buffer(int row);
#endif
};

#endif
