/*
===============================================================================

  FILE:  srwriter_bil.cpp
  
  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2007 martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "srwriter_bil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srbufferinmemory.h"

#define NO_CHOKE_STUPID_3DEM_VIEWER
//#undef NO_CHOKE_STUPID_3DEM_VIEWER

bool SRwriter_bil::open(FILE* file_bil, FILE* file_hdr, FILE* file_blw)
{
  if (file_bil == 0)
  {
    fprintf(stderr, "ERROR: zero file pointer not supported by SRwriter_bil\n");
    return false;
  }

  this->file_hdr = file_hdr;
  this->file_blw = file_blw;
  this->file_bil = file_bil;

  nbands = 1;
  nbits = 16;

  r_count = 0;

  return true;
}

void SRwriter_bil::write_header()
{
  urx = llx+stepx*ncols;
  ury = lly+stepy*nrows;

  // write header file

  if (file_hdr)
  {
    fprintf(file_hdr, "nrows %d\012", nrows);
    fprintf(file_hdr, "ncols %d\012", ncols);
    fprintf(file_hdr, "nbands %d\012", nbands);
    fprintf(file_hdr, "nbits %d\012", nbits);
    fprintf(file_hdr, "layout bil\012");
    fprintf(file_hdr, "nodata %d\012", nodata);
#if defined(_WIN32)   // if little endian machine (i == intel, m == motorola)
    fprintf(file_hdr, "byteorder I\012");
#else                 // else big endian machine
    fprintf(file_hdr, "byteorder M\012");
#endif
    // we also output the following info (although it may be specified in the world file)
    if (llx != -1.0f) fprintf(file_hdr, "ulxmap %lf\012", llx);
    if (lly != -1.0f) fprintf(file_hdr, "ulymap %lf\012", ury);
    if (stepx != -1.0f) fprintf(file_hdr, "xdim %g\012", stepx);
    if (stepy != -1.0f) fprintf(file_hdr, "ydim %g\012", stepy);

    // write world file

    if (file_blw)
    {
#ifdef NO_CHOKE_STUPID_3DEM_VIEWER
      fprintf(file_blw, "0.01666360294118\012");
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "0.01666360294118\012");
      fprintf(file_blw, "-90.0\012");
      fprintf(file_blw, "50.0\012");
#else
      fprintf(file_blw, "%g\012", stepx);
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "0.0\012");
      fprintf(file_blw, "%g\012", -stepy);
      fprintf(file_blw, "%lf\012", llx);
      fprintf(file_blw, "%lf\012", ury);
#endif
    }
  }

  if (srbuffer == 0)
  {
    srbuffer = new SRbufferInMemory();
  }
  srbuffer->prepare(nrows, ncols, nbits*nbands);
}

void SRwriter_bil::write_raster(int raster)
{
  if (nbits == 16)
  {
    short r = (short)raster;
    fwrite(&r, 2, 1, file_bil);
  }
  else if (nbits == 32)
  {
    fwrite(&raster, 4, 1, file_bil);
  }
  else
  {
    fprintf(stderr, "ERROR: nbits %d not supported by SRwriter_bil\n",nbits);
  }
  r_count++;
}

void SRwriter_bil::write_nodata()
{
  if (nbits == 16)
  {
    short nodata = (short)this->nodata;
    fwrite(&nodata, 2, 1, file_bil);
  }
  else if (nbits == 32)
  {
    fwrite(&nodata, 4, 1, file_bil);
  }
  else
  {
    fprintf(stderr, "ERROR: nbits %d not supported by SRwriter_bil\n",nbits);
  }
  r_count++;
}

void SRwriter_bil::close(bool close_file)
{
  // close for SRwriter_bil
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

  if (close_file)
  {
    if (file_bil) fclose(file_bil);
    if (file_hdr) fclose(file_hdr);
    if (file_blw) fclose(file_blw);
  }
  file_bil = 0;
  file_hdr = 0;
  file_blw = 0;
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

SRwriter_bil::SRwriter_bil()
{
  // init of SRwriter_bil
  file_bil = 0;
  file_hdr = 0;
  file_blw = 0;
}

SRwriter_bil::~SRwriter_bil()
{
  // clean-up for SRwriter_bil
}
