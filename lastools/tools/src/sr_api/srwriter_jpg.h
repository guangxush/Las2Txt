/*
===============================================================================

  FILE:  srwriter_jpg.h

  CONTENTS:

    Writes a row-by-row raster to a binary file using the JPG format. Only 8 bit
    are supported with either 1 band (GREY) or 3 bands (RGB).

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    20 June 2007 -- created after accepting the LLNL offer with 5K signing bonus

===============================================================================
*/
#ifndef SRWRITER_JPG_H
#define SRWRITER_JPG_H

#include "srwriter.h"
#include "geoprojectionconverter.h"

#include <stdio.h>

class SRwriter_jpg : public SRwriter
{
public:

  // SRwriter interface function implementations

  void write_header();

  void write_raster(int raster);
  void write_nodata();

  void close(bool close_files=true);

  // SRwriter_jpg functions

  bool open(FILE* file);
  void create_kml_overview(const char* kml_file_name, GeoProjectionConverter* kml_geo_converter);
  void set_compression_quality(int compression_quality);

  SRwriter_jpg();
  ~SRwriter_jpg();

private:
  FILE* file;
  void* sCInfo;
  int count;
  void* row;
  int compression_quality;
  char* kml_file_name;
  GeoProjectionConverter* kml_geo_converter;
};

#endif
