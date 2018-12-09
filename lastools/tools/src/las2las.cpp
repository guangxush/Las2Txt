/*
===============================================================================

  FILE:  las2las.cpp
  
  CONTENTS:
  
    This tool reads and writes LIDAR data in the LAS format and is typically
    used to modify the contents of a LAS file. Examples are clipping the points
    to those that lie within a certain region specified by a bounding box (-clip),
    or points that are between a certain height (-clip_z or clip_elev)
    eliminating points that are the second return (-elim_return 2), that have a
    scan angle above a certain threshold (-elim_scan_angle_above 5), or that are
    below a certain intensity (-elim_intensity_below 15).
    Another typical use may be to extract only first (-first_only) returns or
    only last returns (-last_only). Extracting the first return is actually the
    same as eliminating all others (e.g. -elim_return 2 -elim_return 3, ...).

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2007-08 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    10 June 2009 -- added -scale_rgb_down and -scale_rgb_up to scale rgb values
    12 March 2009 -- updated to ask for input if started without arguments 
    9 March 2009 -- added ability to remove user defined headers or vlrs
    17 September 2008 -- updated to deal with LAS format version 1.2
    17 September 2008 -- allow clipping in double precision and based on z
    10 July 2007 -- created after talking with Linda about the H1B process
  
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
  fprintf(stderr,"las2las -remove_extra in.las out.las\n");
  fprintf(stderr,"las2las -remove_vlr -i in.las -o out.las\n");
  fprintf(stderr,"las2las -scale_rgb_up -i in.las -o out.las\n");
  fprintf(stderr,"las2las -i in.las -clip 630000 4834500 630500 4835000 -clip_z 10 100 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -eliminate_return 2 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -eliminate_scan_angle_above 15 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -set_version 1.2 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -eliminate_intensity_below 1000 -olas > out.las\n");
  fprintf(stderr,"las2las -i in.las -first_only -clip_int 63000000 483450000 63050000 483500000 -o out.las\n");
  fprintf(stderr,"las2las -i in.las -last_only -eliminate_intensity_below 2000 -olas > out.las\n");
  fprintf(stderr,"las2las -i in.las -keep_class 2 -keep_class 3 -keep_class 4 -olas > out.las\n");
  fprintf(stderr,"las2las -h\n");
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
  int* clip_xy_int_min = 0;
  int* clip_xy_int_max = 0;
  int* clip_z_int_min = 0;
  int* clip_z_int_max = 0;
  double* clip_xy_min = 0;
  double* clip_xy_max = 0;
  double* clip_z_min = 0;
  double* clip_z_max = 0;
  double* coordinates = 0;
  bool remove_extra_header = false;
  bool remove_variable_length_records = false;
  int scale_rgb = 0;
  int elim_return = 0;
  char set_version_major = -1;
  char set_version_minor = -1;
  int elim_scan_angle_above = 0;
  int elim_intensity_below = 0;
  int keep_classification[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  bool first_only = false;
  bool last_only = false;

  if (argc == 1)
  {
    fprintf(stderr,"las2las.exe is better run in the command line\n");
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
    else if (strcmp(argv[i],"-clip_int") == 0 || strcmp(argv[i],"-clip_int_xy") == 0)
    {
      clip_xy_int_min = new int[2];
      i++;
      clip_xy_int_min[0] = atoi(argv[i]);
      i++;
      clip_xy_int_min[1] = atoi(argv[i]);
      clip_xy_int_max = new int[2];
      i++;
      clip_xy_int_max[0] = atoi(argv[i]);
      i++;
      clip_xy_int_max[1] = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-clip_int_z") == 0 || strcmp(argv[i],"-clip_int_elev") == 0)
    {
      clip_z_int_min = new int[1];
      i++;
      clip_z_int_min[0] = atoi(argv[i]);
      clip_z_int_max = new int[1];
      i++;
      clip_z_int_max[0] = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-clip") == 0 || strcmp(argv[i],"-clip_xy") == 0)
    {
      if (coordinates == 0) coordinates = new double[3];
      clip_xy_min = new double[2];
      i++;
      clip_xy_min[0] = atof(argv[i]);
      i++;
      clip_xy_min[1] = atof(argv[i]);
      clip_xy_max = new double[2];
      i++;
      clip_xy_max[0] = atof(argv[i]);
      i++;
      clip_xy_max[1] = atof(argv[i]);
    }
    else if (strcmp(argv[i],"-clip_z") == 0 || strcmp(argv[i],"-clip_elev") == 0)
    {
      if (coordinates == 0) coordinates = new double[3];
      clip_z_min = new double[1];
      i++;
      clip_z_min[0] = atof(argv[i]);
      clip_z_max = new double[1];
      i++;
      clip_z_max[0] = atof(argv[i]);
    }
    else if (strcmp(argv[i],"-eliminate_return") == 0 || strcmp(argv[i],"-elim_return") == 0 || strcmp(argv[i],"-elim_ret") == 0)
    {
      i++;
      elim_return |= (1 << atoi(argv[i]));
    }
    else if (strcmp(argv[i],"-eliminate_scan_angle_above") == 0 || strcmp(argv[i],"-elim_scan_angle_above") == 0)
    {
      i++;
      elim_scan_angle_above = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-eliminate_intensity_below") == 0 || strcmp(argv[i],"-elim_intensity_below") == 0)
    {
      i++;
      elim_intensity_below = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-keep_classification") == 0 || strcmp(argv[i],"-keep_class") == 0)
    {
      i++;
      int j = 0;
      while (keep_classification[j] != -1) j++;
      keep_classification[j] = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-version") == 0 || strcmp(argv[i],"-set_version") == 0)
    {
      i++;
      int major;
      int minor;
      if (sscanf(argv[i],"%d.%d",&major,&minor) != 2)
      {
        fprintf(stderr,"cannot understand argument '%s'\n", argv[i]);
        usage();
      }
      set_version_major = (char)major;
      set_version_minor = (char)minor;
    }
    else if (strcmp(argv[i],"-scale_rgb_down") == 0)
    {
      scale_rgb = 1;
    }
    else if (strcmp(argv[i],"-scale_rgb_up") == 0)
    {
      scale_rgb = 2;
    }
    else if (strcmp(argv[i],"-first_only") == 0)
    {
      first_only = true;
    }
    else if (strcmp(argv[i],"-last_only") == 0)
    {
      last_only = true;
    }
    else if (strcmp(argv[i],"-remove_extra") == 0 || strcmp(argv[i],"-remove_extra_header") == 0)
    {
      remove_extra_header = true;
    }
    else if (strcmp(argv[i],"-remove_variable") == 0 || strcmp(argv[i],"-remove_vlr") == 0)
    {
      remove_variable_length_records = true;
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

  LASreader* lasreader = new LASreader();

  if (lasreader->open(file_in) == false)
  {
    fprintf(stderr, "ERROR: could not open lasreader\n");
    byebye(argc==1);
  }

  // do the first pass

  bool first_surviving_point = true;
  unsigned int surviving_number_of_point_records = 0;
  unsigned int surviving_number_of_points_by_return[] = {0,0,0,0,0,0,0,0};
  LASpoint surviving_point_min;
  LASpoint surviving_point_max;
  double surviving_gps_time_min;
  double surviving_gps_time_max;
  short surviving_rgb_min[3];
  short surviving_rgb_max[3];

  int clipped = 0;
  int eliminated_return = 0;
  int eliminated_scan_angle = 0;
  int eliminated_intensity = 0;
  int eliminated_first_only = 0;
  int eliminated_last_only = 0;
  int eliminated_classification = 0;

  if (verbose) ptime("start.");
  fprintf(stderr, "first pass reading %d points ...\n", lasreader->npoints);

  while (lasreader->read_point())
  {
    if (coordinates) lasreader->get_coordinates(coordinates);

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
    if (clip_xy_int_min && (lasreader->point.x < clip_xy_int_min[0] || lasreader->point.y < clip_xy_int_min[1]))
    {
      clipped++;
      continue;
    }
    if (clip_xy_int_max && (lasreader->point.x > clip_xy_int_max[0] || lasreader->point.y > clip_xy_int_max[1]))
    {
      clipped++;
      continue;
    }
    if (clip_z_int_min && (lasreader->point.z < clip_z_int_min[0]))
    {
      clipped++;
      continue;
    }
    if (clip_z_int_max && (lasreader->point.z > clip_z_int_max[0]))
    {
      clipped++;
      continue;
    }
    if (clip_xy_min && (coordinates[0] < clip_xy_min[0] || coordinates[1] < clip_xy_min[1]))
    {
      clipped++;
      continue;
    }
    if (clip_xy_max && (coordinates[0] > clip_xy_max[0] || coordinates[1] > clip_xy_max[1]))
    {
      clipped++;
      continue;
    }
    if (clip_z_min && (coordinates[2] < clip_z_min[0]))
    {
      clipped++;
      continue;
    }
    if (clip_z_max && (coordinates[2] > clip_z_max[0]))
    {
      clipped++;
      continue;
    }
    if (elim_return && (elim_return & (1 << lasreader->point.return_number)))
    {
      eliminated_return++;
      continue;
    }
    if (elim_scan_angle_above && (lasreader->point.scan_angle_rank > elim_scan_angle_above || lasreader->point.scan_angle_rank < -elim_scan_angle_above))
    {
      eliminated_scan_angle++;
      continue;
    }
    if (elim_intensity_below && lasreader->point.intensity < elim_intensity_below)
    {
      eliminated_intensity++;
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
    surviving_number_of_point_records++;
    surviving_number_of_points_by_return[lasreader->point.return_number-1]++;
    if (first_surviving_point)
    {
      surviving_point_min = lasreader->point;
      surviving_point_max = lasreader->point;
      if (lasreader->points_have_gps_time)
      {
        surviving_gps_time_min = lasreader->gps_time;
        surviving_gps_time_max = lasreader->gps_time;
      }
      if (lasreader->points_have_rgb)
      {
        if (scale_rgb == 1)
        {
          lasreader->rgb[0] = (lasreader->rgb[0]/256);
          lasreader->rgb[1] = (lasreader->rgb[1]/256);
          lasreader->rgb[2] = (lasreader->rgb[2]/256);
        }
        else if (scale_rgb == 2)
        {
          lasreader->rgb[0] = (lasreader->rgb[0]*256);
          lasreader->rgb[1] = (lasreader->rgb[1]*256);
          lasreader->rgb[2] = (lasreader->rgb[2]*256);
        }
        surviving_rgb_min[0] = lasreader->rgb[0];
        surviving_rgb_min[1] = lasreader->rgb[1];
        surviving_rgb_min[2] = lasreader->rgb[2];
        surviving_rgb_max[0] = lasreader->rgb[0];
        surviving_rgb_max[1] = lasreader->rgb[1];
        surviving_rgb_max[2] = lasreader->rgb[2];
      }
      first_surviving_point = false;
    }
    else
    {
      if (lasreader->point.x < surviving_point_min.x) surviving_point_min.x = lasreader->point.x;
      else if (lasreader->point.x > surviving_point_max.x) surviving_point_max.x = lasreader->point.x;
      if (lasreader->point.y < surviving_point_min.y) surviving_point_min.y = lasreader->point.y;
      else if (lasreader->point.y > surviving_point_max.y) surviving_point_max.y = lasreader->point.y;
      if (lasreader->point.z < surviving_point_min.z) surviving_point_min.z = lasreader->point.z;
      else if (lasreader->point.z > surviving_point_max.z) surviving_point_max.z = lasreader->point.z;
      if (lasreader->point.intensity < surviving_point_min.intensity) surviving_point_min.intensity = lasreader->point.intensity;
      else if (lasreader->point.intensity > surviving_point_max.intensity) surviving_point_max.intensity = lasreader->point.intensity;
      if (lasreader->point.edge_of_flight_line < surviving_point_min.edge_of_flight_line) surviving_point_min.edge_of_flight_line = lasreader->point.edge_of_flight_line;
      else if (lasreader->point.edge_of_flight_line > surviving_point_max.edge_of_flight_line) surviving_point_max.edge_of_flight_line = lasreader->point.edge_of_flight_line;
      if (lasreader->point.scan_direction_flag < surviving_point_min.scan_direction_flag) surviving_point_min.scan_direction_flag = lasreader->point.scan_direction_flag;
      else if (lasreader->point.scan_direction_flag > surviving_point_max.scan_direction_flag) surviving_point_max.scan_direction_flag = lasreader->point.scan_direction_flag;
      if (lasreader->point.number_of_returns_of_given_pulse < surviving_point_min.number_of_returns_of_given_pulse) surviving_point_min.number_of_returns_of_given_pulse = lasreader->point.number_of_returns_of_given_pulse;
      else if (lasreader->point.number_of_returns_of_given_pulse > surviving_point_max.number_of_returns_of_given_pulse) surviving_point_max.number_of_returns_of_given_pulse = lasreader->point.number_of_returns_of_given_pulse;
      if (lasreader->point.return_number < surviving_point_min.return_number) surviving_point_min.return_number = lasreader->point.return_number;
      else if (lasreader->point.return_number > surviving_point_max.return_number) surviving_point_max.return_number = lasreader->point.return_number;
      if (lasreader->point.classification < surviving_point_min.classification) surviving_point_min.classification = lasreader->point.classification;
      else if (lasreader->point.classification > surviving_point_max.classification) surviving_point_max.classification = lasreader->point.classification;
      if (lasreader->point.scan_angle_rank < surviving_point_min.scan_angle_rank) surviving_point_min.scan_angle_rank = lasreader->point.scan_angle_rank;
      else if (lasreader->point.scan_angle_rank > surviving_point_max.scan_angle_rank) surviving_point_max.scan_angle_rank = lasreader->point.scan_angle_rank;
      if (lasreader->point.user_data < surviving_point_min.user_data) surviving_point_min.user_data = lasreader->point.user_data;
      else if (lasreader->point.user_data > surviving_point_max.user_data) surviving_point_max.user_data = lasreader->point.user_data;
      if (lasreader->point.point_source_ID < surviving_point_min.point_source_ID) surviving_point_min.point_source_ID = lasreader->point.point_source_ID;
      else if (lasreader->point.point_source_ID > surviving_point_max.point_source_ID) surviving_point_max.point_source_ID = lasreader->point.point_source_ID;
      if (lasreader->point.point_source_ID < surviving_point_min.point_source_ID) surviving_point_min.point_source_ID = lasreader->point.point_source_ID;
      else if (lasreader->point.point_source_ID > surviving_point_max.point_source_ID) surviving_point_max.point_source_ID = lasreader->point.point_source_ID;
      if (lasreader->points_have_gps_time)
      {
        if (lasreader->gps_time < surviving_gps_time_min) surviving_gps_time_min = lasreader->gps_time;
        else if (lasreader->gps_time > surviving_gps_time_max) surviving_gps_time_max = lasreader->gps_time;
      }
      if (lasreader->points_have_rgb)
      {
        if (scale_rgb == 1)
        {
          lasreader->rgb[0] = (lasreader->rgb[0]/256);
          lasreader->rgb[1] = (lasreader->rgb[1]/256);
          lasreader->rgb[2] = (lasreader->rgb[2]/256);
        }
        else if (scale_rgb == 2)
        {
          lasreader->rgb[0] = (lasreader->rgb[0]*256);
          lasreader->rgb[1] = (lasreader->rgb[1]*256);
          lasreader->rgb[2] = (lasreader->rgb[2]*256);
        }
        if (lasreader->rgb[0] < surviving_rgb_min[0]) surviving_rgb_min[0] = lasreader->rgb[0];
        else if (lasreader->rgb[0] > surviving_rgb_max[0]) surviving_rgb_max[0] = lasreader->rgb[0];
        if (lasreader->rgb[1] < surviving_rgb_min[1]) surviving_rgb_min[1] = lasreader->rgb[1];
        else if (lasreader->rgb[1] > surviving_rgb_max[1]) surviving_rgb_max[1] = lasreader->rgb[1];
        if (lasreader->rgb[2] < surviving_rgb_min[2]) surviving_rgb_min[2] = lasreader->rgb[2];
        else if (lasreader->rgb[2] > surviving_rgb_max[2]) surviving_rgb_max[2] = lasreader->rgb[2];
      }
    }
  }

  if (eliminated_first_only) fprintf(stderr, "eliminated based on first returns only: %d\n", eliminated_first_only);
  if (eliminated_last_only) fprintf(stderr, "eliminated based on last returns only: %d\n", eliminated_last_only);
  if (clipped) fprintf(stderr, "clipped: %d\n", clipped);
  if (eliminated_return) fprintf(stderr, "eliminated based on return number: %d\n", eliminated_return);
  if (eliminated_scan_angle) fprintf(stderr, "eliminated based on scan angle: %d\n", eliminated_scan_angle);
  if (eliminated_intensity) fprintf(stderr, "eliminated based on intensity: %d\n", eliminated_intensity);
  if (eliminated_classification) fprintf(stderr, "eliminated based on classification: %d\n", eliminated_classification);

  lasreader->close();
  fclose(file_in);

  if (verbose)
  {
    fprintf(stderr, "x %d %d %d\n",surviving_point_min.x, surviving_point_max.x, surviving_point_max.x - surviving_point_min.x);
    fprintf(stderr, "y %d %d %d\n",surviving_point_min.y, surviving_point_max.y, surviving_point_max.y - surviving_point_min.y);
    fprintf(stderr, "z %d %d %d\n",surviving_point_min.z, surviving_point_max.z, surviving_point_max.z - surviving_point_min.z);
    fprintf(stderr, "intensity %d %d %d\n",surviving_point_min.intensity, surviving_point_max.intensity, surviving_point_max.intensity - surviving_point_min.intensity);
    fprintf(stderr, "edge_of_flight_line %d %d %d\n",surviving_point_min.edge_of_flight_line, surviving_point_max.edge_of_flight_line, surviving_point_max.edge_of_flight_line - surviving_point_min.edge_of_flight_line);
    fprintf(stderr, "scan_direction_flag %d %d %d\n",surviving_point_min.scan_direction_flag, surviving_point_max.scan_direction_flag, surviving_point_max.scan_direction_flag - surviving_point_min.scan_direction_flag);
    fprintf(stderr, "number_of_returns_of_given_pulse %d %d %d\n",surviving_point_min.number_of_returns_of_given_pulse, surviving_point_max.number_of_returns_of_given_pulse, surviving_point_max.number_of_returns_of_given_pulse - surviving_point_min.number_of_returns_of_given_pulse);
    fprintf(stderr, "return_number %d %d %d\n",surviving_point_min.return_number, surviving_point_max.return_number, surviving_point_max.return_number - surviving_point_min.return_number);
    fprintf(stderr, "classification %d %d %d\n",surviving_point_min.classification, surviving_point_max.classification, surviving_point_max.classification - surviving_point_min.classification);
    fprintf(stderr, "scan_angle_rank %d %d %d\n",surviving_point_min.scan_angle_rank, surviving_point_max.scan_angle_rank, surviving_point_max.scan_angle_rank - surviving_point_min.scan_angle_rank);
    fprintf(stderr, "user_data %d %d %d\n",surviving_point_min.user_data, surviving_point_max.user_data, surviving_point_max.user_data - surviving_point_min.user_data);
    fprintf(stderr, "point_source_ID %d %d %d\n",surviving_point_min.point_source_ID, surviving_point_max.point_source_ID, surviving_point_max.point_source_ID - surviving_point_min.point_source_ID);
    if (lasreader->points_have_gps_time)
    {
      fprintf(stderr, "gps_time %.8f %.8f %.8f\n",surviving_gps_time_min, surviving_gps_time_max, surviving_gps_time_max - surviving_gps_time_min);
    }
    if (lasreader->points_have_rgb)
    {
      fprintf(stderr, "R %d %d %d\n",surviving_rgb_min[0], surviving_rgb_max[0], surviving_rgb_max[0] - surviving_rgb_min[0]);
      fprintf(stderr, "G %d %d %d\n",surviving_rgb_min[1], surviving_rgb_max[1], surviving_rgb_max[1] - surviving_rgb_min[1]);
      fprintf(stderr, "B %d %d %d\n",surviving_rgb_min[2], surviving_rgb_max[2], surviving_rgb_max[2] - surviving_rgb_min[2]);
    }
  }

  // quit if no output specified

  if (file_name_out == 0 && !olas && !olaz)
  {
    fprintf(stderr, "no output specified. exiting ...\n");
    byebye(argc==1);
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
    usage();
  }

  if (lasreader->open(file_in) == false)
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
    usage();
  }

  // maybe we should remove some stuff

  if (remove_extra_header)
  {
    lasreader->header.clean_user_data_in_header();
    lasreader->header.clean_user_data_after_header();
  }

  if (remove_variable_length_records)
  {
    lasreader->header.clean_vlrs();
  }  

  // prepare the header for the surviving points

  lasreader->header.number_of_point_records = surviving_number_of_point_records;
  for (i = 0; i < 5; i++) lasreader->header.number_of_points_by_return[i] = surviving_number_of_points_by_return[i];
  lasreader->header.min_x = surviving_point_min.x*lasreader->header.x_scale_factor + lasreader->header.x_offset;
  lasreader->header.max_x = surviving_point_max.x*lasreader->header.x_scale_factor + lasreader->header.x_offset;
  lasreader->header.min_y = surviving_point_min.y*lasreader->header.y_scale_factor + lasreader->header.y_offset;
  lasreader->header.max_y = surviving_point_max.y*lasreader->header.y_scale_factor + lasreader->header.y_offset;
  lasreader->header.min_z = surviving_point_min.z*lasreader->header.z_scale_factor + lasreader->header.z_offset;
  lasreader->header.max_z = surviving_point_max.z*lasreader->header.z_scale_factor + lasreader->header.z_offset;
  if (set_version_major != -1) lasreader->header.version_major = set_version_major;
  if (set_version_minor != -1) lasreader->header.version_minor = set_version_minor;
  fprintf(stderr, "second pass reading %d and writing %d points ...\n", lasreader->npoints, surviving_number_of_point_records);

  LASwriter* laswriter = new LASwriter();

  if (laswriter->open(file_out, &lasreader->header, compression) == false)
  {
    fprintf(stderr, "ERROR: could not open laswriter\n");
    byebye(argc==1);
  }

  // loop over points

  while (lasreader->read_point())
  {
    if (coordinates) lasreader->get_coordinates(coordinates);

    // put the points through all the tests
    if (last_only && lasreader->point.return_number != lasreader->point.number_of_returns_of_given_pulse)
    {
      continue;
    }
    if (first_only && lasreader->point.return_number != 1)
    {
      continue;
    }
    if (clip_xy_int_min && (lasreader->point.x < clip_xy_int_min[0] || lasreader->point.y < clip_xy_int_min[1]))
    {
      continue;
    }
    if (clip_xy_int_max && (lasreader->point.x > clip_xy_int_max[0] || lasreader->point.y > clip_xy_int_max[1]))
    {
      continue;
    }
    if (clip_z_int_min && (lasreader->point.z < clip_z_int_min[0]))
    {
      continue;
    }
    if (clip_z_int_max && (lasreader->point.z > clip_z_int_max[0]))
    {
      continue;
    }
    if (clip_xy_min && (coordinates[0] < clip_xy_min[0] || coordinates[1] < clip_xy_min[1]))
    {
      continue;
    }
    if (clip_xy_max && (coordinates[0] > clip_xy_max[0] || coordinates[1] > clip_xy_max[1]))
    {
      continue;
    }
    if (clip_z_min && (coordinates[2] < clip_z_min[0]))
    {
      continue;
    }
    if (clip_z_max && (coordinates[2] > clip_z_max[0]))
    {
      continue;
    }
    if (elim_return && (elim_return & (1 << lasreader->point.return_number)))
    {
      continue;
    }
    if (elim_scan_angle_above && (lasreader->point.scan_angle_rank > elim_scan_angle_above || lasreader->point.scan_angle_rank < -elim_scan_angle_above))
    {
      continue;
    }
    if (elim_intensity_below && lasreader->point.intensity < elim_intensity_below)
    {
      continue;
    }
    if (keep_classification[0] != -1)
    {
      int j = 0;
      while (keep_classification[j] != -1 && lasreader->point.classification != keep_classification[j]) j++;
      if (keep_classification[j] == -1) continue;
    }
    if (lasreader->points_have_rgb)
    {
      if (scale_rgb == 1)
      {
        lasreader->rgb[0] = (lasreader->rgb[0]/256);
        lasreader->rgb[1] = (lasreader->rgb[1]/256);
        lasreader->rgb[2] = (lasreader->rgb[2]/256);
      }
      else if (scale_rgb == 2)
      {
        lasreader->rgb[0] = (lasreader->rgb[0]*256);
        lasreader->rgb[1] = (lasreader->rgb[1]*256);
        lasreader->rgb[2] = (lasreader->rgb[2]*256);
      }
    }
    laswriter->write_point(&(lasreader->point), lasreader->gps_time, lasreader->rgb);
  }

  laswriter->close();
  fclose(file_out);

  lasreader->close();
  fclose(file_in);

  if (verbose) ptime("done.");

  byebye(argc==1);

  return 0;
}
