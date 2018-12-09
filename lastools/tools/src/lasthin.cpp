/*
===============================================================================

  FILE:  lasthin.cpp
  
  CONTENTS:
  
    This tool is a very simplistic point thinner. It overlays a uniform grid
    and keeps the lowest point with each cell. Optionally we can request to
    only consider last returns (-last_only) or a particular classification
    (-keep_classification 2) before doing the thinning.

    To be done: As we are  essentially gridding the data we also allow this grid to be
    output in form of an ESRI ARC/INFO ASCII GRID. Before outputting the grid
    one can request possible empty grid cells to be flodded with values from
    neighbour cells. With iterative flooding we can fill larger empty spots
    (-flood 4).

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2007  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    12 March 2009 -- updated to ask for input if started without arguments 
    19 April 2008 -- created after not going on Sheker's full moon hike
  
===============================================================================
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lasreader.h"
#include "laswriter.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"lasthin in.las out.las\n");
  fprintf(stderr,"lasthin -i in.las -grid_spacing 0.5 -o out.las\n");
  fprintf(stderr,"lasthin -i in.las -last_only -grid_spacing 1.0 -remove_extra_header -o out.las\n");
  fprintf(stderr,"lasthin -i in.las -keep_class 2 -keep_class 3 -keep_class 4 -grid_spacing 0.5 -olas > out.las\n");
  fprintf(stderr,"lasthin -h\n");
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

void ptime(const char *const msg)
{
  float t= ((float)clock())/CLOCKS_PER_SEC;
  fprintf(stderr, "cumulative CPU time thru %s = %f\n", msg, t);
}

int main(int argc, char *argv[])
{
  int i;
  bool verbose = false;
  char* file_name_in = 0;
  char* file_name_out = 0;
  bool olas = false;
  bool olaz = false;
  bool remove_extra_header = false;
  int keep_classification[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  bool last_only = false;
  double grid_spacing = 1.0;

  if (argc == 1)
  {
    fprintf(stderr,"lasthin.exe is better run in the command line\n");
    file_name_in = new char[256];
    fprintf(stderr,"enter input file: "); fgets(file_name_in, 256, stdin);
    file_name_in[strlen(file_name_in)-1] = '\0';
    file_name_out = new char[256];
    fprintf(stderr,"enter output file: "); fgets(file_name_out, 256, stdin);
    file_name_out[strlen(file_name_out)-1] = '\0';
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
    else if (strcmp(argv[i],"-o") == 0)
    {
      i++;
      file_name_out = argv[i];
    }
    else if (strcmp(argv[i],"-olas") == 0)
    {
      olas = true;
    }
    else if (strcmp(argv[i],"-olaz") == 0)
    {
      olaz = true;
    }
    else if (strcmp(argv[i],"-grid_spacing") == 0)
    {
      i++;
      grid_spacing = atof(argv[i]);
    }
    else if (strcmp(argv[i],"-keep_classification") == 0 || strcmp(argv[i],"-keep_class") == 0)
    {
      i++;
      int j = 0;
      while (keep_classification[j] != -1) j++;
      keep_classification[j] = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-last_only") == 0)
    {
      last_only = true;
    }
    else if (strcmp(argv[i],"-remove_extra_header") == 0)
    {
      remove_extra_header = true;
    }
    else if (i == argc - 2 && file_name_in == 0 && file_name_out == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in == 0 && file_name_out == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in != 0 && file_name_out == 0)
    {
      file_name_out = argv[i];
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
      byebye(argc==1);
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
    byebye(argc==1);
  }

  LASreader* lasreader = new LASreader();

  if (!lasreader->open(file_in))
  {
    fprintf(stderr, "ERROR: could not open lasreader\n");
    byebye(argc==1);
  }

  // create the grid
  int lowest_x = (int)((lasreader->header.min_x / grid_spacing) + .5);
  int lowest_y = (int)((lasreader->header.min_y / grid_spacing) + .5);
  int highest_x = (int)((lasreader->header.max_x / grid_spacing) + .5);
  int highest_y = (int)((lasreader->header.max_y / grid_spacing) + .5);
  int size_x = highest_x-lowest_x+1;
  int size_y = highest_y-lowest_y+1;
  int size = size_x*size_y;

  fprintf(stderr, "thinning points onto %d by %d = %d grid (grid_spacing = %.2f unit)\n", size_x, size_y, size, grid_spacing);
  
  int* elevation_grid = new int[size];
  if (elevation_grid == 0) 
  {
    fprintf(stderr, "ERROR: could not allocate grid of size %d by %d\n");
    byebye(argc==1);
  }
  for (i = 0; i < size; i++) elevation_grid[i] = 2147483647;

  // do the first pass

  unsigned int surviving_number_of_point_records = 0;
  unsigned int surviving_number_of_points_by_return[] = {0,0,0,0,0,0,0,0};
  int eliminated_last_only = 0;
  int eliminated_classification = 0;
  int eliminated_thinning = 0;

  if (verbose) ptime("start.");
  fprintf(stderr, "first pass reading %d points ...\n", lasreader->npoints);

  // loop over points

  double xyz[3];
  while (lasreader->read_point(xyz))
  {
    // put the points through two tests
    if (last_only && lasreader->point.return_number != lasreader->point.number_of_returns_of_given_pulse)
    {
      eliminated_last_only++;
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

    int point_x = (int)((xyz[0] / grid_spacing) + .5);
    int point_y = (int)((xyz[1] / grid_spacing) + .5);
    int pos_x = point_x-lowest_x;
    int pos_y = point_y-lowest_y;
    int pos = pos_y*size_x+pos_x;

    if (elevation_grid[pos] == 2147483647)
    {
      surviving_number_of_point_records++;
      surviving_number_of_points_by_return[lasreader->point.return_number-1]++;
      elevation_grid[pos] = lasreader->point.z;
    }
    else
    {
      eliminated_thinning++;
      if (lasreader->point.z < elevation_grid[pos])
      {
        elevation_grid[pos] = lasreader->point.z;
      }
    }
  }

  if (eliminated_last_only) fprintf(stderr, "eliminated based on last returns only: %d\n", eliminated_last_only);
  if (eliminated_classification) fprintf(stderr, "eliminated based on classification: %d\n", eliminated_classification);
  if (eliminated_thinning) fprintf(stderr, "eliminated based on thinning: %d\n", eliminated_thinning);

  fprintf(stderr, "grid saturation is %d of %d point (%.2f percent)\n", surviving_number_of_point_records, size, 100.0f*surviving_number_of_point_records/size);

  lasreader->close();
  fclose(file_in);

  // quit if no output specified

  if (file_name_out == 0 && !olas && !olaz)
  {
    fprintf(stderr, "no output specified. exiting ...\n");
    exit(1);
  }

  // re-open input file

  if (file_name_in)
  {
    if (strstr(file_name_in, ".gz"))
    {
#ifdef _WIN32
      file_in = fopenGzipped(file_name_in, "rb");
#else
      fprintf(stderr, "ERROR: no support for gzipped input\n");
      usage(argc==1);
#endif
    }
    else
    {
      file_in = fopen(file_name_in, "rb");
    }
    if (file_in == 0)
    {
      fprintf(stderr, "ERROR: could not open '%s'\n",file_name_in);
      usage(argc==1);
    }
  }
  else
  {
    fprintf(stderr, "ERROR: no input specified\n");
    usage(argc==1);
  }

  if (!lasreader->open(file_in))
  {
    fprintf(stderr, "ERROR: could not re-open lasreader\n");
    byebye(argc==1);
  }

  // open output file

  int compression = 0;

  FILE* file_out;
  if (file_name_out)
  {
    file_out = fopen(file_name_out, "wb");
    if (strstr(file_name_out, ".laz") || strstr(file_name_out, ".las.lz"))
    {
      compression = 1;
    }
  }
  else if (olas || olaz)
  {
    file_out = stdout;
    if (olaz)
    {
      compression = 1;
    }
  }
  else
  {
    fprintf (stderr, "ERROR: no output specified\n");
    byebye(argc==1);
  }

  // prepare the header for the surviving points

  lasreader->header.number_of_point_records = surviving_number_of_point_records;
  for (i = 0; i < 5; i++) lasreader->header.number_of_points_by_return[i] = surviving_number_of_points_by_return[i];

  fprintf(stderr, "second pass reading %d and writing %d points ...\n", lasreader->npoints, surviving_number_of_point_records);

  LASwriter* laswriter = new LASwriter();

  if (laswriter->open(file_out, &lasreader->header, compression) == false)
  {
    fprintf(stderr, "ERROR: could not open laswriter\n");
    exit(1);
  }

  // loop over points

  while (lasreader->read_point(xyz))
  {
    // put the points through all the tests
    if (last_only && lasreader->point.return_number != lasreader->point.number_of_returns_of_given_pulse)
    {
      continue;
    }
    if (keep_classification[0] != -1)
    {
      int j = 0;
      while (keep_classification[j] != -1 && lasreader->point.classification != keep_classification[j]) j++;
      if (keep_classification[j] == -1) continue;
    }

    int point_x = (int)((xyz[0] / grid_spacing) + .5);
    int point_y = (int)((xyz[1] / grid_spacing) + .5);
    int pos_x = point_x-lowest_x;
    int pos_y = point_y-lowest_y;
    int pos = pos_y*size_x+pos_x;

    if (elevation_grid[pos] == lasreader->point.z)
    {
      elevation_grid[pos]--;
      laswriter->write_point(&(lasreader->point), lasreader->gps_time, lasreader->rgb);
    }
  }

  laswriter->close();
  fclose(file_out);

  lasreader->close();
  fclose(file_in);

  if (verbose) ptime("done.");

  byebye(argc==1);

  return 0;
}
