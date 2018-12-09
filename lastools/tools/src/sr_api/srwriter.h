/*
===============================================================================

  FILE:  SRwriter.h

  CONTENTS:

    Writer interface for writing a standard row-by-row raster grid (e.g. an
    image or an elevation grid). In order to write out-of-order rasters that
    come with row and column with write_raster(row, row, value) we have to
    provide the class with an SRbuffer that it uses to buffer all rasters
    until we call close().

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2007 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    15 June 2007 -- created after skyping with oliver having "kastenwagen" fun
  
===============================================================================
*/
#ifndef SRWRITER_H
#define SRWRITER_H

#include "srbuffer.h"

class SRwriter
{
public:

  // image variables

  int nrows;
  int ncols;
  int nbits;
  int nbands;
  int nodata;

  double llx;
  double lly;
  double urx;
  double ury;

  float stepx;
  float stepy;

  SRbuffer* srbuffer;

  int r_count;

  // virtual functions

  virtual void set_nrows(int nrows){this->nrows = nrows;};
  virtual void set_ncols(int ncols){this->ncols = ncols;};
  virtual void set_nbits(int nbits){this->nbits = nbits;};
  virtual void set_nbands(int nbands){this->nbands = nbands;};
  virtual void set_nodata(int nodata){this->nodata = nodata;};
  virtual void set_lower_left(double llx, double lly){this->llx = llx; this->lly = lly;};
  virtual void set_step_size(float stepx, float stepy){this->stepx = stepx; this->stepy = stepy;};
  virtual void set_buffer(SRbuffer* srbuffer){if (this->srbuffer) delete this->srbuffer; this->srbuffer = srbuffer;};

  virtual void write_header()=0;

  virtual void write_raster(int row, int col, int value){if (srbuffer) srbuffer->write_raster(row, col, value);};
  virtual void write_raster(int raster)=0;
  virtual void write_nodata()=0;

  virtual void close(bool close_file=true){};

  virtual void world_to_raster(const double* world, float* raster) const { raster[0] = (float)((world[0] - llx) / stepx) - 0.5f; raster[1] = (float)((ury - world[1]) / stepy) - 0.5f; raster[2] = (float)world[2]; }
  virtual void raster_to_world(const float* raster, double* world) const { world[0] = llx + raster[0] * stepx; world[1] = ury - raster[1] * stepy; world[2] = raster[2]; }

  SRwriter(){r_count = nrows = ncols = nbits = nbands = -1; srbuffer = 0; llx = lly = urx = ury = stepx = stepy = -1.0f; nodata = -9999;};
  virtual ~SRwriter(){if (r_count != -1) close();};
};

#endif
