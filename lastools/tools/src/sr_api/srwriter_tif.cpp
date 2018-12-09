/*
===============================================================================

  FILE:  srwriter_tif.cpp
  
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
#include "srwriter_tif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "srbufferinmemory.h"

extern "C" {
#include "geotiffio.h"
#include "xtiffio.h"
}

bool SRwriter_tif::open(char* file_name)
{
  if (file_name == 0)
  {
    fprintf(stderr, "ERROR: zero file name pointer not supported by SRwriter_tif\n");
    return false;
  }
  this->file_name = file_name;

  nbands = 1;
  nbits = 8;

  r_count = 0;

  return true;
}

void SRwriter_tif::create_kml_overview(const char* kml_file_name, GeoProjectionConverter* kml_geo_converter)
{
  if (kml_file_name == 0)
  {
    fprintf(stderr, "ERROR: kml_file_name == NULL not supported by SRwriter_tif\n");
    return;
  }
  if (kml_geo_converter == 0)
  {
    fprintf(stderr, "ERROR: kml_geo_converter == NULL not supported by SRwriter_tif\n");
    return;
  }
  this->kml_file_name = strdup(kml_file_name);
  if (kml_geo_converter == (GeoProjectionConverter*)1)
    this->kml_geo_converter = 0; // coordinates are already in lat long
  else
    this->kml_geo_converter = kml_geo_converter;
}

void SRwriter_tif::set_compression(int compress)
{
  this->compress = compress;
}

void SRwriter_tif::write_header()
{
  urx = llx+stepx*ncols;
  ury = lly+stepy*nrows;

  if (nbits != 8)
  {
    fprintf(stderr,"WARNING: forcing nbits from %d to 8", nbits);
    nbits = 8;
  }

  if (nbands != 1 && nbands != 3)
  {
    fprintf(stderr,"WARNING: forcing nbands from %d to 1", nbands);
    nbands = 1;
  }

	tif = XTIFFOpen(file_name,"w");	
	gtif = GTIFNew((TIFF*)tif);
	if (!gtif)
	{
		printf("failed in GTIFNew\n");
    if (tif) TIFFClose((TIFF*)tif);
    if (gtif) GTIFFree((GTIF*)gtif);
	}
	
  row = new char[nbands*ncols];

	double tiepoints[6]={0,0,0,130.0,32.0,0.0};
	double pixscale[3]={1,1,0};
	
	TIFFSetField((TIFF*)tif,TIFFTAG_IMAGEWIDTH,    ncols);
	TIFFSetField((TIFF*)tif,TIFFTAG_IMAGELENGTH,   nrows);
  TIFFSetField((TIFF*)tif,TIFFTAG_COMPRESSION, (compress ? COMPRESSION_LZW : COMPRESSION_NONE));
  TIFFSetField((TIFF*)tif,TIFFTAG_PHOTOMETRIC, (nbands == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB));
  TIFFSetField((TIFF*)tif,TIFFTAG_SAMPLESPERPIXEL, nbands);
	TIFFSetField((TIFF*)tif,TIFFTAG_PLANARCONFIG,  PLANARCONFIG_CONTIG);
	TIFFSetField((TIFF*)tif,TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField((TIFF*)tif,TIFFTAG_ROWSPERSTRIP,  20L);
	TIFFSetField((TIFF*)tif,TIFFTAG_ARTIST,"LAStools (c) 2009 isenburg@cs.unc.edu");
	TIFFSetField((TIFF*)tif,TIFFTAG_GEOTIEPOINTS, 6,tiepoints);
	TIFFSetField((TIFF*)tif,TIFFTAG_GEOPIXELSCALE, 3,pixscale);

	GTIFKeySet((GTIF*)gtif, GTCitationGeoKey, TYPE_ASCII, 0, "DEM created by LAStools (c) 2009 isenburg@cs.unc.edu");

  if (kml_geo_converter)
  {
	  if (kml_geo_converter->get_GeographicTypeGeoKey()) GTIFKeySet((GTIF*)gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeographicTypeGeoKey());
	  if (kml_geo_converter->get_GTRasterTypeGeoKey()) GTIFKeySet((GTIF*)gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GTRasterTypeGeoKey());

    if (kml_geo_converter->get_GeographicTypeGeoKey()) GTIFKeySet((GTIF*)gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeographicTypeGeoKey());
	  if (kml_geo_converter->get_ellipsoid_name()) GTIFKeySet((GTIF*)gtif, GeogCitationGeoKey, TYPE_ASCII, 0, kml_geo_converter->get_ellipsoid_name());
	  if (kml_geo_converter->get_GeogGeodeticDatumGeoKey()) GTIFKeySet((GTIF*)gtif, GeogGeodeticDatumGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeogGeodeticDatumGeoKey());
	  if (kml_geo_converter->get_GeogPrimeMeridianGeoKey()) GTIFKeySet((GTIF*)gtif, GeogPrimeMeridianGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeogPrimeMeridianGeoKey());
	  if (kml_geo_converter->get_GeogLinearUnitsGeoKey()) GTIFKeySet((GTIF*)gtif, GeogLinearUnitsGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeogLinearUnitsGeoKey());
	  if (kml_geo_converter->get_GeogLinearUnitSizeGeoKey()) GTIFKeySet((GTIF*)gtif, GeogLinearUnitSizeGeoKey, TYPE_DOUBLE, 1, kml_geo_converter->get_GeogLinearUnitSizeGeoKey());
    if (kml_geo_converter->get_GeogAngularUnitsGeoKey()) GTIFKeySet((GTIF*)gtif, GeogAngularUnitsGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeogAngularUnitsGeoKey());
	  if (kml_geo_converter->get_GeogAngularUnitSizeGeoKey()) GTIFKeySet((GTIF*)gtif, GeogAngularUnitSizeGeoKey, TYPE_DOUBLE, 1, kml_geo_converter->get_GeogAngularUnitSizeGeoKey());
	  if (kml_geo_converter->get_GeogEllipsoidGeoKey()) GTIFKeySet((GTIF*)gtif, GeogEllipsoidGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeogEllipsoidGeoKey());
	  if (kml_geo_converter->get_GeogSemiMajorAxisGeoKey()) GTIFKeySet((GTIF*)gtif, GeogSemiMajorAxisGeoKey, TYPE_DOUBLE, 1, kml_geo_converter->get_GeogSemiMajorAxisGeoKey());
	  if (kml_geo_converter->get_GeogSemiMinorAxisGeoKey()) GTIFKeySet((GTIF*)gtif, GeogSemiMinorAxisGeoKey, TYPE_DOUBLE, 1, kml_geo_converter->get_GeogSemiMinorAxisGeoKey());
	  if (kml_geo_converter->get_GeogInvFlatteningGeoKey()) GTIFKeySet((GTIF*)gtif, GeogInvFlatteningGeoKey, TYPE_DOUBLE, 1, kml_geo_converter->get_GeogInvFlatteningGeoKey());
    if (kml_geo_converter->get_GeogAzimuthUnitsGeoKey()) GTIFKeySet((GTIF*)gtif, GeogAzimuthUnitsGeoKey, TYPE_SHORT, 1, kml_geo_converter->get_GeogAzimuthUnitsGeoKey());
	  if (kml_geo_converter->get_GeogPrimeMeridianLongGeoKey()) GTIFKeySet((GTIF*)gtif, GeogPrimeMeridianLongGeoKey, TYPE_DOUBLE, 1, kml_geo_converter->get_GeogPrimeMeridianLongGeoKey());

    if (kml_geo_converter->get_ProjectedCSTypeGeoKey()) GTIFKeySet((GTIF*)gtif, ProjectedCSTypeGeoKey, TYPE_SHORT,  1, kml_geo_converter->get_ProjectedCSTypeGeoKey());
	  if (kml_geo_converter->get_projection_name()) GTIFKeySet((GTIF*)gtif, PCSCitationGeoKey, TYPE_ASCII, 0, kml_geo_converter->get_projection_name());
  }

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
    fprintf(file_kml, "  <name>E%.1fk N%.1fk</name>\012",(llx/1000),(lly/1000));
    fprintf(file_kml, "  <Icon>\012");
    fprintf(file_kml, "  <href>\012");
    fprintf(file_kml, "  %s\012",kml_file_name);
    fprintf(file_kml, "  </href>\012");
    fprintf(file_kml, "  </Icon>\012");
    fprintf(file_kml, "  <LatLonBox>\012");
    double point[3];
    float elev;
    // we slightly enlarge the bounding box by 0.4 in all directions to avoid cracks between tiles
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
    fprintf(file_kml, "  <rotation>%.10lf</rotation>\012",(angle_west + angle_south + angle_east + angle_north)/4);  
    fprintf(file_kml, "  </LatLonBox>\012");  
    fprintf(file_kml, "</GroundOverlay>\012");  
    fclose(file_kml);
  }
}

void SRwriter_tif::write_raster(int raster)
{
  if (nbands == 1)
  {
    row[count] = raster & 255;
  }
  else
  {
    if (raster < 16777216/3)
    {
      row[3*count + 0] = (unsigned char)(255.0f*raster/(16777216.0f/3.0f));
      row[3*count + 1] = 0;
      row[3*count + 2] = 0;
    }
    else if (raster < 2*16777216/3)
    {
      row[3*count + 0] = (unsigned char)255;
      row[3*count + 1] = (unsigned char)(255.0f*(raster-(16777216.0f/3.0f))/(16777216.0f/3.0f));
      row[3*count + 2] = 0;
    }
    else
    {
      row[3*count + 0] = (unsigned char)255;
      row[3*count + 1] = (unsigned char)255;
      row[3*count + 2] = (unsigned char)(255.0f*(raster-(2.0f*16777216.0f/3.0f))/(16777216.0f/3.0f));
    }
  }
  count++;
  if (count == ncols)
  {
		if (TIFFWriteScanline((TIFF*)tif, row, row_count, 0) == false)
    {
			TIFFError("WriteImage","failure in WriteScanline\n");
    }
    row_count++;
    count = 0;
  }
  r_count++;
}

void SRwriter_tif::write_nodata()
{
  if (nbands == 1)
  {
    row[count] = 0;
  }
  else
  {
    row[3*count + 0] = 0;
    row[3*count + 1] = 0;
    row[3*count + 2] = 0;
  }
  count++;
  if (count == ncols)
  {
		if (TIFFWriteScanline((TIFF*)tif, row, row_count, 0) == false)
    {
			TIFFError("WriteImage","failure in WriteScanline\n");
    }
    row_count++;
    count = 0;
  }
  r_count++;
}

void SRwriter_tif::close(bool close_file)
{
  // close for SRwriter_tif
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

	GTIFWriteKeys((GTIF*)gtif);
	GTIFFree((GTIF*)gtif);
	XTIFFClose((TIFF*)tif);

  if (row) free(row);
  row = 0;
  file_name = 0;
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

SRwriter_tif::SRwriter_tif()
{
  // init of SRwriter_tif
  file_name = 0;
  tif = 0;
  gtif = 0;
  count = 0;
  row_count = 0;
  row = 0;
  compress = 1;
  kml_file_name = 0;
  kml_geo_converter = 0;
}

SRwriter_tif::~SRwriter_tif()
{
  // clean-up for SRwriter_tif
  if (srbuffer) delete srbuffer;
  if (kml_file_name) free(kml_file_name);
}
