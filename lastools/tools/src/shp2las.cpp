/*
===============================================================================

  FILE:  shp2las.cpp
  
  CONTENTS:
  
    This tool converts point data that stored in the ESRI Shapefile format in
    either a Point or a MultiPoint primitive to the the binary LAS format.

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2007-09 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    25 August 2009 -- created after painting walls and hanging the yellow curtains 
  
===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "laswriter.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"shp2las -i lidar.shp -o lidar.las\n");
  fprintf(stderr,"shp2las -h\n");
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

static void from_big_endian(int* value)
{
#if (defined(i386) || defined(WIN32))   // if little endian machine
  char help;
  char* field = (char*)value;
  help = field[0];
  field[0] = field[3];
  field[3] = help;
  help = field[1];
  field[1] = field[2];
  field[2] = help;
#else                                   // else big endian machine
#endif
}

static void from_little_endian(int* value)
{
#if (defined(i386) || defined(WIN32))   // if little endian machine
#else                                   // else big endian machine
  char help;
  char* field = (char*)value;
  help = field[0];
  field[0] = field[3];
  field[3] = help;
  help = field[1];
  field[1] = field[2];
  field[2] = help;
#endif
}

static void from_big_endian(double* value)
{
#if (defined(i386) || defined(WIN32))   // if little endian machine
  char help;
  char* field = (char*)value;
  help = field[0];
  field[0] = field[7];
  field[7] = help;
  help = field[1];
  field[1] = field[6];
  field[6] = help;
  help = field[2];
  field[2] = field[5];
  field[5] = help;
  help = field[3];
  field[3] = field[4];
  field[4] = help;
#else                                   // else big endian machine
#endif
}

static void from_little_endian(double* value)
{
#if (defined(i386) || defined(WIN32))   // if little endian machine
#else                                   // else big endian machine
  char help;
  char* field = (char*)value;
  help = field[0];
  field[0] = field[7];
  field[7] = help;
  help = field[1];
  field[1] = field[6];
  field[6] = help;
  help = field[2];
  field[2] = field[5];
  field[5] = help;
  help = field[3];
  field[3] = field[4];
  field[4] = help;
#endif
}

inline void VecCopy3dv(double v[3], const double a[3])
{
  v[0] = a[0];
  v[1] = a[1];
  v[2] = a[2];
}

inline void VecUpdateMinMax3dv(double min[3], double max[3], const double v[3])
{
  if (v[0]<min[0]) min[0]=v[0]; else if (v[0]>max[0]) max[0]=v[0];
  if (v[1]<min[1]) min[1]=v[1]; else if (v[1]>max[1]) max[1]=v[1];
  if (v[2]<min[2]) min[2]=v[2]; else if (v[2]>max[2]) max[2]=v[2];
}

int main(int argc, char *argv[])
{
  int i;
  bool ishp = false;
  bool verbose = false;
  int compression = 0;
  char* file_name_in = 0;
  char* file_name_out = 0;

  if (argc == 1)
  {
    fprintf(stderr,"shp2las.exe is better run in the command line\n");
    file_name_in = new char[256];
    fprintf(stderr,"enter input file: "); fgets(file_name_in, 256, stdin);
    file_name_in[strlen(file_name_in)-1] = '\0';
    file_name_out = new char[256];
    fprintf(stderr,"enter output file: "); fgets(file_name_out, 256, stdin);
    file_name_out[strlen(file_name_out)-1] = '\0';
  }

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-h") == 0)
    {
      usage();
    }
    else if (strcmp(argv[i],"-verbose") == 0)
    {
      verbose = true;
    }
    else if (strcmp(argv[i],"-compress") == 0 || strcmp(argv[i],"-compression") == 0)
    {
      compression = 1;
    }
    else if (strcmp(argv[i],"-ishp") == 0)
    {
      ishp = true;
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
    else if (i == argc - 2 && file_name_in == 0 && file_name_out == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in == 0 && file_name_out == 0)
    {
      file_name_in = argv[i];
    }
    else if (i == argc - 1 && file_name_in && file_name_out == 0)
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

  if (ishp)
  {
    // input from stdin
    file_in = stdin;
#ifdef _WIN32
    if(_setmode( _fileno( stdin ), _O_BINARY ) == -1 )
    {
      fprintf(stderr, "ERROR: cannot set stdin to binary (untranslated) mode\n");
    }
#endif
  }
  else
  {
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
        fprintf(stderr, "ERROR: could not open '%s' for read\n",file_name_in);
        usage(argc==1);
      }
    }
    else
    {
      fprintf(stderr, "ERROR: no input file specified\n");
      usage(argc==1);
    }
  }

  // read SHP header

  int int_input;
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // file code (BIG)
  from_big_endian(&int_input); 
  if (int_input != 9994)
  {
    fprintf(stderr, "ERROR: wrong file code %d != 9994\n", int_input);
    return false;
  }
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // unused (BIG)
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // unused (BIG)
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // unused (BIG)
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // unused (BIG)
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // unused (BIG)
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // file length (BIG)
  from_big_endian(&int_input); 
  if (verbose) fprintf(stderr, "file length %d\n", int_input);
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // version (LITTLE)
  from_little_endian(&int_input); 
  if (int_input != 1000)
  {
    fprintf(stderr, "ERROR: wrong version %d != 1000\n", int_input);
    return false;
  }
  if (fread(&int_input, sizeof(int), 1, file_in) != 1) return false; // shape type (LITTLE)
  from_little_endian(&int_input); 
  int shape_type = int_input;
  if (shape_type != 1 && shape_type != 11 && shape_type != 21 && shape_type != 8 && shape_type != 18 && shape_type != 28)
  {
    fprintf(stderr, "ERROR: wrong shape type %d != 1,11,21,8,18,28\n", shape_type);
    return false;
  }
  if (verbose) fprintf(stderr, "shape type %d\n", shape_type);
  double double_input;
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // xmin (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "xmin %lf\n", double_input);
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // ymin (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "ymin %lf\n", double_input);
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // xmax (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "xmax %lf\n", double_input);
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // ymax (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "ymax %lf\n", double_input);
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // zmin (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "zmin %lf\n", double_input);
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // zmax (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "zmax %lf\n", double_input);
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // mmin (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "mmin %lf\n", double_input);
  if (fread(&double_input, sizeof(double), 1, file_in) != 1) return false; // mmax (LITTLE)
  from_little_endian(&double_input); 
  if (verbose) fprintf(stderr, "mmax %lf\n", double_input);

  // create output file name if needed 

  if (file_name_out == 0)
  {
    if (file_name_in == 0)
    {
      fprintf(stderr, "ERROR: no output file specified\n");
      usage(argc==1);
    }

    int len = strlen(file_name_in);
    file_name_out = strdup(file_name_in);
    if (file_name_out[len-3] == '.' && file_name_out[len-2] == 'g' && file_name_out[len-1] == 'z')
    {
      len = len - 4;
    }
    while (len > 0 && file_name_out[len] != '.')
    {
      len--;
    }
    file_name_out[len] = '.';
    file_name_out[len+1] = 'l';
    file_name_out[len+2] = 'a';
    file_name_out[len+3] = 's';
    file_name_out[len+4] = '\0';
  }

  // open output file
  
  FILE* file_out = fopen(file_name_out, "wb");

  // fail if output file does not open

  if (file_out == 0)
  {
    fprintf(stderr, "ERROR: could not open '%s' for write\n",file_name_out);
    usage(argc==1);
  }

  // open LASwriter

  LASwriter* laswriter = new LASwriter();
  if (laswriter->open(file_out, 0, compression) == false)
  {
    fprintf(stderr, "ERROR: could not open LASwriter\n");
    usage(argc==1);
  }

  double* points = 0;
  int points_allocated = 0;
  int number_of_points = 0;

  // read and convert the points to LAS format
  while (true)
  {
    int int_input;
    if (fread(&int_input, sizeof(int), 1, file_in) != 1) break; // record number (BIG)
    from_big_endian(&int_input);
    if (fread(&int_input, sizeof(int), 1, file_in) != 1) break; // content length (BIG)
    from_big_endian(&int_input);
    if (fread(&int_input, sizeof(int), 1, file_in) != 1) break; // shape type (LITTLE)
    from_little_endian(&int_input);
    if (int_input != shape_type)
    {
      fprintf(stderr, "ERROR: wrong shape type %d != %d\n", int_input, shape_type);
    }
    double double_input;
    if (shape_type == 8 || shape_type == 18 || shape_type == 28) // Multipoint
    {
      if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // xmin (LITTLE)
      from_little_endian(&double_input);
      if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // ymin (LITTLE)
      from_little_endian(&double_input);
      if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // xmax (LITTLE)
      from_little_endian(&double_input);
      if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // ymax (LITTLE)
      from_little_endian(&double_input);
      if (fread(&int_input, sizeof(int), 1, file_in) != 1) break; // number of points (LITTLE)
      from_little_endian(&int_input);
      number_of_points = int_input;
    }
    else // or regular point?
    {
      number_of_points = 1;
    }
    if (points_allocated < number_of_points)
    {
      if (points) delete [] points;
      points = new double[number_of_points*3];
      points_allocated = number_of_points;
    }
    // read points x and y
    for (i = 0; i < number_of_points; i++)
    {
      if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // x of point (LITTLE)
      from_little_endian(&double_input);
      points[3*i+0] = double_input;
      if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // y of point (LITTLE)
      from_little_endian(&double_input);
      points[3*i+1] = double_input;
    }
    // read points z
    if (shape_type == 11 || shape_type == 18)
    {
      if (shape_type == 18) // Multipoint
      {
        if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // zmin (LITTLE)
        from_little_endian(&double_input);
        if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // zmin (LITTLE)
        from_little_endian(&double_input);
      }
      for (i = 0; i < number_of_points; i++)
      {
        if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // z of point (LITTLE)
        from_little_endian(&double_input);
        points[3*i+2] = double_input;
      }
    }
    else
    {
      for (i = 0; i < number_of_points; i++)
      {
        points[3*i+2] = 0.0;
      }
    }
    // read points m
    if (shape_type == 11 || shape_type == 21 || shape_type == 18 || shape_type == 28)
    {
      if (shape_type == 18 || shape_type == 28) // Multipoint
      {
        if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // mmin (LITTLE)
        from_little_endian(&double_input);
        if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // mmin (LITTLE)
        from_little_endian(&double_input);
      }
      for (i = 0; i < number_of_points; i++)
      {
        if (fread(&double_input, sizeof(double), 1, file_in) != 1) break; // m of point (LITTLE)
        from_little_endian(&double_input);
      }
    }
    // write points
    for (i = 0; i < number_of_points; i++)
    {
      laswriter->write_point(&(points[3*i]));
    }
  }

  // close the input file
  
  fclose(file_in);

  // close the writer

  laswriter->close();

  // close the output file

  fclose(file_out);

  fprintf(stderr,"converted %d points to LAS.\n", laswriter->npoints);

  byebye(argc==1);

  return 0;
}
