/*
===============================================================================

  FILE:  srwriteopener.cpp
  
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
#include "srwriteopener.h"

// streaming raster writers
#include "srwriter_bil.h"
#include "srwriter_png.h"
#include "srwriter_jpg.h"
#include "srwriter_tif.h"
#include "srwritetiled.h"

#include <stdlib.h>
#include <string.h>

void SRwriteOpener::set_file_format(const char* file_format)
{
  if (this->file_format) free(this->file_format);
  this->file_format = strdup(file_format);
}

void SRwriteOpener::set_file_name(const char* file_name)
{
  if (this->file_name) free(this->file_name);
  this->file_name = strdup(file_name);
}

void SRwriteOpener::set_nodata_value(int nodata_value)
{
  this->nodata_value = nodata_value;
}

void SRwriteOpener::set_kml_geo_converter(GeoProjectionConverter* kml_geo_converter)
{
  this->kml_geo_converter = kml_geo_converter;
}

void SRwriteOpener::set_tiling(const char* file_name_base, int tile_size)
{
  if (this->file_name_base) free(this->file_name_base);
  this->file_name_base = strdup(file_name_base);
  this->tile_size = tile_size;
}

void SRwriteOpener::set_compression_quality(int compression_quality)
{
  this->compression_quality = compression_quality;
}

SRwriter* SRwriteOpener::open()
{
  // if the file format is not given explicitely we try to guess it
  if (file_format == 0)
  {
    if (file_name == 0)
    {
      if (file_name_base)
      {
        fprintf(stderr, "WARNING: no output file format was specified. assuming PNG.\n");
        file_format = strdup("png");
      }
      else
      {
        fprintf(stderr, "ERROR: neither output file name nor file format was specified\n");
        return 0;
      }
    }
    else if (strstr(file_name, ".bil"))
      file_format = strdup("bil");
    else if (strstr(file_name, ".png"))
      file_format = strdup("png");
    else if (strstr(file_name, ".jpg"))
      file_format = strdup("jpg");
    else if (strstr(file_name, ".tif"))
      file_format = strdup("tif");
    else
    {
      fprintf(stderr, "ERROR: could not guess file format from file name '%s'\n", file_name);
      return 0;
    }
  }

  // maybe we just open a tile writer ... then the actual file opening happens later per tile
  if (file_name_base)
  {
    SRwriteTiled* srwritetiled = new SRwriteTiled();
    if (srwritetiled->open(this))
    {
      srwritetiled->set_file_name_base(file_name_base);
      srwritetiled->set_tile_size(tile_size);
      if (kml_geo_converter) srwritetiled->create_kml_overview(kml_geo_converter);
      // make sure we don't open another tiling when srwritetiled calls open() again
      free(file_name_base); file_name_base = 0; tile_size = 0;
      return srwritetiled;
    }
  }
  else
  {
    FILE* file;

    // depending on the file format we open the file in binary or text mode
    bool is_binary = true;
    if (strcmp(file_format, "txt") == 0)
      is_binary = false;

    // open the file
    file = fopen(file_name, (is_binary ? "wb" : "w"));
    if (file == 0)
    {
      fprintf(stderr,"ERROR: cannot open %s\n",file_name);
      return 0;
    }

    // now we open the respective writer

    if (strcmp(file_format, "png") == 0)
    {
      SRwriter_png* srwriter_png = new SRwriter_png();
      if (srwriter_png->open(file))
      {
        if (kml_geo_converter)
        {
          srwriter_png->create_kml_overview(file_name, kml_geo_converter);
        }
        return srwriter_png;
      }
      delete srwriter_png;
    }
    else if (strcmp(file_format, "jpg") == 0)
    {
      SRwriter_jpg* srwriter_jpg = new SRwriter_jpg();
      if (srwriter_jpg->open(file))
      {
        if (kml_geo_converter)
        {
          srwriter_jpg->create_kml_overview(file_name, kml_geo_converter);
        }
        if (compression_quality != -1)
        {
          srwriter_jpg->set_compression_quality(compression_quality);
        }
        return srwriter_jpg;
      }
      delete srwriter_jpg;
    }
    else if (strcmp(file_format, "tif") == 0)
    {
      fclose(file);
      SRwriter_tif* srwriter_tif = new SRwriter_tif();
      if (srwriter_tif->open(file_name))
      {
        if (kml_geo_converter)
        {
          srwriter_tif->create_kml_overview(file_name, kml_geo_converter);
        }
        if (compression_quality != -1)
        {
          srwriter_tif->set_compression(compression_quality);
        }
        return srwriter_tif;
      }
      delete srwriter_tif;
    }
    else if (strcmp(file_format, "bil") == 0)
    {
      if (strstr(file_name, "test"))
      {
        fprintf(stderr,"WARNING: the 3dem viewer cannot handle files containing the word 'test'\n");
      }
      char* file_name_copy = strdup(file_name);
      file_name_copy[strlen(file_name_copy)-1] = 'r';
      file_name_copy[strlen(file_name_copy)-2] = 'd';
      file_name_copy[strlen(file_name_copy)-3] = 'h';
      FILE* file_hdr = fopen(file_name_copy, "w");
      if (file_hdr == 0)
      {
        fprintf(stderr,"ERROR: cannot open '%s' for write\n", file_name_copy);
        free(file_name_copy);
        return 0;
      }
      file_name_copy[strlen(file_name_copy)-1] = 'w';
      file_name_copy[strlen(file_name_copy)-2] = 'l';
      file_name_copy[strlen(file_name_copy)-3] = 'b';
      FILE* file_blw = fopen(file_name_copy, "w");
      if (file_blw == 0)
      {
        fprintf(stderr,"ERROR: cannot open '%s' for write\n", file_name_copy);
        free(file_name_copy);
        return 0;
      }
      free(file_name_copy);
      SRwriter_bil* srwriter_bil = new SRwriter_bil();
      if (srwriter_bil->open(file, file_hdr, file_blw))
      {
        srwriter_bil->set_nodata(nodata_value);
        return srwriter_bil;
      }
      delete srwriter_bil;
    }
    else
    {
      fprintf(stderr,"ERROR: cannot determine the writer for format '%s'\n", file_format);
    }
  }
  return 0;
}

SRwriteOpener::SRwriteOpener()
{
  file_format = 0;
  file_name = 0;
  nodata_value = -9999;
  kml_geo_converter = 0;
  file_name_base = 0;
  tile_size = 0;
  compression_quality = -1;
}

SRwriteOpener::~SRwriteOpener()
{
  if (file_format) free(file_format);
  if (file_name) free(file_name);
  if (file_name_base) free(file_name_base);
}
