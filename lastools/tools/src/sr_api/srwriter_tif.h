/*
===============================================================================

  FILE:  srwriter_tif.h

  CONTENTS:

    Writes a row-by-row raster to a binary file using the GeoTIFF format.

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    03 May 2009 -- created on BART after checking out 1917 Chestnut again

===============================================================================
*/
#ifndef SRWRITER_TIF_H
#define SRWRITER_TIF_H

#include "srwriter.h"
#include "geoprojectionconverter.h"

#include <stdio.h>

class SRwriter_tif : public SRwriter
{
public:

  // SRwriter interface function implementations

  void write_header();

  void write_raster(int raster);
  void write_nodata();

  void close(bool close_files=true);

  // SRwriter_tif functions

  bool open(char* file_name);
  void create_kml_overview(const char* kml_file_name, GeoProjectionConverter* kml_geo_converter);
  void set_compression(int compress);

  SRwriter_tif();
  ~SRwriter_tif();

private:
  char* file_name;
  void* gtif;
  void* tif;
  int count;
  int row_count;
  char* row;
  int compress;
  char* kml_file_name;
  GeoProjectionConverter* kml_geo_converter;
};

#endif
