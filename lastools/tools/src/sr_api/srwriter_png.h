/*
===============================================================================

  FILE:  srwriter_png.h

  CONTENTS:

    Writes a row-by-row raster to a binary file using the PNG format.

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006-07 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    15 June 2007 -- created after Peter's encouraging LLNL offer email from OBX

===============================================================================
*/
#ifndef SRWRITER_PNG_H
#define SRWRITER_PNG_H

#include "srwriter.h"
#include "geoprojectionconverter.h"

#include <stdio.h>

class SRwriter_png : public SRwriter
{
public:

  // SRwriter interface function implementations

  void write_header();

  void write_raster(int raster);
  void write_nodata();

  void close(bool close_files=true);

  // SRwriter_png functions

  bool open(FILE* file);
  void create_kml_overview(const char* kml_file_name, GeoProjectionConverter* kml_geo_converter);

  SRwriter_png();
  ~SRwriter_png();

private:
  FILE* file;
  void* png_ptr;
  void* info_ptr;
  int count;
  void* row;
  char* kml_file_name;
  GeoProjectionConverter* kml_geo_converter;
};

#endif
