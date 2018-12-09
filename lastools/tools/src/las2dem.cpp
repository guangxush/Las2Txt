/*
===============================================================================

  FILE:  las2dem.cpp
  
  CONTENTS:
  
    This tool reads LIDAR data in the LAS format, triangulates them into a
    TIN, and then rasters the TIN onto a DEM. The output is either in PNG,
    JPG, or BIL format. Optionally, a KML file is generated that puts the
    DEM in geospatial context to be displayed in Google Earth. If the LAS
    file contains projection information (i.e. geo keys as variable length
    records) then this is used to create the metadata for the KML file. It
    is also possible to provide georeferencing information in the command-
    line to create the KML file.

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2009 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    11 April 2009 -- created after making a 120K offer on 1881 Chestnut 
  
===============================================================================
*/

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lasreader.h"
#include "triangulate.h"
#include "srwriter.h"
#include "srwriteopener.h"

#include "srbufferrowbands.h"
#include "srbufferrows.h"
#include "srbuffersimple.h"
#include "srbuffertiles.h"
#include "srbufferinmemory.h"


float kill_threshold_squared = 10000.0f;
SRwriter* srwriter = 0;

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"las2dem -i lidar.las -o lidar.png\n");
  fprintf(stderr,"las2dem -i lidar.las -step 0.5 -o lidar.tif -false_coloring\n");
  fprintf(stderr,"las2dem -i lidar.las -ncols 400 -nrows 400 -o lidar.jpg -false_coloring\n");
  fprintf(stderr,"las2dem -i lidar.las -first_only -o lidar.jpg -utm 11S -ellipsoid 23\n");
  fprintf(stderr,"las2dem -last_only lidar.las lidar.png -sp83 TX_N -ellipsoid 11 -elevation_coloring\n");
  fprintf(stderr,"las2dem -i lidar.las -keep_class 2 -keep_class 3 -keep_class 9 -o lines.png -sp27 PA_N -ellipsoid 5\n");
  fprintf(stderr,"las2dem -i lidar.las -keep_class 8 -o lines.bil -nbits 32 -elevation\n");
  fprintf(stderr,"las2dem -h\n");
  if (wait)
  {
    fprintf(stderr,"<press ENTER>\n");
    getc(stdin);
  }
  exit(1);
}

static void byebye(bool wait=false)
{
  if (wait)
  {
    fprintf(stderr,"<press ENTER>\n");
    getc(stdin);
  }
  exit(1);
}

static void ptime(const char *const msg)
{
  float t= ((float)clock())/CLOCKS_PER_SEC;
  fprintf(stderr, "cumulative CPU time thru %s = %f\n", msg, t);
}

static inline void VecZero3fv(float v[3])
{
  v[0] = 0.0f;
  v[1] = 0.0f;
  v[2] = 0.0f;
}

static inline void VecSet3fv(float v[3], float x, float y, float z)
{
  v[0] = x;
  v[1] = y;
  v[2] = z;
}

static inline void VecSelfAdd3fv(float v[3], const float a[3])
{
  v[0] += a[0];
  v[1] += a[1];
  v[2] += a[2];
}

static inline void VecSubtract3fv(float v[3], const float a[3], const float b[3])
{
  v[0] = a[0] - b[0];
  v[1] = a[1] - b[1];
  v[2] = a[2] - b[2];
}

static inline float VecDotProd3fv(const float a[3], const float b[3])
{
  return( a[0]*b[0] + a[1]*b[1] + a[2]*b[2] );
}

static inline void VecCrossProd3fv(float v[3], const float a[3], const float b[3]) // c = a X b
{ 
  VecSet3fv(v, a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]);
}

static inline float VecLength3fv(const float v[3])
{
  return( (float)sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]) );
}

static inline bool VecSelfNormalize3fv(float v[3])
{
  float l = VecLength3fv(v);
  if (l)
  {
    v[0] /= l;
    v[1] /= l;
    v[2] /= l;
    return true;
  }
  return false;
}

static inline bool VecCcwNormNormal3fv(float n[3], const float a[3], const float b[3], const float c[3])
{
  float ab[3], ac[3];
  VecSubtract3fv(ab,b,a);
  VecSubtract3fv(ac,c,a);
  VecCrossProd3fv(n,ab,ac);
  return VecSelfNormalize3fv(n);
}

//----------------------------------------------------------------------------
// CLAMP TO RASTER INT THAT IS INSIDE TRIANGLE BIASED TOWARDS MIN (INCLUSIVE)
//----------------------------------------------------------------------------

static inline int Ceil(const float x)
{
  int ceilx = (int)x;
  if (x>ceilx) ceilx++;
  return ceilx;
}

static inline int Floor(const float x)
{
  int floorx = (int)x;
  if (x<floorx) floorx--;
  return floorx;
}

/*
  Rasterize a triangle, handling all the boundary cases.
  We know that ay<by<cy, with ties broken on x coordinate 
    (horizontal lines are thought of as having positive slope epsilon)
  We shift the triangle up and right to resolve points on edges.
    (makes sure northing & easting are in a triangle).
  Thus, y range is floor(ay)+1 to <floor(cy)+1; 
  and x range is ceil(first x) to < ceil(xlimit)

  We consider scanlines in two cases:
    < b: from floor(ay)+1 to <=by (or <floor(by)+1), then
    >=b: from max(floor(ay)+1,floor(by)+1) to <=cy (or <floor(cy)+1).

  For scanline iy, we find the first x and limit x coordinates.
  Both are rounded down (toward the left), 
    so the first point is included if on the boundary 
    and the last is not if on the boundary.
  There are two subcases (since we know that the orientation det != 0): 
    if det > 0, then the triangle is to the right of middle point b
       <=b case: first x determined by ab, bc; xlimit by ac;
       > b case: first x determined by ac; xlimit by ab, bc;
   if det < 0, then the triangle is to the left of middle point b
       <=b case: first x determined by ac; xlimit by ab, bc;
       > b case: first x determined by ab, bc; xlimit by ac;

  In each case, if the line is ab, ix = Ceil(ax + (ax-bx)/(ay-by)*(iy-ay)) 
 
  That determines the pairs (ix,iy) at which to interpolate.

  The linear interpolant satisfies my favorite determinant in homog coords:
    Det[b; a; c; (1 ix iy Z)] = 0.
  Subtract first row to go to cartesian coords: 
    Det[a-b; c-b; (ix-bx iy-by Z-bz)] = 0;
  Expand by minors in last row:
    Z = bz - (ix-bx)*Dx/det + (iy-by)*Dy/det, 
  where det = the orientation Det[ax-bx ay-by; cx-bx cy-by];
    Dx  = Det[ay-by az-bz; cy-by cz-bz]; 
    Dy  = Det[ax-bx az-bz; cx-bx cz-bz]; 
*/
static void raster_triangle(const float* a, const float* b, const float* c)
{
  const float* t;

  // SORT VERTICES BY Y VALUES, breaking ties by x
#define SWAP(a,b,t) t=a,a=b,b=t
#define LEXGREATER(a,b) (a[1]>b[1] || (a[1] == b[1] && a[0]>b[0]))

  // enforce ay<=by<=cy
  if (LEXGREATER(a,c)) SWAP(a,c,t);
  if (LEXGREATER(a,b)) SWAP(a,b,t);
  else if (LEXGREATER(b,c)) SWAP(b,c,t);
      
  int iy = Floor(a[1])+1;      // start just above lowest point = a
  if (iy > c[1])
  {
#ifdef COLLECT_STATISTICS
    count_early_exits++;
#endif
    return; // triangles has no rasters
  }
    // CALCULATE EDGE VALUE DIFFERENCES IN X and Y DIRECTION
  float ABx=b[0]-a[0], ABy=b[1]-a[1];
  float BCx=c[0]-b[0], BCy=c[1]-b[1];
  
  // Move so b is origin. 
  float Ax=a[0]-b[0], Ay=a[1]-b[1];
  float Cx=c[0]-b[0], Cy=c[1]-b[1];
  float ACx=c[0]-a[0], ACy=c[1]-a[1];
 
  double det = Ax*Cy - Ay*Cx; // orientation determinant: + if A->C is ccw around b. 0 for degenerate line triangle;
  
  if (det == 0 || Ax*Ax + Ay*Ay > kill_threshold_squared || ACx*ACx + ACy*ACy > kill_threshold_squared || Cx*Cx + Cy*Cy > kill_threshold_squared)
  {
    return;  // triangle is too badly shaped
  }

  double ACxy = ACx/ACy;  // know ACy>0
  double Az = a[2]-b[2], Cz = c[2]-b[2], ACz = c[2]-a[2];
  double Dx = (Az*Cy-Ay*Cz)/det;  // linear interp: Z = (x-b0)*Dx+(y-b1)*Dy+b2
  double Dy = (Ax*Cz-Az*Cx)/det; 
  int ix, Xlimit; 

  if (iy <= b[1]) // do a_y to b_y range only if iy<=b_y
  {          
    double xy = Ax/Ay;        // NB: here we know Ay = a_y-b_y < 0.

    for (; iy <= b[1] ; iy++) {        // do scanline iy

      int abx = Ceil(b[0] + (float)(xy*(iy-b[1])));  // ix and Xlimit 
      int acx = Ceil(a[0]+(float)(ACxy*(iy-a[1])));  //   for 2 subcases:
      if (det>0) {ix = abx; Xlimit = acx;}  // -triangle right of b
      else {ix = acx; Xlimit = abx;};    // -triangle left of b

      if (ix < Xlimit)
      {
        double Z = b[2] + Dy*(iy-b[1]) + Dx*(ix-b[0]); // starting Z
        for (; ix < Xlimit; ix++) {
          srwriter->write_raster(iy, ix, (int) Z);
          Z += Dx;
        }
      }
    } 
  }

  if (iy <= c[1]) // do b_y to c_y range only if b_y<c_y
  {
    double xy = Cx/Cy;          // NB: know Cy = c_y-b_y > 0.

    for (; iy <= c[1]; iy++) {    // do scanline iy

      int cbx = Ceil(b[0] + (float)(xy*(iy-b[1])));  // ix and Xlimit 
      int cax = Ceil(c[0]+(float)(ACxy*(iy-c[1])));  //   for 2 subcases:
      if (det>0) {ix = cbx; Xlimit = cax;}  // -triangle right of b
      else {ix = cax; Xlimit = cbx;};    // -triangle left of b

      if (ix<=Xlimit)
      {
        double Z = b[2] + Dy*(iy-b[1]) + Dx*(ix-b[0]); // starting Z
        for (; ix < Xlimit; ix++)
        {
          srwriter->write_raster(iy, ix, (int) Z);
          Z += Dx;
        }
      }
    }
  }
}

int main(int argc, char *argv[])
{
  int i;
  bool verbose = false;
  char* file_name_in = 0;
  int keep_classification[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  bool first_only = false;
  bool last_only = false;
  int nrows = 0, ncols = 0; // the number of rows and cols in the output grid
  int nbits = 8; // the number of bits in the interpolated raster values 
  float stepx = 0.0f, stepy = 0.0f; // the real-world raster spacing of the grid
  double llx = -1e30f, lly = -1e30f; // the real-world lower left corner of the grid
  float light [3] = {.5,.5,1};
  int hillside_shading = 1;
  bool latlong = false;
  bool elevation_coloring = false;
  bool false_colors = false;
  float elevation_scale = 0; // zero means no scaling
  const char* buffer = 0;

  SRwriteOpener* srwriteopener = 0;

  GeoProjectionConverter* kml_geo_converter = 0;

  if (argc == 1)
  {
    fprintf(stderr,"las2iso.exe is better run in the command line\n");
    file_name_in = new char[256];
    fprintf(stderr,"enter input file: "); fgets(file_name_in, 256, stdin);
    file_name_in[strlen(file_name_in)-1] = '\0';
    char* file_name_out = new char[256];
    fprintf(stderr,"enter output file: "); fgets(file_name_out, 256, stdin);
    file_name_out[strlen(file_name_out)-1] = '\0';
    srwriteopener = new SRwriteOpener();
    srwriteopener->set_file_name(file_name_out);
  }

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-verbose") == 0)
    {
      verbose = true;
    }
    else if (strcmp(argv[i],"-h") == 0)
    {
      usage();
    }
    else if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      file_name_in = argv[i];
    }
    else if (strncmp(argv[i],"-o",2) == 0)
    {
      if (srwriteopener == 0) srwriteopener = new SRwriteOpener();
      if (strcmp(argv[i],"-o") == 0) // the commandline '-o' followed by the output filename
      {
        i++;
        srwriteopener->set_file_name(argv[i]);
      }
      else // the commandline '-oxxx' specifies the output format 'xxx'
      {
        srwriteopener->set_file_format(&(argv[i][2]));
      }
    }
    else if (strcmp(argv[i],"-keep_classification") == 0 || strcmp(argv[i],"-keep_class") == 0 || strcmp(argv[i],"-keep") == 0)
    {
      i++;
      int j = 0;
      while (keep_classification[j] != -1) j++;
      keep_classification[j] = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-first_only") == 0)
    {
      first_only = true;
    }
    else if (strcmp(argv[i],"-last_only") == 0)
    {
      last_only = true;
    }
    else if (strcmp(argv[i],"-nrows") == 0)
    {
      i++;
      nrows = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-ncols") == 0)
    {
      i++;
      ncols = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-nbits") == 0)
    {
      i++;
      nbits = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-stepx") == 0 || strcmp(argv[i],"-xdim") == 0)
    {
      i++;
      sscanf(argv[i], "%f", &stepx);
    }
    else if (strcmp(argv[i],"-stepy") == 0 || strcmp(argv[i],"-ydim") == 0)
    {
      i++;
      sscanf(argv[i], "%f", &stepy);
    }
    else if (strcmp(argv[i],"-step") == 0 || strcmp(argv[i],"-stepxy") == 0 || strcmp(argv[i],"-xydim") == 0)
    {
      i++;
      sscanf(argv[i], "%f", &stepx);
      stepy = stepx;
    }
    else if (strcmp(argv[i],"-llx") == 0 || strcmp(argv[i],"-llxmap") == 0)
    {
      i++;
      sscanf(argv[i], "%lf", &llx);
    }
    else if (strcmp(argv[i],"-lly") == 0 || strcmp(argv[i],"-llymap") == 0)
    {
      i++;
      sscanf(argv[i], "%lf", &lly);
    }
    else if (strcmp(argv[i],"-ll") == 0 || strcmp(argv[i],"-llmap") == 0)
    {
      i++;
      sscanf(argv[i], "%lf", &llx);
      i++;
      sscanf(argv[i], "%lf", &lly);
    }
    else if (strcmp(argv[i],"-nodata") == 0)
    {
      i++;
      if (srwriteopener) srwriteopener->set_nodata_value(atoi(argv[i]));
    }
    else if (strcmp(argv[i],"-shade") == 0 || strcmp(argv[i],"-shaded") == 0 || strcmp(argv[i],"-hillside") == 0 || strcmp(argv[i],"-relief") == 0 || strcmp(argv[i],"-shaded_terrain") == 0)
    {
      hillside_shading += 1;
      elevation_coloring = false;
      false_colors = false;
    }
    else if (strcmp(argv[i],"-elevation_coloring") == 0 || strcmp(argv[i],"-elevation_color") == 0 || strcmp(argv[i],"-elev_color") == 0)
    {
      hillside_shading = 0;
      elevation_coloring = true;
      false_colors = false;
    }
    else if (strcmp(argv[i],"-false_coloring") == 0 || strcmp(argv[i],"-false_color") == 0  || strcmp(argv[i],"-elevation_false") == 0 || strcmp(argv[i],"-elev_false") == 0 || strcmp(argv[i],"-false") == 0)
    {
      hillside_shading = 0;
      elevation_coloring = true;
      false_colors = true;
    }
    else if (strcmp(argv[i],"-elevation") == 0 || strcmp(argv[i],"-elev") == 0)
    {
      hillside_shading = 0;
      elevation_coloring = false;
      false_colors = false;
    }
    else if (strcmp(argv[i],"-elevation_scale") == 0 || strcmp(argv[i],"-elev_scale") == 0)
    {
      i++;
      elevation_scale = (float)atof(argv[i]);
    }
    else if (strcmp(argv[i],"-light") == 0)
    {
      i++;
      sscanf(argv[i], "%f", &(light[0]));
      i++;
      sscanf(argv[i], "%f", &(light[1]));
      i++;
      sscanf(argv[i], "%f", &(light[2]));
    }
    else if (strcmp(argv[i],"-buffer") == 0)
    {
      i++;
      buffer = argv[i];
    }
    else if (strcmp(argv[i],"-kill_threshold") == 0 || strcmp(argv[i],"-kill") == 0)
    {
      i++;
      kill_threshold_squared = (float)(atof(argv[i])*atof(argv[i]));
    }
    else if (strcmp(argv[i],"-zone") == 0 || strcmp(argv[i],"-utm") == 0 || strcmp(argv[i],"-utm_zone") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      char tmp[256];
      if (kml_geo_converter->set_utm_projection(argv[++i], tmp))
      {
        fprintf(stderr, "using UTM zone '%s'\n",tmp);
      }
      else
      {
        fprintf(stderr, "ERROR: utm zone '%s' is unknown. use a format such as '11S' or '10T'\n",argv[i]);
        exit(1);
      }
    }
    else if (strcmp(argv[i],"-latlong") == 0 || strcmp(argv[i],"-latlon") == 0)
    {
      latlong = true;
    }
    else if (strcmp(argv[i],"-ellipsoid") == 0 || strcmp(argv[i],"-ellipse") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      char tmp[256];
      int ellipsoid_id = atoi(argv[++i]);
      if (kml_geo_converter->set_reference_ellipsoid(ellipsoid_id,tmp))
      {
        fprintf(stderr, "using ellipsoid '%s'\n",tmp);
      }
      else
      {
        fprintf(stderr, "ERROR: ellipsoid %d is unknown. use one of those: \n", ellipsoid_id);
        ellipsoid_id = 1;
        while (kml_geo_converter->set_reference_ellipsoid(ellipsoid_id++, tmp))
        {
          fprintf(stderr, "  %s\n", tmp);
        }
        exit(1);
      }
    }
    else if (strcmp(argv[i],"-lcc") == 0 || strcmp(argv[i],"-lambert") == 0 || strcmp(argv[i],"-lambert_conic_conformal") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      double falseEasting; sscanf(argv[i+1], "%lf", &falseEasting);
      double falseNorthing; sscanf(argv[i+2], "%lf", &falseNorthing);
      if (strcmp(argv[i+3],"survey_feet") == 0 || strcmp(argv[i+3],"survey_foot") == 0 || strcmp(argv[i+3],"surveyfeet") == 0 || strcmp(argv[i+3],"surveyfoot") == 0 || strcmp(argv[i+3],"surveyft") == 0 || strcmp(argv[i+3],"sft") == 0)
      {
        falseEasting *= 0.3048006096012;
        falseNorthing *= 0.3048006096012;
      }
      else if (strcmp(argv[i+3],"feet") == 0 || strcmp(argv[i+3],"foot") == 0 || strcmp(argv[i+3],"ft") == 0)
      {
        falseEasting *= 0.3048;
        falseNorthing *= 0.3048;
      }
      else if (strcmp(argv[i+3],"meters") && strcmp(argv[i+3],"meter") && strcmp(argv[i+3],"met") && strcmp(argv[i+3],"m"))
      {
        fprintf(stderr,"ERROR: wrong options for '%s'. use like shown in these examples:\n",argv[i]);
        fprintf(stderr,"  %s 609601.22 0.0 meter 33.75 -79 34.33333 36.16666\n",argv[i]);
        fprintf(stderr,"  %s 609601.22 0.0 m 33.75 -79 34.33333 36.16666\n",argv[i]);
        fprintf(stderr,"  %s 1640416.666667 0.0 surveyfeet 47.000000 -120.833333 47.50 48.733333\n",argv[i]);
        fprintf(stderr,"  %s 1640416.666667 0.0 sft 47.000000 -120.833333 47.50 48.733333\n",argv[i]);
        fprintf(stderr,"  %s 1804461.942257 0.0 feet 0.8203047 -2.1089395 47.50 48.733333\n",argv[i]);
        fprintf(stderr,"  %s 1804461.942257 0.0 ft 0.8203047 -2.1089395 47.50 48.733333\n",argv[i]);
      }
      double latOfOriginDeg; sscanf(argv[i+4], "%lf", &latOfOriginDeg);
      double longOfOriginDeg; sscanf(argv[i+5], "%lf", &longOfOriginDeg);
      double firstStdParallelDeg; sscanf(argv[i+6], "%lf", &firstStdParallelDeg);
      double secondStdParallelDeg; sscanf(argv[i+7], "%lf", &secondStdParallelDeg);
      char tmp[256];
      if (kml_geo_converter->set_lambert_conformal_conic_projection(falseEasting, falseNorthing, latOfOriginDeg, longOfOriginDeg, firstStdParallelDeg, secondStdParallelDeg, tmp))
      {
        fprintf(stderr, "using LCC projection: '%s'\n",tmp);
      }
      else
      {
        fprintf(stderr, "ERROR: bad parameters for '%s'.\n",argv[i]);
        exit(1);
      }
      i+=7;
    }
    else if (strcmp(argv[i],"-sp") == 0 || strcmp(argv[i],"-sp83") == 0 || strcmp(argv[i],"-stateplane") == 0 || strcmp(argv[i],"-stateplane83") == 0 || strcmp(argv[i],"-state_plane") == 0 || strcmp(argv[i],"-state_plane83") == 0)
    {
      char tmp[256];
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      if (kml_geo_converter->set_state_plane_nad83_lcc(argv[i+1], tmp))
      {
        fprintf(stderr, "using state plane '%s' (NAD83 LCC) '%s'\n",argv[i+1],tmp);
      }
      else if (kml_geo_converter->set_state_plane_nad83_tm(argv[i+1], tmp))
      {
        fprintf(stderr, "using state plane '%s' (NAD83 TM) '%s'\n",argv[i+1],tmp);
      }
      else
      {
        fprintf(stderr, "ERROR: bad state code in '%s %s'.\n",argv[i],argv[i+1]);
        kml_geo_converter->print_all_state_plane_nad83_lcc();
        kml_geo_converter->print_all_state_plane_nad83_tm();
        exit(1);
      }
      i++;
    }
    else if (strcmp(argv[i],"-sp27") == 0 || strcmp(argv[i],"-stateplane27") == 0 || strcmp(argv[i],"-state_plane27") == 0)
    {
      char tmp[256];
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      if (kml_geo_converter->set_state_plane_nad27_lcc(argv[i+1], tmp))
      {
        fprintf(stderr, "using state plane '%s' (NAD27 LCC) '%s'\n",argv[i+1],tmp);
      }
      else if (kml_geo_converter->set_state_plane_nad27_tm(argv[i+1], tmp))
      {
        fprintf(stderr, "using state plane '%s' (NAD27 TM) '%s'\n",argv[i+1],tmp);
      }
      else
      {
        fprintf(stderr, "ERROR: bad state code in '%s %s'.\n",argv[i],argv[i+1]);
        kml_geo_converter->print_all_state_plane_nad27_lcc();
        kml_geo_converter->print_all_state_plane_nad27_tm();
        exit(1);
      }
      i++;
    }
    else if (strcmp(argv[i],"-tm") == 0 || strcmp(argv[i],"-traverse") == 0 || strcmp(argv[i],"-traverse_mercator") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      double falseEasting; sscanf(argv[i+1], "%lf", &falseEasting);
      double falseNorthing; sscanf(argv[i+2], "%lf", &falseNorthing);
      if (strcmp(argv[i+3],"survey_feet") == 0 || strcmp(argv[i+3],"survey_foot") == 0 || strcmp(argv[i+3],"surveyfeet") == 0 || strcmp(argv[i+3],"surveyfoot") == 0 || strcmp(argv[i+3],"surveyft") == 0 || strcmp(argv[i+3],"sft") == 0)
      {
        falseEasting *= 0.3048006096012;
        falseNorthing *= 0.3048006096012;
      }
      else if (strcmp(argv[i+3],"feet") == 0 || strcmp(argv[i+3],"foot") == 0 || strcmp(argv[i+3],"ft") == 0)
      {
        falseEasting *= 0.3048;
        falseNorthing *= 0.3048;
      }
      else if (strcmp(argv[i+3],"meters") && strcmp(argv[i+3],"meter") && strcmp(argv[i+3],"met") && strcmp(argv[i+3],"m"))
      {
        fprintf(stderr,"ERROR: wrong options for '%s'. use like shown in these examples:\n",argv[i]);
        fprintf(stderr,"  %s 609601.22 0.0 meter 33.75 -79 0.99996\n",argv[i]);
        fprintf(stderr,"  %s 609601.22 0.0 m 33.75 -79 0.99996\n",argv[i]);
        fprintf(stderr,"  %s 1640416.666667 0.0 surveyfeet 47.000000 -120.833333 0.99996\n",argv[i]);
        fprintf(stderr,"  %s 1640416.666667 0.0 sft 47.000000 -120.833333 0.99996\n",argv[i]);
        fprintf(stderr,"  %s 1804461.942257 0.0 feet 0.8203047 -2.1089395 0.99996\n",argv[i]);
        fprintf(stderr,"  %s 1804461.942257 0.0 ft 0.8203047 -2.1089395 0.99996\n",argv[i]);
        exit(1);
      }
      double latOriginDeg; sscanf(argv[i+4], "%lf", &latOriginDeg);
      double longMeridianDeg; sscanf(argv[i+5], "%lf", &longMeridianDeg);
      double scaleFactor; sscanf(argv[i+6], "%lf", &scaleFactor);
      char tmp[256];
      if (kml_geo_converter->set_transverse_mercator_projection(falseEasting, falseNorthing, latOriginDeg, longMeridianDeg, scaleFactor, tmp))
      {
        fprintf(stderr, "using TM projection: '%s'\n",tmp);
      }
      else
      {
        fprintf(stderr, "ERROR: bad parameters for '%s'.\n",argv[i]);
        exit(1);
      }
      i+=6;
    }
    else if (strcmp(argv[i],"-survey_feet") == 0 || strcmp(argv[i],"-survey_foot") == 0 || strcmp(argv[i],"-surveyfeet") == 0 || strcmp(argv[i],"-surveyfoot") == 0 || strcmp(argv[i],"-surveyft") == 0 || strcmp(argv[i],"-sft") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      kml_geo_converter->set_coordinates_in_survey_feet();
    }
    else if (strcmp(argv[i],"-feet") == 0 || strcmp(argv[i],"-foot") == 0 || strcmp(argv[i],"-ft") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      kml_geo_converter->set_coordinates_in_feet();
    }
    else if (strcmp(argv[i],"-meter") == 0 || strcmp(argv[i],"-met") == 0 || strcmp(argv[i],"-m") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      kml_geo_converter->set_coordinates_in_meter();
    }
    else if (strcmp(argv[i],"-elevation_feet") == 0 || strcmp(argv[i],"-elevation_foot") == 0 || strcmp(argv[i],"-elevation_ft") == 0 || strcmp(argv[i],"-elev_feet") == 0 || strcmp(argv[i],"-elev_foot") == 0 || strcmp(argv[i],"-elev_ft") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      kml_geo_converter->set_elevation_in_feet();
    }
    else if (strcmp(argv[i],"-elevation_meters") == 0 || strcmp(argv[i],"-elevation_meter") == 0 || strcmp(argv[i],"-elevation_m") == 0 || strcmp(argv[i],"-elev_meters") == 0 || strcmp(argv[i],"-elev_meter") == 0 || strcmp(argv[i],"-elev_m") == 0)
    {
      if (kml_geo_converter == 0) kml_geo_converter = new GeoProjectionConverter();
      kml_geo_converter->set_elevation_in_meter();
    }
    else if (strcmp(argv[i],"-tiling_ns") == 0 || strcmp(argv[i],"-tiles_ns") == 0)
    {
      if (srwriteopener == 0) srwriteopener = new SRwriteOpener();
      srwriteopener->set_tiling(argv[i+1], atoi(argv[i+2]));
      i+=2;
    }
    else if (strcmp(argv[i],"-quality") == 0 || strcmp(argv[i],"-compress") == 0 || strcmp(argv[i],"-compression") == 0)
    {
      if (srwriteopener == 0) srwriteopener = new SRwriteOpener();
      srwriteopener->set_compression_quality(atoi(argv[i+1]));
      i++;
    }
    else if (i == argc - 2 && file_name_in == 0 && srwriteopener == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in == 0 && srwriteopener == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in != 0 && srwriteopener == 0)
    {
      if (srwriteopener == 0) srwriteopener = new SRwriteOpener();
      srwriteopener->set_file_name(argv[i]);
    }
    else
    {
      fprintf(stderr,"cannot understand argument '%s'\n", argv[i]);
      usage();
    }
  }

  // open input file

  FILE* file_in;
  if (file_name_in)
  {
    if (strstr(file_name_in, ".gz"))
    {
#ifdef _WIN32
      file_in = fopenGzipped(file_name_in, "rb");
#else
      fprintf(stderr, "ERROR: no support for gzipped input\n");
      exit(1);
#endif
    }
    else
    {
      file_in = fopen(file_name_in, "rb");
    }
    if (file_in == 0)
    {
      fprintf(stderr, "ERROR: could not open '%s'\n",file_name_in);
      byebye(argc==1);
    }
  }
  else
  {
    fprintf(stderr, "ERROR: no input specified\n");
    usage(argc==1);
  }

  double coordinates[3];
  LASreader* lasreader = new LASreader();

  if (lasreader->open(file_in) == false)
  {
    fprintf(stderr, "ERROR: could not open lasreader\n");
    byebye(argc==1);
  }

  // open output raster

  if (srwriteopener == 0)
  {
    fprintf(stderr, "ERROR: no output specified\n");
    usage(argc==1);
  }

  if (kml_geo_converter)
  {
    // if we have a geo converter make sure we have some projection
    if (kml_geo_converter->get_ellipsoid_name() == 0)
    {
      char description[256];
      kml_geo_converter->set_reference_ellipsoid(23,description);
      fprintf(stderr, "WARNING: using default ellipsoid '%s'\n",description);
    }
    if (kml_geo_converter->get_projection_name() == 0)
    {
      char description[256];
      kml_geo_converter->set_utm_projection("11T", description);
      fprintf(stderr, "WARNING: using default UTM projection '%s'\n",description);
    }
    srwriteopener->set_kml_geo_converter(kml_geo_converter); 
  }
  else if (lasreader->header.vlr_geo_keys)
  {
    // otherwise use the georeferencing information in the LAS header
    kml_geo_converter = new GeoProjectionConverter();
    if (kml_geo_converter->set_projection_from_geo_keys(lasreader->header.vlr_geo_keys[0].number_of_keys, (GeoProjectionConverterGeoKeys*)lasreader->header.vlr_geo_key_entries, lasreader->header.vlr_geo_ascii_params, lasreader->header.vlr_geo_double_params))
    {
      srwriteopener->set_kml_geo_converter(kml_geo_converter); 
    }
    else
    {
      delete kml_geo_converter;
      kml_geo_converter = 0;
    }
  }
  else if (latlong)
  {
    srwriteopener->set_kml_geo_converter((GeoProjectionConverter *)1); 
  }

  if (!(srwriter = srwriteopener->open()))
  {
    usage(argc==1);
  }
  
  int npoints = lasreader->npoints;

  // make a first pass over the points if we only triangulate a subset

  if (first_only || last_only || keep_classification[0] != -1)
  {
    int eliminated_first_only = 0;
    int eliminated_last_only = 0;
    int eliminated_classification = 0;
  
    fprintf(stderr, "extra pass reading %d points to determine which we keep ...\n", lasreader->npoints);
    if (verbose) ptime("start extra pass.");

    while (lasreader->read_point())
    {
      // put the points through all the tests
      if (last_only && lasreader->point.return_number != lasreader->point.number_of_returns_of_given_pulse)
      {
        eliminated_last_only++;
        continue;
      }
      if (first_only && lasreader->point.return_number != 1)
      {
        eliminated_first_only++;
        continue;
      }
      if (keep_classification[0] != -1)
      {
        int j = 0;
        while (keep_classification[j] != -1 && lasreader->point.classification != keep_classification[j]) j++;
        if (keep_classification[j] == -1)
        {
          eliminated_classification++;
          continue;
        }
      }
    }

    if (verbose) ptime("done extra pass.");

    // subtract eliminated points
    npoints = npoints - (eliminated_first_only + eliminated_last_only + eliminated_classification);
 
    if (eliminated_first_only) fprintf(stderr, "eliminated based on first returns only: %d\n", eliminated_first_only);
    if (eliminated_last_only) fprintf(stderr, "eliminated based on last returns only: %d\n", eliminated_last_only);
    if (eliminated_classification) fprintf(stderr, "eliminated based on classification: %d\n", eliminated_classification);

    // close the input file
    lasreader->close();
    fclose(file_in);

    // re-open input file
    if (strstr(file_name_in, ".gz"))
    {
#ifdef _WIN32
      file_in = fopenGzipped(file_name_in, "rb");
#else
      fprintf(stderr, "ERROR: no support for gzipped input\n");
      exit(1);
#endif
    }
    else
    {
      file_in = fopen(file_name_in, "rb");
    }
    if (file_in == 0)
    {
      fprintf(stderr, "ERROR: could not re-open '%s'\n",file_name_in);
      byebye(argc==1);
    }
    if (lasreader->open(file_in) == false)
    {
      fprintf(stderr, "ERROR: could not re-open lasreader\n");
      byebye(argc==1);
    }  
  }

  // loop over points, store points, and triangulate

  int count = 0;
  float* point_buffer = (float*)malloc(sizeof(float)*3*npoints);
  
  TINclean(npoints);

  fprintf(stderr, "reading %d points and triangulating %d points\n", lasreader->npoints, npoints);
  if (verbose) ptime("start triangulation pass.");

  while (lasreader->read_point(coordinates))
  {
    // put the points through all the tests
    if (last_only && lasreader->point.return_number != lasreader->point.number_of_returns_of_given_pulse)
    {
      continue;
    }
    if (first_only && lasreader->point.return_number != 1)
    {
      continue;
    }
    if (keep_classification[0] != -1)
    {
      int j = 0;
      while (keep_classification[j] != -1 && lasreader->point.classification != keep_classification[j]) j++;
      if (keep_classification[j] == -1) continue;
    }
    // add point to the point buffer while subtracting the min bounding box for x and y
    point_buffer[3*count+0] = (float)(coordinates[0] - lasreader->header.min_x);
    point_buffer[3*count+1] = (float)(coordinates[1] - lasreader->header.min_y);
    point_buffer[3*count+2] = (float)(coordinates[2]);
    // add the point to the triangulation
    TINadd(&(point_buffer[3*count]));
    count++;
  }
  TINfinish();

  lasreader->close();
  fclose(file_in);

  // set grid parameters

  if (llx == -1e30f)
  {
    llx = lasreader->header.min_x;
    fprintf(stderr,"lower left x unspecified. setting it to %g.\n", llx);
  }

  if (lly == -1e30f)
  {
    lly = lasreader->header.min_y;
    fprintf(stderr,"lower left y unspecified. setting it to %g.\n", lly);
  }

  if (ncols == 0)
  {
    if (stepx == 0)
    {
      stepx = 1.0f;
      fprintf(stderr,"step size x was unspecified. we set it to %g.\n", stepx);
    }
    if (llx < lasreader->header.max_x)
    {
      if (llx < lasreader->header.min_x) fprintf(stderr,"WARNING: lower left x lies left of the TIN's bounding box.\n");
      ncols = (int)((lasreader->header.max_x - llx) / stepx);
    }
    else
    {
      fprintf(stderr,"WARNING: lower left x lies right of the TIN's bounding box.\n");
      ncols = 1;
    }
    if (ncols & 1) ncols++;
    fprintf(stderr,"ncols was unspecified. we set it to %d.\n", ncols);
  }
  else
  {
    if (stepx == 0)
    {
      if (llx < lasreader->header.max_x)
      {
        if (llx < lasreader->header.min_x) fprintf(stderr,"WARNING: lower left x lies left of the TIN's bounding box.\n");
        stepx = (float)((lasreader->header.max_x - llx) / ncols);
      }
      else
      {
        fprintf(stderr,"WARNING: lower left x lies right of the TIN's bounding box.\n");
        stepx = 1;
      }
      fprintf(stderr,"step size x was unspecified. we set it to %g.\n", stepx);
    }
  }
  if (nrows == 0)
  {
    if (stepy == 0)
    {
      stepy = 1.0f;
      fprintf(stderr,"step size y was unspecified. we set it to %g.\n", stepy);
    }
    if (lly < lasreader->header.max_y)
    {
      if (lly < lasreader->header.min_y) fprintf(stderr,"WARNING: lower left y lies below the TIN's bounding box.\n");
      nrows = (int)((lasreader->header.max_y - lly) / stepy);
    }
    else
    {
      fprintf(stderr,"WARNING: lower left y lies above the TIN's bounding box.\n");
      nrows = 1;
    }
    if (nrows & 1) nrows++;
    fprintf(stderr,"nrows was unspecified. we set it to %d.\n", nrows);
  }
  else
  {
    if (stepy == 0)
    {
      if (lly < lasreader->header.max_y)
      {
        if (lly < lasreader->header.min_y) fprintf(stderr,"WARNING: lower left y lies below of the TIN's bounding box.\n");
        stepy = (float)((lasreader->header.max_y - lly) / nrows);
      }
      else
      {
        fprintf(stderr,"WARNING: lower left y lies above the TIN's bounding box.\n");
        stepy = 1;
      }
      fprintf(stderr,"step size y was unspecified. we set it to %g.\n", stepy);
    }
  }

  if (strcmp(srwriteopener->file_format, "bil") == 0)
  {
    if (nbits != 16 && nbits != 32)
    {
      fprintf(stderr,"WARNING: nbits set to 16 bits for BIL output.\n");
      nbits = 16;
    }
    if (hillside_shading == 1) // unless hillside shading was explicitely set we disable it
    {
      hillside_shading = 0;
    }
    if (false_colors)
    {
      fprintf(stderr,"WARNING: false_colors not supported for BIL output.\n");
      false_colors = false;
    }
  }

  srwriter->set_nrows(nrows);
  srwriter->set_ncols(ncols);
  srwriter->set_nbands(false_colors ? 3 : 1);
  srwriter->set_nbits(false_colors ? 8 : nbits);
  srwriter->set_lower_left(llx, lly);
  srwriter->set_step_size(stepx, stepy);

  if (buffer)
  {
    if (strcmp(buffer,"simple") == 0)
    {
      srwriter->set_buffer(new SRbufferSimple()); // simple uncompressed
    }
    else if (strcmp(buffer,"inmemory") == 0 || strcmp(buffer,"memory") == 0)
    {
      srwriter->set_buffer(new SRbufferInMemory()); // all rasters are kept in main memory
    }
    else if (strcmp(buffer,"rows") == 0)
    {
      srwriter->set_buffer(new SRbufferRows()); // one 64 raster buffer per row and one temp file
    }
    else if (strcmp(buffer,"tiles") == 0)
    {
      srwriter->set_buffer(new SRbufferTiles()); // one 4096 raster buffer and one temp file
    }
    else if (strcmp(buffer,"rowbands") == 0)
    {
      srwriter->set_buffer(new SRbufferRowBands()); // one 64 raster buffer per row one temp file for each 64 rows
    }
    else
    {
      fprintf(stderr,"WARNING: buffering method '%s' is unknown ...\n", buffer);
    }
  }
  else
  {
    // for large rasters use a disk buffer
    if (nrows * ncols > 2048*2048)
    {
      srwriter->set_buffer(new SRbufferRowBands()); // GIS 2006 paper
    }
    else
    {
      srwriter->set_buffer(new SRbufferInMemory()); // for small images
    }
  }

#ifdef _WIN32
  // increase number of files that may be open at the same time to 2048
  _setmaxstdio(2048);
#endif

  srwriter->write_header();

  // maybe generate normals

  float* point_normals = 0;

  if (hillside_shading)
  {
    // compute per vertex normals
    point_normals = (float*)malloc(sizeof(float)*3*npoints);
    for (i = 0; i < npoints; i++)
    {
      VecZero3fv(&point_normals[i*3]);
    }
    // loop over the triangles and add their normals to the vertices
    float normal[3];
    TINtriangle* t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          // compute normal
          VecCcwNormNormal3fv(normal, t->V[0], t->V[1], t->V[2]);
          // add normal to all three vertices
          for (i = 0; i < 3; i++)
          {
            VecSelfAdd3fv(&(point_normals[t->V[i]-point_buffer]), normal);
          }
        }
      }
    }
    VecSelfNormalize3fv(light);
    for (i = 0; i < npoints; i++)
    {
      VecSelfNormalize3fv(&point_normals[i*3]);
      point_buffer[3*i+2] = (float)fabs(VecDotProd3fv(&point_normals[i*3], light))*255;
    }
    free(point_normals);
  }

  // loop over the vertices, convert to raster coordinates, and generate the clip codes

  char* point_clip_code = (char*)malloc(sizeof(char)*npoints);

  for (i = 0; i < npoints; i++)
  {
    coordinates[0] = lasreader->header.min_x + point_buffer[3*i+0];
    coordinates[1] = lasreader->header.min_y + point_buffer[3*i+1];
    coordinates[2] = point_buffer[3*i+2];
    srwriter->world_to_raster(coordinates, &(point_buffer[3*i+0]));
    point_clip_code[i] = (point_buffer[3*i+0] < 0) | ((point_buffer[3*i+1] < 0) << 1) | ((point_buffer[3*i+0] >= srwriter->ncols) << 2) | ((point_buffer[3*i+1] >= srwriter->nrows) << 3);

    if (elevation_coloring)
    {
      if (false_colors)
      {
        point_buffer[3*i+2] = (float)((point_buffer[3*i+2] - lasreader->header.min_z)/(lasreader->header.max_z - lasreader->header.min_z)*16777216);
      }
      else
      {
        point_buffer[3*i+2] = (float)((point_buffer[3*i+2] - lasreader->header.min_z)/(lasreader->header.max_z - lasreader->header.min_z)*255);
      }
    }
    else if (elevation_scale)
    {
      point_buffer[3*i+2] *= elevation_scale;
    }  
  }

  // loop over the triangles and rasterize

  fprintf(stderr, "rastering the triangles ...\n");
  if (verbose) ptime("start output.");

  TINtriangle* t = TINget_triangle(0);
  for (count = 0; count < TINget_size(); count++, t++)
  {
    if (t->next < 0) // if not deleted
    {
      if (t->V[0]) // if not infinite
      {
        if (point_clip_code[(t->V[0]-point_buffer)/3] & point_clip_code[(t->V[1]-point_buffer)/3] & point_clip_code[(t->V[2]-point_buffer)/3])
        {
          continue;
        }
        raster_triangle(t->V[0], t->V[1], t->V[2]);
      }
    }
  }

  srwriter->close();
  delete srwriter;
  delete srwriteopener;

  if (verbose) ptime("done.");

  byebye(argc==1);

  return 0;
}
