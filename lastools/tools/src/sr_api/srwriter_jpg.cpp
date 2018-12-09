/*
===============================================================================

  FILE:  srwriter_jpg.cpp
  
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
#include "srwriter_jpg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "srbufferinmemory.h"

extern "C" {
#include "jpeglib.h"
#include "jerror.h"
}

bool SRwriter_jpg::open(FILE* file)
{
  if (file == 0)
  {
    fprintf(stderr, "ERROR: zero file pointer not supported by SRwriter_jpg\n");
    return false;
  }
  this->file = file;

  nbands = 1;
  nbits = 8;

  r_count = 0;

  return true;
}

void SRwriter_jpg::create_kml_overview(const char* kml_file_name, GeoProjectionConverter* kml_geo_converter)
{
  if (kml_file_name == 0)
  {
    fprintf(stderr, "ERROR: kml_file_name == NULL not supported by SRwriter_jpg\n");
    return;
  }
  if (kml_geo_converter == 0)
  {
    fprintf(stderr, "ERROR: kml_geo_converter == NULL not supported by SRwriter_jpg\n");
    return;
  }
  this->kml_file_name = strdup(kml_file_name);
  if (kml_geo_converter == (GeoProjectionConverter*)1)
    this->kml_geo_converter = 0; // coordinates are already in lat long
  else
    this->kml_geo_converter = kml_geo_converter;
}

void SRwriter_jpg::set_compression_quality(int compression_quality)
{
  if (compression_quality < 10 || compression_quality > 90)
  {
    fprintf(stderr, "ERROR: compression_quality == %d not supported by SRwriter_jpg\n", compression_quality);
    return;
  }
  this->compression_quality = compression_quality;
}

/* Expanded data destination object for stdio output */

typedef struct {
  struct jpeg_destination_mgr pub; /* public fields */

  FILE * outfile;    /* target stream */
  JOCTET * buffer;    /* start of buffer */
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE  4096  /* choose an efficiently fwrite'able size */

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

METHODDEF(void)
init_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  /* Allocate the output buffer --- it will be released when done with image */
  dest->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
          OUTPUT_BUF_SIZE * sizeof(JOCTET));

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  if (fwrite(dest->buffer, 1, OUTPUT_BUF_SIZE, dest->outfile) !=
      (size_t) OUTPUT_BUF_SIZE)
    ERREXIT(cinfo, JERR_FILE_WRITE);

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

  return TRUE;
}

/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

METHODDEF(void)
term_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

  /* Write any data remaining in the buffer */
  if (datacount > 0) {
    if (fwrite(dest->buffer, 1, datacount, dest->outfile) != datacount)
      ERREXIT(cinfo, JERR_FILE_WRITE);
  }
  if( fflush(dest->outfile) != 0 )
    ERREXIT(cinfo, JERR_FILE_WRITE);
}

/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

void
jpeg_vsiio_dest (j_compress_ptr cinfo, FILE * outfile)
{
  my_dest_ptr dest;

  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if (cinfo->dest == NULL) {  /* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
          sizeof(my_destination_mgr));
  }

  dest = (my_dest_ptr) cinfo->dest;
  dest->pub.init_destination = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination = term_destination;
  dest->outfile = outfile;
}

void SRwriter_jpg::write_header()
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

  sCInfo = (void*)malloc(sizeof(struct jpeg_compress_struct));
  struct jpeg_error_mgr sJErr;
    
  ((struct jpeg_compress_struct*)sCInfo)->err = jpeg_std_error( &sJErr );
  jpeg_create_compress( (struct jpeg_compress_struct*)sCInfo );
    
  jpeg_vsiio_dest( (struct jpeg_compress_struct*)sCInfo, file );
  
  ((struct jpeg_compress_struct*)sCInfo)->image_width = ncols;
  ((struct jpeg_compress_struct*)sCInfo)->image_height = nrows;
  ((struct jpeg_compress_struct*)sCInfo)->input_components = nbands;
  if( nbands == 1 )
  {
    ((struct jpeg_compress_struct*)sCInfo)->in_color_space = JCS_GRAYSCALE;
  }
  else
  {
    ((struct jpeg_compress_struct*)sCInfo)->in_color_space = JCS_RGB;
  }
  jpeg_set_defaults( (struct jpeg_compress_struct*)sCInfo );
    
  jpeg_set_quality( (struct jpeg_compress_struct*)sCInfo, compression_quality, TRUE );

  jpeg_start_compress( (struct jpeg_compress_struct*)sCInfo, TRUE );

  row = (JSAMPLE*)malloc(nbands*ncols*sizeof(JSAMPLE));
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

void SRwriter_jpg::write_raster(int raster)
{
  if (nbands == 1)
  {
    ((JSAMPLE*)row)[count] = (JSAMPLE)(raster & 255);
  }
  else
  {
    if (raster < 16777216/3)
    {
      ((JSAMPLE*)row)[3*count + 0] = (JSAMPLE) (255.0f*(float)raster/(16777216.0f/3.0f));
      ((JSAMPLE*)row)[3*count + 1] = (JSAMPLE) 0;
      ((JSAMPLE*)row)[3*count + 2] = (JSAMPLE) 0;
    }
    else if (raster < 2*16777216/3)
    {
      ((JSAMPLE*)row)[3*count + 0] = (JSAMPLE) 255;
      ((JSAMPLE*)row)[3*count + 1] = (JSAMPLE) (255.0f*((float)raster-(16777216.0f/3.0f))/(16777216.0f/3.0f));
      ((JSAMPLE*)row)[3*count + 2] = (JSAMPLE) 0;
    }
    else
    {
      ((JSAMPLE*)row)[3*count + 0] = (JSAMPLE) 255;
      ((JSAMPLE*)row)[3*count + 1] = (JSAMPLE) 255;
      ((JSAMPLE*)row)[3*count + 2] = (JSAMPLE) (225.0f*((float)raster-(2.0f*16777216.0f/3.0f))/(16777216.0f/3.0f));
    }
  }
  count++;
  if (count == ncols)
  {
    JSAMPLE* ppSamples = (JSAMPLE*)row;
    jpeg_write_scanlines( (struct jpeg_compress_struct*)sCInfo, &ppSamples, 1 );
    count = 0;
  }
  r_count++;
}

void SRwriter_jpg::write_nodata()
{
  if (nbands == 1)
  {
    ((JSAMPLE*)row)[count] = (JSAMPLE)0;
  }
  else
  {
    ((JSAMPLE*)row)[3*count + 0] = (JSAMPLE)0;
    ((JSAMPLE*)row)[3*count + 1] = (JSAMPLE)0;
    ((JSAMPLE*)row)[3*count + 2] = (JSAMPLE)0;
  }
  count++;
  if (count == ncols)
  {
    JSAMPLE* ppSamples = (JSAMPLE*)row;
    jpeg_write_scanlines( (struct jpeg_compress_struct*)sCInfo, &ppSamples, 1 );
    count = 0;
  }
  r_count++;
}

void SRwriter_jpg::close(bool close_file)
{
  // close for SRwriter_jpg
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

  jpeg_finish_compress( (struct jpeg_compress_struct*)sCInfo );
  jpeg_destroy_compress( (struct jpeg_compress_struct*)sCInfo );
  sCInfo = 0;

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

SRwriter_jpg::SRwriter_jpg()
{
  // init of SRwriter_jpg
  file = 0;
  sCInfo = 0;
  count = 0;
  row = 0;
  compression_quality = 85;
  kml_file_name = 0;
  kml_geo_converter = 0;
}

SRwriter_jpg::~SRwriter_jpg()
{
  // clean-up for SRwriter_jpg
  if (srbuffer) delete srbuffer;
  if (kml_file_name) free(kml_file_name);
}
