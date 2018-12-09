/*
===============================================================================

  FILE:  SRwriteTiled.h
  
  CONTENTS:
  
    Writes Streaming Lines into different files depending on a user defined
    tiling in a user defined format.

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2007  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    17 June 2007 -- created after the strawberrycanyon hike where we saw goats

===============================================================================
*/
#ifndef SRWRITE_TILED_H
#define SRWRITE_TILED_H

#include "srwriter.h"
#include "srwriteopener.h"
#include "geoprojectionconverter.h"

#include <stdio.h>

class SRwriteTiled : public SRwriter
{
public:

  // srwriter interface function implementations

  void write_header();

  void write_raster(int row, int col, int value);
  void write_raster(int raster);
  void write_nodata();

  void close(bool close_file=true);

  // srwritetiled functions

  void set_file_name_base(const char* file_name_base);
  void set_tile_size(int tile_size);
  void create_kml_overview(GeoProjectionConverter* kml_geo_converter);

  bool open(SRwriteOpener* srwriteopener);

  SRwriteTiled();
  ~SRwriteTiled();

private:
  bool open_tile_file(int tile, int x, int y);
  SRwriter** tile_writers;
  SRwriteOpener* srwriteopener;
  char* file_name_base;
  int tile_size;
  int tiles_x;
  int tiles_y;
  int r_clipped;
  FILE* kml_overview_file;
  GeoProjectionConverter* kml_geo_converter;
};

#endif
