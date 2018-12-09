/*
===============================================================================

  FILE:  las2tin.cpp
  
  CONTENTS:
  
    This tool reads LIDAR data in the LAS format and triangulates them into a
    TIN. The output is either a simple list of triangles in ASCII format (without
    the points) or a mesh simple OBJ format.

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2009 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
     8 August 2009 -- added the possibility to output to ESRI Shapefiles  
    31 March 2009 -- created on a lonely flight UA 927 from FRA to SFO  
  
===============================================================================
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lasreader.h"
#include "triangulate.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"las2tin -i lidar.las -o tin.shp\n");
  fprintf(stderr,"las2tin -i lidar.las -first_only -o mesh.obj\n");
  fprintf(stderr,"las2tin -i lidar.las -last_only -o triangles.txt\n");
  fprintf(stderr,"las2tin -i lidar.las -last_only -keep_class 2 -keep_class 3 -keep_class 9 -o tin.shp\n");
  fprintf(stderr,"las2tin -i lidar.las -keep_class 8 -oobj > mesh.obj\n");
  fprintf(stderr,"las2tin -h\n");
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

void to_big_endian(int* value)
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

void to_little_endian(int* value)
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

void to_big_endian(double* value)
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

void to_little_endian(double* value)
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

int main(int argc, char *argv[])
{
  int i;
  bool verbose = false;
  char* file_name_in = 0;
  char* file_name_out = 0;
  bool otxt = false;
  bool oobj = false;
  int keep_classification[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  bool first_only = false;
  bool last_only = false;

  if (argc == 1)
  {
    fprintf(stderr,"las2tin.exe is better run in the command line\n");
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
    else if (strcmp(argv[i],"-otxt") == 0)
    {
      otxt = true;
    }
    else if (strcmp(argv[i],"-oobj") == 0)
    {
      oobj = true;
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

  double coordinates[3];
  LASreader* lasreader = new LASreader();

  if (lasreader->open(file_in) == false)
  {
    fprintf(stderr, "ERROR: could not open lasreader\n");
    byebye(argc==1);
  }

  int npoints = lasreader->npoints;

  if (first_only || last_only || keep_classification[0] != -1) // if we only triangulate a subset of points
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

  // open output file

  FILE* file_out;
  FILE* file_shx;
  if (file_name_out)
  {
    if (strstr(file_name_out, ".shp"))
    {
      file_out = fopen(file_name_out, "wb");
      file_name_out[strlen(file_name_out)-3] = 's';
      file_name_out[strlen(file_name_out)-2] = 'h';
      file_name_out[strlen(file_name_out)-1] = 'x';
      file_shx = fopen(file_name_out, "wb");
    }
    else
    {
      file_out = fopen(file_name_out, "w");
      file_shx = 0;
    }
  }
  else if (otxt || oobj)
  {
    file_out = stdout;
  }
  else
  {
    fprintf (stderr, "ERROR: no output specified\n");
    usage(argc==1);
  }

  // loop over points, store points, and triangulate

  int count = 0;
  float* point_buffer = (float*)malloc(sizeof(float)*3*npoints);
  int* index_map = (npoints == lasreader->npoints ? 0 : (int*)malloc(sizeof(int)*npoints));
  
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
    // add point to the point buffer while subtracting the min bounding box
    point_buffer[3*count+0] = (float)(coordinates[0] - lasreader->header.min_x);
    point_buffer[3*count+1] = (float)(coordinates[1] - lasreader->header.min_y);
    point_buffer[3*count+2] = (float)(coordinates[2] - lasreader->header.min_z);
    if (index_map) index_map[count] = lasreader->p_count -1;
    // add the point to the triangulation
    TINadd(&(point_buffer[3*count]));
    count++;
  }
  TINfinish();

  lasreader->close();
  fclose(file_in);

  fprintf(stderr, "outputting the triangles ...\n");
  if (verbose) ptime("start output.");

  // output triangulation or mesh

  if ((file_name_out && strstr(file_name_out, ".obj")) || oobj)
  {
    for (count = 0; count < npoints; count++)
    {
      fprintf(file_out, "v %.12g %.12g %.6g\012", lasreader->header.min_x+point_buffer[count*3+0], lasreader->header.min_y+point_buffer[count*3+1], lasreader->header.min_z+point_buffer[count*3+2]);
    }
    TINtriangle* t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          fprintf(file_out, "f %d %d %d\012", 1 + ((t->V[0] - point_buffer)/3), 1 + ((t->V[1] - point_buffer)/3), 1 + ((t->V[2] - point_buffer)/3));
        }
      }
    }
  }
  else if ((file_name_out && strstr(file_name_out, ".txt")) || otxt)
  {
    TINtriangle* t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          if (index_map)
            fprintf(file_out, "%d %d %d\012", index_map[(t->V[0] - point_buffer)/3], index_map[(t->V[1] - point_buffer)/3], index_map[(t->V[2] - point_buffer)/3]);
          else
            fprintf(file_out, "%d %d %d\012", (t->V[0] - point_buffer)/3, (t->V[1] - point_buffer)/3, (t->V[2] - point_buffer)/3);
        }
      }
    }
  }
  else if (file_name_out && strstr(file_name_out, ".shx"))
  {
    // count triangles
    int nfaces = 0;
    TINtriangle* t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          nfaces++;
        }
      }
    }
    int content_length = (76 + (nfaces * 104)) / 2;

    // write header

    int int_output;
    int_output = 9994; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // file code (BIG)
    int_output = 0; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
    fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
    fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
    fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
    fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
    int_output = 50 + 4 + content_length; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // file length (BIG)
    int_output = 1000; to_little_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // version (LITTLE)
    int_output = 31; to_little_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = MultiPatch
    double double_output;
    double_output = lasreader->header.min_x; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
    double_output = lasreader->header.min_y; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
    double_output = lasreader->header.max_x; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
    double_output = lasreader->header.max_y; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)
    double_output = lasreader->header.min_z; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
    double_output = lasreader->header.max_z; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)
    double_output = 0.0; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // mmin (LITTLE)
    double_output = 0.0; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // mmax (LITTLE)

    if (file_shx)
    {
      int_output = 9994; to_big_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_shx); // file code (BIG)
      int_output = 0; to_big_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_shx); // unused (BIG)
      fwrite(&int_output, sizeof(int), 1, file_shx); // unused (BIG)
      fwrite(&int_output, sizeof(int), 1, file_shx); // unused (BIG)
      fwrite(&int_output, sizeof(int), 1, file_shx); // unused (BIG)
      fwrite(&int_output, sizeof(int), 1, file_shx); // unused (BIG)
      int_output = 50 + 4; to_big_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_shx); // file length (BIG)
      int_output = 1000; to_little_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_shx); // version (LITTLE)
      int_output = 31; to_little_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_shx); // shape type (LITTLE) = MultiPatch
      double_output = lasreader->header.min_x; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // xmin (LITTLE)
      double_output = lasreader->header.min_y; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // ymin (LITTLE)
      double_output = lasreader->header.max_x; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // xmax (LITTLE)
      double_output = lasreader->header.max_y; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // ymax (LITTLE)
      double_output = lasreader->header.min_z; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // zmin (LITTLE)
      double_output = lasreader->header.max_z; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // zmax (LITTLE)
      double_output = 0.0; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // mmin (LITTLE)
      double_output = 0.0; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_shx); // mmax (LITTLE)
    }

    // write contents

    int_output = 1; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // record number (BIG)
    int_output = content_length; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // content length (BIG)

    int_output = 31; to_little_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = MultiPatch

    double_output = lasreader->header.min_x; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
    double_output = lasreader->header.min_y; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
    double_output = lasreader->header.max_x; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
    double_output = lasreader->header.max_y; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)

    int_output = nfaces; to_little_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // number of parts (LITTLE)
    int_output = nfaces * 3; to_little_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_out); // number of points (LITTLE)
    // index to first point in each part (here as many parts as triangles)
    for (i = 0; i < nfaces; i++)
    {
      int_output = 3*i; to_little_endian(&int_output); 
      fwrite(&int_output, sizeof(int), 1, file_out); // (LITTLE)
    }
    // type of each part (here each parts is a triangle strip)
    for (i = 0; i < nfaces; i++)
    {
      int_output = 0; to_little_endian(&int_output); 
      fwrite(&int_output, sizeof(int), 1, file_out); // (LITTLE)
    }

    // write points x and y

    t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          for (int j = 0; j < 3; j++)
          {
            double_output = lasreader->header.min_x+t->V[j][0]; to_little_endian(&double_output);
            fwrite(&double_output, sizeof(double), 1, file_out); // x of point (LITTLE)
            double_output = lasreader->header.min_y+t->V[j][1]; to_little_endian(&double_output);
            fwrite(&double_output, sizeof(double), 1, file_out); // y of point (LITTLE)
          }
        }
      }
    }

    // write z
    double_output = lasreader->header.min_z; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
    double_output = lasreader->header.max_z; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)
    t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          for (int j = 0; j < 3; j++)
          {
            double_output = lasreader->header.min_z+t->V[j][2]; to_little_endian(&double_output);
            fwrite(&double_output, sizeof(double), 1, file_out); // z of point (LITTLE)
          }
        }
      }
    }

    // write m

    double_output = 0.0; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // mmin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_out); // mmax (LITTLE)
    for (i = 0; i < nfaces; i++)
    {
      for (int j = 0; j < 3; j++)
      {
        fwrite(&double_output, sizeof(double), 1, file_out); // m of point (LITTLE)
      }
    }

    if (file_shx)
    {
      int int_output;
      int_output = 50; to_big_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_shx); // offset (BIG)
      int_output = content_length; to_big_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_shx); // content length (BIG)
    }
  }

  if (verbose) ptime("done.");

  byebye(argc==1);

  return 0;
}
