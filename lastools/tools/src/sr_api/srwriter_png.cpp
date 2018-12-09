/*
===============================================================================

  FILE:  srwriter_png.cpp
  
  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2006  martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "srwriter_png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "srbufferinmemory.h"
#include "png.h"

bool SRwriter_png::open(FILE* file)
{
  if (file == 0)
  {
    fprintf(stderr, "ERROR: zero file pointer not supported by SRwriter_png\n");
    return false;
  }
  this->file = file;

  nbands = 1;
  nbits = 8;

  r_count = 0;

  return true;
}

void SRwriter_png::create_kml_overview(const char* kml_file_name, GeoProjectionConverter* kml_geo_converter)
{
  if (kml_file_name == 0)
  {
    fprintf(stderr, "ERROR: kml_file_name == NULL not supported by SRwriter_png\n");
    return;
  }
  if (kml_geo_converter == 0)
  {
    fprintf(stderr, "ERROR: kml_geo_converter == NULL not supported by SRwriter_png\n");
    return;
  }
  this->kml_file_name = strdup(kml_file_name);
  if (kml_geo_converter == (GeoProjectionConverter*)1)
    this->kml_geo_converter = 0; // coordinates are already in lat long
  else
    this->kml_geo_converter = kml_geo_converter;
}

void SRwriter_png::write_header()
{
  urx = llx+stepx*ncols;
  ury = lly+stepy*nrows;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  
  if (!png_ptr)
    {fprintf(stderr,"ERROR: png_create_write_struct failed\n"); exit(1);}

  info_ptr = png_create_info_struct((png_structp)png_ptr);

  if (!info_ptr)
    {fprintf(stderr,"ERROR: png_create_info_struct failed\n"); exit(1);}

  if (setjmp(png_jmpbuf((png_structp)png_ptr)))
    fprintf(stderr,"ERROR: Error during init_io\n");

  png_init_io((png_structp)png_ptr, file);

  /* write header */
  if (setjmp(png_jmpbuf((png_structp)png_ptr)))
    {fprintf(stderr,"ERROR: Error during writing header"); exit(1);}

  if (nbands == 3)
  {
    if (nbits != 8)
    {
      fprintf(stderr,"WARNING: forcing nbits from %d to 8", nbits);
      nbits = 8;
    }
    png_set_IHDR((png_structp)png_ptr, (png_infop)info_ptr, ncols, nrows,
           nbits, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
           PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  }
  else
  {
    if (nbands != 1)
    {
      fprintf(stderr,"WARNING: forcing nbands from %d to 1", nbands);
      nbands = 1;
    }
    if (nbits != 8 && nbits != 16)
    {
      fprintf(stderr,"WARNING: forcing nbits from %d to 8", nbits);
      nbits = 8;
    }
    png_set_IHDR((png_structp)png_ptr, (png_infop)info_ptr, ncols, nrows,
           nbits, PNG_COLOR_TYPE_GRAY_ALPHA, PNG_INTERLACE_NONE,
           PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  }

  png_write_info((png_structp)png_ptr, (png_infop)info_ptr);

  /* write bytes */
  if (setjmp(png_jmpbuf((png_structp)png_ptr)))
    {fprintf(stderr,"ERROR: Error during writing bytes"); exit(1);}

  row = malloc(((png_infop)info_ptr)->rowbytes);
  count = 0;

  if (srbuffer == 0)
  {
    srbuffer = new SRbufferInMemory();
  }
  srbuffer->prepare(nrows, ncols, nbits*nbands);

  if (kml_file_name)
  {
    char* file_name_copy = strdup(kml_file_name);
    file_name_copy[strlen(file_name_copy)-1] = 'l';
    file_name_copy[strlen(file_name_copy)-2] = 'm';
    file_name_copy[strlen(file_name_copy)-3] = 'k';
    FILE* file_kml = fopen(file_name_copy, "w");
    if (file_kml == 0)
    {
      fprintf(stderr,"ERROR: cannot open '%s' for write\n", file_name_copy);
      free(file_name_copy);
      return;
    }
    fprintf(file_kml, "<GroundOverlay>\012");
    fprintf(file_kml, "  <name>E%.3fk N%.3fk</name>\012",(llx/1000),(lly/1000));
    fprintf(file_kml, "  <Icon><href>%s</href></Icon>\012",kml_file_name);
    fprintf(file_kml, "  <LatLonBox>\012");
    double point[3];
    float elev;
    // we slightly enlarge the bounding box by (stepx*ncols)*0.4 in all directions to avoid cracks between tiles
#define ENLARGEMENT_FACTOR_PROJECTED 0.4
#define ENLARGEMENT_FACTOR_LATLONG 0.0001
    double lon_ll,lat_ll;
    if (kml_geo_converter)
    {
      point[0] = llx - ENLARGEMENT_FACTOR_PROJECTED; point[1] = lly - ENLARGEMENT_FACTOR_PROJECTED; point[2] = 0;
      kml_geo_converter->to_kml_style_lat_long_elevation(point, lat_ll, lon_ll, elev);
    }
    else
    {
      lon_ll = llx - ENLARGEMENT_FACTOR_LATLONG; lat_ll = lly - ENLARGEMENT_FACTOR_LATLONG; elev = 0;
    }
    double lon_ul,lat_ul;
    if (kml_geo_converter)
    {
      point[0] = llx - ENLARGEMENT_FACTOR_PROJECTED; point[1] = ury + ENLARGEMENT_FACTOR_PROJECTED; point[2] = 0;
      kml_geo_converter->to_kml_style_lat_long_elevation(point, lat_ul, lon_ul, elev);
    }
    else
    {
      lon_ul = llx - ENLARGEMENT_FACTOR_LATLONG; lat_ul = ury + ENLARGEMENT_FACTOR_LATLONG; elev = 0;
    }
    double lon_lr,lat_lr;
    if (kml_geo_converter)
    {
      point[0] = urx + ENLARGEMENT_FACTOR_PROJECTED; point[1] = lly - ENLARGEMENT_FACTOR_PROJECTED; point[2] = 0;
      kml_geo_converter->to_kml_style_lat_long_elevation(point, lat_lr, lon_lr, elev);
    }
    else
    {
      lon_lr = urx + ENLARGEMENT_FACTOR_LATLONG; lat_lr = lly - ENLARGEMENT_FACTOR_LATLONG; elev = 0;
    }
    double lon_ur,lat_ur;
    if (kml_geo_converter)
    {
      point[0] = urx + ENLARGEMENT_FACTOR_PROJECTED; point[1] = ury + ENLARGEMENT_FACTOR_PROJECTED; point[2] = 0;
      kml_geo_converter->to_kml_style_lat_long_elevation(point, lat_ur, lon_ur, elev);
    }
    else
    {
      lon_ur = urx + ENLARGEMENT_FACTOR_LATLONG; lat_ur = ury + ENLARGEMENT_FACTOR_LATLONG; elev = 0;
    }
    fprintf(file_kml, "  <south>%.10lf</south>\012",(lat_ll+lat_lr)/2);
    fprintf(file_kml, "  <west>%.10lf</west>\012",(lon_ll+lon_ul)/2);
    fprintf(file_kml, "  <north>%.10lf</north>\012",(lat_ul+lat_ur)/2);  
    fprintf(file_kml, "  <east>%.10lf</east>\012",(lon_lr+lon_ur)/2);
    // fancy hack to calculate how to rotate the raster for correct alignment
    double west[2];
    west[0] = lon_ul + lon_ll - lon_ur - lon_lr;
    west[1] = lat_ul + lat_ll - lat_ur - lat_lr;
    double angle_west = acos(-west[0]/sqrt(west[0]*west[0] + west[1]*west[1]))/3.141592653589793238462643383279502884197169399375105820974944592*180.0;
    double south[2];
    south[0] = lon_ll + lon_lr - lon_ul - lon_ur;
    south[1] = lat_ll + lat_lr - lat_ul - lat_ur;
    double angle_south = acos(-south[1]/sqrt(south[0]*south[0] + south[1]*south[1]))/3.141592653589793238462643383279502884197169399375105820974944592*180.0;
    double east[2];
    east[0] = lon_ur + lon_lr - lon_ul - lon_ll;
    east[1] = lat_ur + lat_lr - lat_ul - lat_ll;
    double angle_east = acos(east[0]/sqrt(east[0]*east[0] + east[1]*east[1]))/3.141592653589793238462643383279502884197169399375105820974944592*180.0;
    double north[2];
    north[0] = lon_ul + lon_ur - lon_ll - lon_lr;
    north[1] = lat_ul + lat_ur - lat_ll - lat_lr;
    double angle_north = acos(north[1]/sqrt(north[0]*north[0] + north[1]*north[1]))/3.141592653589793238462643383279502884197169399375105820974944592*180.0;
    if (lon_ul > lon_ll)
    {
      fprintf(file_kml, "  <rotation>%.10lf</rotation>\012",(angle_west + angle_south + angle_east + angle_north)/-4.0);
    }
    else
    {
      fprintf(file_kml, "  <rotation>%.10lf</rotation>\012",(angle_west + angle_south + angle_east + angle_north)/4.0);
    }
    fprintf(file_kml, "  </LatLonBox>\012");  
    fprintf(file_kml, "</GroundOverlay>\012");  
    fclose(file_kml);
  }
}

void SRwriter_png::write_raster(int raster)
{
  if (nbits == 8)
  {
    if (nbands == 1)
    {
      ((png_bytep)row)[2*count + 0] = (png_byte)(raster & 255);
      ((png_bytep)row)[2*count + 1] = (png_byte)255;
    }
    else
    {
      if (raster < 16777216/3)
      {
        ((png_bytep)row)[4*count + 0] = (png_byte) (255.0f*(float)raster/(16777216.0f/3.0f));
        ((png_bytep)row)[4*count + 1] = (png_byte) 0;
        ((png_bytep)row)[4*count + 2] = (png_byte) 0;
        ((png_bytep)row)[4*count + 3] = (png_byte) 255;
      }
      else if (raster < 2*16777216/3)
      {
        ((png_bytep)row)[4*count + 0] = (png_byte) 255;
        ((png_bytep)row)[4*count + 1] = (png_byte) (255.0f*((float)raster-(16777216.0f/3.0f))/(16777216.0f/3.0f));
        ((png_bytep)row)[4*count + 2] = (png_byte) 0;
        ((png_bytep)row)[4*count + 3] = (png_byte) 255;
      }
      else
      {
        ((png_bytep)row)[4*count + 0] = (png_byte) 255;
        ((png_bytep)row)[4*count + 1] = (png_byte) 255;
        ((png_bytep)row)[4*count + 2] = (png_byte) (225.0f*((float)raster-(2.0f*16777216.0f/3.0f))/(16777216.0f/3.0f));
        ((png_bytep)row)[4*count + 3] = (png_byte) 255;
      }
    }
  }
  else
  {
    ((png_int_16p)row)[2*count + 0] = (png_int_16)(raster & 65335);
    ((png_int_16p)row)[2*count + 1] = (png_int_16)65535;
  }
  count++;
  if (count == ncols)
  {
    png_write_row((png_structp)png_ptr, (png_bytep)row);
    count = 0;
  }
  r_count++;
}

void SRwriter_png::write_nodata()
{
  if (nbits == 8)
  {
    if (nbands == 1)
    {
      ((png_bytep)row)[2*count + 0] = (png_byte)0;
      ((png_bytep)row)[2*count + 1] = (png_byte)0;
    }
    else
    {
      ((png_bytep)row)[4*count + 0] = (png_byte)0;
      ((png_bytep)row)[4*count + 1] = (png_byte)0;
      ((png_bytep)row)[4*count + 2] = (png_byte)0;
      ((png_bytep)row)[4*count + 3] = (png_byte)0;
    }
  }
  else
  {
    ((png_int_16p)row)[2*count + 0] = (png_int_16)0;
    ((png_int_16p)row)[2*count + 1] = (png_int_16)0;
  }
  count++;
  if (count == ncols)
  {
    png_write_row((png_structp)png_ptr, (png_bytep)row);
    count = 0;
  }
  r_count++;
}

void SRwriter_png::close(bool close_file)
{
  // close for SRwriter_png
  int sort_buffer_size = 0;
  unsigned char* sort_buffer = 0;
  if (sort_buffer_size = srbuffer->required_sort_buffer_size())
  {
    sort_buffer = (unsigned char*)malloc(sort_buffer_size);
  }
  srbuffer->sort_and_output(sort_buffer, this);
  if (srbuffer->r_duplicate) fprintf(stderr, "WARNING: there were %d duplicate rasters\n",srbuffer->r_duplicate);
  if (srbuffer->r_clipped) fprintf(stderr, "there were %d clipped rasters (and %d unclipped ones)\n",srbuffer->r_clipped, srbuffer->r_count);
  if (sort_buffer) free(sort_buffer);

  if (setjmp(png_jmpbuf((png_structp)png_ptr)))
    {fprintf(stderr,"ERROR: Error during end of write"); exit(1);}
  png_write_end((png_structp)png_ptr, NULL);
  png_destroy_write_struct(((png_structpp)&png_ptr), ((png_infopp)&info_ptr));
  png_ptr = 0;
  info_ptr = 0;
  if (row) free(row);
  row = 0;
  if (close_file && file && file != stdout) fclose(file);
  file = 0;
  if (kml_file_name) free(kml_file_name);
  kml_file_name = 0;
  kml_geo_converter = 0;

  // close for SRwriter interface
  if (r_count != -1)
  {
    if (r_count != nrows*ncols)
    {
      fprintf(stderr, "WARNING: r_count is %d but nrows (%d) * ncols (%d) is %d\n",r_count,nrows,ncols,nrows*ncols);
    }
    r_count = -1;
  }
}

SRwriter_png::SRwriter_png()
{
  // init of SRwriter_png
  file = 0;
  png_ptr = 0;
  info_ptr = 0;
  count = 0;
  row = 0;
  kml_file_name = 0;
  kml_geo_converter = 0;
}

SRwriter_png::~SRwriter_png()
{
  // clean-up for SRwriter_png
  if (srbuffer) delete srbuffer;
  if (kml_file_name) free(kml_file_name);
}
