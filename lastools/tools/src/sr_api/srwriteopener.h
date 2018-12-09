/*
===============================================================================

  FILE:  srwriteopener.h
  
  CONTENTS:
  
    Takes a file_name and/or a format description as input and opens a streaming
    raster writer in various formats. This avoids
    (a) having to reimplement opening different formats in new applications
    (b) having to add new formats to every existing application

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2007  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:

    18 June 2007 -- created after finding out that aunt doris broke her wrist

===============================================================================
*/
#ifndef SRWRITEOPENER_H
#define SRWRITEOPENER_H

#include "srwriter.h"
#include "geoprojectionconverter.h"

#include <stdio.h>

class SRwriteOpener
{
public:

  void set_file_format(const char* file_format);
  void set_file_name(const char* file_name);

  void set_nodata_value(int nodata_value);
  void set_kml_geo_converter(GeoProjectionConverter* kml_geo_converter);
  void set_tiling(const char* file_name_base, int tile_size);
  void set_compression_quality(int compression_quality);

  SRwriter* open();

  SRwriteOpener();
  ~SRwriteOpener();

  char* file_format;
  char* file_name;
private:
  int nodata_value;
  GeoProjectionConverter* kml_geo_converter;
  char* file_name_base;
  int tile_size;
  int compression_quality;
};

#endif
