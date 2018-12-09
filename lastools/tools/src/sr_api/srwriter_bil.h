/*
===============================================================================

  FILE:  SRwriter_bil.h

  CONTENTS:

    Writes a row-by-row raster to a binary file using the BIL format consisting
    of an ASCII *.hdr file that describes the data, a binary *.bil file that
    contains the data, and an (optional) *.bwl file that georeferences the file.

    If you want to look at the produced *.bil files with the 3DEM viewer you need
    to hack the *.bwl file to contain some bogus values that prevent that stupid
    3DEM viewer from choking ... and you must make sure that the filename does not
    contain the substring "test" as 3DEM seems to choke on those.

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    15 June 2007 -- created after skyping with oliver having "kastenwagen" fun

===============================================================================
*/
#ifndef SRWRITER_BIL_H
#define SRWRITER_BIL_H

#include "srwriter.h"

#include <stdio.h>

class SRwriter_bil : public SRwriter
{
public:

  // SRwriter interface function implementations

  void write_header();

  void write_raster(int raster);
  void write_nodata();

  void close(bool close_files=true);

  // SRwriter_bil functions

  bool open(FILE* file_bil, FILE* file_hdr=0, FILE* file_blw=0);

  SRwriter_bil();
  ~SRwriter_bil();

private:
  FILE* file_bil;
  FILE* file_hdr;
  FILE* file_blw;
};

#endif
