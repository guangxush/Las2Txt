/*
===============================================================================

  FILE:  srwritetiled.cpp
  
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
#include "srwritetiled.h"

#include "srbuffersimple.h"

#include <stdlib.h>
#include <string.h>

void SRwriteTiled::set_file_name_base(const char* file_name_base)
{
  if (this->file_name_base) free(this->file_name_base);
  this->file_name_base = strdup(file_name_base);
}

void SRwriteTiled::set_tile_size(int tile_size)
{
  this->tile_size = tile_size;
}

void SRwriteTiled::create_kml_overview(GeoProjectionConverter* kml_geo_converter)
{
  this->kml_geo_converter = kml_geo_converter;
}

bool SRwriteTiled::open(SRwriteOpener* srwriteopener)
{
  if (srwriteopener == 0)
  {
    fprintf(stderr,"ERROR: no srwriteopener specified\n");
    return false;
  }
  
  if (srwriteopener->file_format == 0)
  {
    fprintf(stderr,"ERROR: no output file format specified\n");
    return false;
  }

  this->srwriteopener = srwriteopener;
  return true;
}

bool SRwriteTiled::open_tile_file(int tile, int x, int y)
{
  char file_name[256];  
  sprintf(file_name, "%s_%0.3d_%0.3d.%s", file_name_base, x, y, srwriteopener->file_format);
  srwriteopener->set_file_name(file_name);
  if (!(tile_writers[tile] = srwriteopener->open()))
  {
    exit(1);
  }
  tile_writers[tile]->set_nrows(tile_size);
  tile_writers[tile]->set_ncols(tile_size);
  tile_writers[tile]->set_nbits(nbits);
  tile_writers[tile]->set_nbands(nbands);
  tile_writers[tile]->set_step_size(stepx, stepy);
  tile_writers[tile]->set_lower_left(llx + x*tile_size*stepx, lly + y*tile_size*stepy);
  // should be SRbufferTiles once this is implemented correctly
  // maybe we could use memory buffering for small tilings
  SRbufferSimple* srbuffersimple = new SRbufferSimple();
  sprintf(file_name, "%s_%0.3d_%0.3d.tmp", file_name_base, x, y);
  srbuffersimple->set_file_name(file_name);
  tile_writers[tile]->set_buffer(srbuffersimple);

  tile_writers[tile]->write_header();

  if (kml_overview_file)
  {
    double point[3];
    double lt,lg;
    float elev;
    fprintf(kml_overview_file, " <NetworkLink>\012");
    fprintf(kml_overview_file, "  <name>tile %3d %3d   (%d x %d %s)</name>\012",x, y,((x+1==tiles_x)&&(ncols%tile_size))?(int)((ncols%tile_size)*stepx+.5):(int)(tile_size*stepx+.5),((y+1==tiles_y)&&(nrows%tile_size))?(int)((nrows%tile_size)*stepy+.5):(int)(tile_size*stepy+.5),kml_geo_converter->get_coordinate_unit_description_string(false));
    fprintf(kml_overview_file, "  <Region>\012");
    fprintf(kml_overview_file, "  <LatLonAltBox>\012");
    point[0] = llx + x*tile_size*stepx; point[1] = lly + y*tile_size*stepy;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  <south>%.10lf</south> <west>%.10lf</west>\012",lt,lg);
    point[0] = llx + (x+1)*tile_size*stepx; point[1] = lly +(y+1)*tile_size*stepy;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  <north>%.10lf</north> <east>%.10lf</east>\012",lt,lg);  
    fprintf(kml_overview_file, "  </LatLonAltBox>\012");
    fprintf(kml_overview_file, "  <Lod><minLodPixels>%d</minLodPixels><minFadeExtent>%d</minFadeExtent></Lod>\012",tile_size/2,tile_size/10);
    fprintf(kml_overview_file, "  </Region>\012");
    fprintf(kml_overview_file, "  <Link>\012");
    fprintf(kml_overview_file, "  <href>%s_%0.3d_%0.3d.kml</href>\012", file_name_base, x, y);
    fprintf(kml_overview_file, "  </Link>\012");
    fprintf(kml_overview_file, "  <viewRefreshMode>onRegion</viewRefreshMode>\012");
    fprintf(kml_overview_file, "  </NetworkLink>\012");

    fprintf(kml_overview_file, " <Placemark><styleUrl>#yellow_thin</styleUrl>\012");
    fprintf(kml_overview_file, "  <Region>\012");
    fprintf(kml_overview_file, "  <LatLonAltBox>\012");
    point[0] = llx + x*tile_size*stepx; point[1] = lly + y*tile_size*stepy;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  <south>%.10lf</south> <west>%.10lf</west>\012",lt,lg);
    point[0] = llx + (x+1)*tile_size*stepx; point[1] = lly + (y+1)*tile_size*stepy;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  <north>%.10lf</north> <east>%.10lf</east>\012",lt,lg);  
    fprintf(kml_overview_file, "  </LatLonAltBox>\012");
    fprintf(kml_overview_file, "  <Lod><minLodPixels>16</minLodPixels><maxLodPixels>%d</maxLodPixels><minFadeExtent>16</minFadeExtent><maxFadeExtent>%d</maxFadeExtent></Lod>\012",tile_size, tile_size/5);
    fprintf(kml_overview_file, "  </Region>\012");
    fprintf(kml_overview_file, "  <LineString><altitudeMode>clampToGround</altitudeMode><coordinates>\012");
    point[0] = llx + x*tile_size*stepx; point[1] = lly + y*tile_size*stepy;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
    point[0] = llx + (x+1)*tile_size*stepx; if (point[0] > urx) point[0] = urx; point[1] = lly + y*tile_size*stepy;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
    point[0] = llx + (x+1)*tile_size*stepx; if (point[0] > urx) point[0] = urx; point[1] = lly + (y+1)*tile_size*stepy; if (point[1] > ury) point[1] = ury;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
    point[0] = llx + x*tile_size*stepx; point[1] = lly + (y+1)*tile_size*stepy; if (point[1] > ury) point[1] = ury;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
    point[0] = llx + x*tile_size*stepx; point[1] = lly + y*tile_size*stepy;
    kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
    fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
    fprintf(kml_overview_file, "  </coordinates></LineString>\012");
    fprintf(kml_overview_file, "  </Placemark>\012");
  }

  return true;
}

void SRwriteTiled::write_header()
{
  urx = llx+stepx*ncols;
  ury = lly+stepy*nrows;

  if (tile_size <= 0)
  {
    tile_size = 1024;
  }
  tiles_x = ncols / tile_size + (ncols % tile_size ? 1 : 0);
  tiles_y = nrows / tile_size + (nrows % tile_size ? 1 : 0);

  tile_writers = new SRwriter*[tiles_x * tiles_y];
  for (int i = 0; i < tiles_x * tiles_y; i++) tile_writers[i] = 0;

  r_count = 0;
  r_clipped = 0;

  if (kml_geo_converter)
  {
    char file_name[256];
    sprintf(file_name, "%s.kml", file_name_base);
    kml_overview_file = fopen(file_name, "w");
    if (kml_overview_file == 0)
    {
      fprintf(stderr, "WARNING: cannot open kml overview file '%s' for write\n", file_name);
    }
    else
    {
      double point[3];
      double lt,lg;
      float elev;
      point[0] = llx; point[1] = lly;
      kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
      fprintf(kml_overview_file, "<kml>\012");
      fprintf(kml_overview_file, " <Document>\012");
      fprintf(kml_overview_file, " <name>%d x %d tiling with %d x %d %s</name>\012",tiles_x,tiles_y,(int)(stepx*tile_size+.5),(int)(stepy*tile_size+.5),kml_geo_converter->get_coordinate_unit_description_string(false));
      fprintf(kml_overview_file, " <Style id=\"popup\"><BalloonStyle><text>$[description]</text></BalloonStyle></Style>\012");
      fprintf(kml_overview_file, " <Style id=\"yellow_thick\"><LineStyle><color>7f00ff88</color><width>2</width></LineStyle></Style>\012");  
      fprintf(kml_overview_file, " <Style id=\"yellow_thin\"><LineStyle><color>7f00ff88</color><width>1</width></LineStyle></Style>\012");  
      fprintf(kml_overview_file, "  <Placemark>\012");
      fprintf(kml_overview_file, "   <styleUrl>popup</styleUrl>\012");
      fprintf(kml_overview_file, "    <name>%d x %d tiling with %d by %d %s tiles</name>\012",tiles_x,tiles_y,(int)(stepx*tile_size+.5),(int)(stepy*tile_size+.5),kml_geo_converter->get_coordinate_unit_description_string(false));
      fprintf(kml_overview_file, "   <description><![CDATA[lon: %4.10f<BR>lat: %4.10f<BR>tile: %d %s / %d pixel<BR><BR>original georeference:<BR>E%.2fk N%.2fk<BR>%s<BR>%s<BR><BR>The software (including source code) to generate such raster DEM tilings from LIDAR points is available <a href=\"http://www.cs.unc.edu/~isenburg/googleearth/\">here</a>.]]></description>\012",lg,lt,(int)(stepx*tile_size+.5),kml_geo_converter->get_coordinate_unit_description_string(false),tile_size,(point[0]/1000),(point[1]/1000),kml_geo_converter->get_projection_name(),kml_geo_converter->get_ellipsoid_name());
      fprintf(kml_overview_file, "   <Point><coordinates>%4.10f,%4.10f,0</coordinates></Point>\012",lg,lt);
      fprintf(kml_overview_file, "   </Placemark>\012");
      fprintf(kml_overview_file, " <Placemark><styleUrl>#yellow_thick</styleUrl>\012");
      fprintf(kml_overview_file, "  <LineString><altitudeMode>clampToGround</altitudeMode><coordinates>\012");
      fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
      point[0] = urx; point[1] = lly;
      kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
      fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
      point[0] = urx; point[1] = ury;
      kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
      fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
      point[0] = llx; point[1] = ury;
      kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
      fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
      point[0] = llx; point[1] = lly;
      kml_geo_converter->to_kml_style_lat_long_elevation(point,lt,lg,elev);
      fprintf(kml_overview_file, "  %.10lf,%.10lf,0\012",lg,lt);
      fprintf(kml_overview_file, "  </coordinates></LineString>\012");
      fprintf(kml_overview_file, "  </Placemark>\012");
    }
  }
}

void SRwriteTiled::write_raster(int row, int col, int value)
{
  if (row < 0 || col < 0 || row >= nrows || col >= ncols)
  {
    r_clipped++;
    return;
  }
  int x = col / tile_size;
  int y = (nrows - row) / tile_size;
  int tile = y * tiles_x + x;
  if (tile_writers[tile] == 0) // do we need to create a writer for this tile
  {
    open_tile_file(tile, x, y);
  }
  tile_writers[tile]->write_raster(tile_size-((nrows-row)%tile_size)-1,col%tile_size,value);
}

void SRwriteTiled::write_raster(int value)
{
  int row = r_count/nrows;
  int col = r_count%nrows;
  int x = col / tile_size;
  int y = (nrows - row) / tile_size;
  int tile = y * tiles_x + x;
  if (tile_writers[tile] == 0) // do we need to create a writer for this tile
  {
    open_tile_file(tile,x,y);
  }
  tile_writers[tile]->write_raster(value);
  r_count++;
}

void SRwriteTiled::write_nodata()
{
  int row = r_count/nrows;
  int col = r_count%nrows;
  int x = col / tile_size;
  int y = (nrows - row) / tile_size;
  int tile = y * tiles_x + x;
  if (tile_writers[tile] == 0) // do we need to create a writer for this tile
  {
    open_tile_file(tile,x,y);
  }
  tile_writers[tile]->write_nodata();
  r_count++;
}

void SRwriteTiled::close(bool close_file)
{
  int r_count_tiles = 0;
  // close of SRwriteTiled
  for (int i = 0; i < tiles_x * tiles_y; i++)
  {
    if (tile_writers[i])
    {
      r_count_tiles += tile_writers[i]->srbuffer->r_count;
      tile_writers[i]->close(close_file);
      delete tile_writers[i];
    }
  }
  delete [] tile_writers;
  tile_writers = 0;
  if (r_clipped) fprintf(stderr, "there were %d clipped rasters (and %d unclipped ones)\n", r_clipped, r_count_tiles);
  if (kml_overview_file)
  {
    fprintf(kml_overview_file, " </Document>\012");
    fprintf(kml_overview_file, "</kml>\012");
    fclose(kml_overview_file);
  }
  // close for SRwriter interface
  if (r_count > 0)
  {
    if (r_count != nrows*ncols)
    {
      fprintf(stderr, "WARNING: r_count is %d but nrows (%d) * ncols (%d) is %d\n",r_count,nrows,ncols,nrows*ncols);
    }
    r_count = -1;
  }
}

SRwriteTiled::SRwriteTiled()
{
  // init of SRwriteTiled interface
  srwriteopener = 0;
  tile_writers = 0;
  file_name_base = strdup("tiling");
  tile_size = -1;
  tiles_x = -1;
  tiles_y = -1;
  r_clipped = -1;
  kml_overview_file = 0;
  kml_geo_converter = 0;
}

SRwriteTiled::~SRwriteTiled()
{
  // clean-up for SRwriteTiled
  if (file_name_base) free(file_name_base);
}
