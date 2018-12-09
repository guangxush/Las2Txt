/*
===============================================================================

  FILE:  las2shp.cpp
  
  CONTENTS:
  
    This tool converts LIDAR data from the binary LAS format to the bulky
    ESRI Shapefile format using the MultiPointZ primitive.

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

#include "lasreader.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"las2shp -i lidar.las -o lidar.shp\n");
  fprintf(stderr,"las2shp -i lidar.las -o lidar.shp -record 2048\n");
  fprintf(stderr,"las2shp -h\n");
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

static int lidardouble2string(char* string, double value)
{
  int len;
  len = sprintf(string, "%f", value) - 1;
  while (string[len] == '0') len--;
  if (string[len] != '.') len++;
  string[len] = '\0';
  return len;
}

static int lidardouble2string(char* string, double value0, double value1, double value2, bool eol)
{
  int len;
  len = sprintf(string, "%f", value0) - 1;
  while (string[len] == '0') len--;
  if (string[len] != '.') len++;
  len += sprintf(&(string[len]), " %f", value1) - 1;
  while (string[len] == '0') len--;
  if (string[len] != '.') len++;
  len += sprintf(&(string[len]), " %f", value2) - 1;
  while (string[len] == '0') len--;
  if (string[len] != '.') len++;
  if (eol) string[len++] = '\012';
  string[len] = '\0';
  return len;
}

static void to_big_endian(int* value)
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

static void to_little_endian(int* value)
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

static void to_big_endian(double* value)
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

static void to_little_endian(double* value)
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
  bool ilas = false;
  bool verbose = false;
  char* file_name_in = 0;
  char* file_name_out = 0;
  char printstring[256];
  int point_buffer_max = 1024;

  if (argc == 1)
  {
    fprintf(stderr,"las2shp.exe is better run in the command line\n");
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
    else if (strcmp(argv[i],"-ilas") == 0)
    {
      ilas = true;
    }
    else if (strcmp(argv[i],"-record_size") == 0 || strcmp(argv[i],"-record") == 0)
    {
      i++;
      point_buffer_max = atoi(argv[i]);
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

  if (ilas)
  {
    // input from stdin
    file_in = stdin;
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

  LASreader* lasreader = new LASreader();
  if (lasreader->open(file_in) == false)
  {
    fprintf (stderr, "ERROR: lasreader open failed for '%s'\n", file_name_in);
    usage(argc==1);
  }

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
    file_name_out[len+1] = 's';
    file_name_out[len+2] = 'h';
    file_name_out[len+3] = 'p';
    file_name_out[len+4] = '\0';
  }

  // open output SHP file
  
  FILE* file_out = fopen(file_name_out, "wb");

  // fail if output SHP file does not open

  if (file_out == 0)
  {
    fprintf(stderr, "ERROR: could not open '%s' for write\n",file_name_out);
    usage(argc==1);
  }

  // create shx file name

  file_name_out[strlen(file_name_out)-3] = 's';
  file_name_out[strlen(file_name_out)-2] = 'h';
  file_name_out[strlen(file_name_out)-1] = 'x';

  // open output SHX file

  FILE* file_shx = fopen(file_name_out, "wb");

  // write SHP header
  int record_number = 0;
  int file_length = 50;

  int int_output;
  int_output = 9994; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // file code (BIG)
  int_output = 0; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
  fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
  fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
  fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
  fwrite(&int_output, sizeof(int), 1, file_out); // unused (BIG)
  int_output = file_length; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // file length (BIG)
  int_output = 1000; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // version (LITTLE)
  int_output = 18; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = MultiPointZ
  double double_output = 0.0f; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // mmin (LITTLE)
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
    int_output = file_length; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // file length (BIG)
    int_output = 1000; to_little_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // version (LITTLE)
    int_output = 13; to_little_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // shape type (LITTLE) = PolyLineZ
    double_output = 0.0f; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmax (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymax (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmax (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // mmin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // mmax (LITTLE)
  }

  // output header info

  if (verbose)
  {
    LASheader* header = &(lasreader->header);
    fprintf(stderr, "file signature:            '%s'\n", header->file_signature);
    fprintf(stderr, "file source ID:            %d\n", header->file_source_id);
    fprintf(stderr, "reserved (global encoding):%d\n", header->global_encoding);
    fprintf(stderr, "project ID GUID data 1-4:  %d %d %d '%s'\n", header->project_ID_GUID_data_1, header->project_ID_GUID_data_2, header->project_ID_GUID_data_3, header->project_ID_GUID_data_4);
    fprintf(stderr, "version major.minor:       %d.%d\n", header->version_major, header->version_minor);
    fprintf(stderr, "system_identifier:         '%s'\n", header->system_identifier);
    fprintf(stderr, "generating_software:       '%s'\n", header->generating_software);
    fprintf(stderr, "file creation day/year:    %d/%d\n", header->file_creation_day, header->file_creation_year);
    fprintf(stderr, "header size                %d\n", header->header_size);
    fprintf(stderr, "offset to point data       %d\n", header->offset_to_point_data);
    fprintf(stderr, "number var. length records %d\n", header->number_of_variable_length_records);
    fprintf(stderr, "point data format          %d\n", header->point_data_format);
    fprintf(stderr, "point data record length   %d\n", header->point_data_record_length);
    fprintf(stderr, "number of point records    %d\n", header->number_of_point_records);
    fprintf(stderr, "number of points by return %d %d %d %d %d\n", header->number_of_points_by_return[0], header->number_of_points_by_return[1], header->number_of_points_by_return[2], header->number_of_points_by_return[3], header->number_of_points_by_return[4]);
    fprintf(stderr, "scale factor x y z         "); lidardouble2string(printstring, header->x_scale_factor, header->y_scale_factor, header->z_scale_factor, true); fprintf(stderr, printstring);
    fprintf(stderr, "offset x y z               "); lidardouble2string(printstring, header->x_offset, header->y_offset, header->z_offset, true); fprintf(stderr, printstring);
    fprintf(stderr, "min x y z                  "); lidardouble2string(printstring, header->min_x, header->min_y, header->min_z, true); fprintf(stderr, printstring);
    fprintf(stderr, "max x y z                  "); lidardouble2string(printstring, header->max_x, header->max_y, header->max_z, true); fprintf(stderr, printstring);
  }

  // read and convert the points to ESRI's Shapefile format
  int point_buffer_num = point_buffer_max;
  double* point_buffer = new double[3*point_buffer_max];
  double bb_min_d[3];
  double bb_max_d[3];

  while (point_buffer_num == point_buffer_max)
  {
    point_buffer_num = 0;
    while (lasreader->read_point(&(point_buffer[point_buffer_num*3])))
    {
      point_buffer_num++;
      if (point_buffer_num == point_buffer_max) break;
    }

    if (point_buffer_num)
    {
      double min_d[3];
      double max_d[3];

      VecCopy3dv(min_d, point_buffer);
      VecCopy3dv(max_d, point_buffer);

      for (i = 1; i < point_buffer_num; i++)
      {
        VecUpdateMinMax3dv(min_d, max_d, &(point_buffer[3*i]));
      }

      if (record_number == 0)
      {
        VecCopy3dv(bb_min_d, min_d);
        VecCopy3dv(bb_max_d, max_d);
      }
      else
      {
        VecUpdateMinMax3dv(bb_min_d, bb_max_d, min_d);
        VecUpdateMinMax3dv(bb_min_d, bb_max_d, max_d);
      }

      record_number++;
      int content_length = (72 + point_buffer_num * 32) / 2;
      file_length += (4 + content_length);

      int int_output;
      int_output = record_number; to_big_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_out); // record number (BIG)
      int_output = content_length; to_big_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_out); // content length (BIG)

      int_output = 18; to_little_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_out); // shape type MultiPointZ (LITTLE)

      double double_output;
      double_output = min_d[0]; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
      double_output = min_d[1]; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
      double_output = max_d[0]; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
      double_output = max_d[1]; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)

      int_output = point_buffer_num; to_little_endian(&int_output);
      fwrite(&int_output, sizeof(int), 1, file_out); // number of points (LITTLE)

      for (i = 0; i < point_buffer_num; i++)
      {
        double_output = point_buffer[3*i+0]; to_little_endian(&double_output);
        fwrite(&double_output, sizeof(double), 1, file_out); // x (LITTLE)
        double_output = point_buffer[3*i+1]; to_little_endian(&double_output);
        fwrite(&double_output, sizeof(double), 1, file_out); // y (LITTLE)
      }

      double_output = min_d[2]; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
      double_output = max_d[2]; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)

      for (i = 0; i < point_buffer_num; i++)
      {
        double_output = point_buffer[3*i+2]; to_little_endian(&double_output);
        fwrite(&double_output, sizeof(double), 1, file_out); // z (LITTLE)
      }

      double_output = 0.0; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // mmin (LITTLE)
      double_output = 0.0; to_little_endian(&double_output);
      fwrite(&double_output, sizeof(double), 1, file_out); // mmax (LITTLE)

      for (i = 0; i < point_buffer_num; i++)
      {
        double_output = 0.0; to_little_endian(&double_output);
        fwrite(&double_output, sizeof(double), 1, file_out); // m (LITTLE)
      }
    }
  }

  delete [] point_buffer;

  // update SHP header

  fseek(file_out, 24, SEEK_SET);
  int_output = file_length; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // file length (BIG)

  fseek(file_out, 36, SEEK_SET);
  double_output = bb_min_d[0]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
  double_output = bb_min_d[1]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
  double_output = bb_max_d[0]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
  double_output = bb_max_d[1]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)
  double_output = bb_min_d[2]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
  double_output = bb_max_d[2]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)

  if (file_shx)
  {
    fseek(file_shx, 24, SEEK_SET);
    int_output = 50 + 4 * record_number; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // file length (BIG)

    fseek(file_shx, 36, SEEK_SET);
    double_output = bb_min_d[0]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmin (LITTLE)
    double_output = bb_min_d[1]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymin (LITTLE)
    double_output = bb_max_d[0]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmax (LITTLE)
    double_output = bb_max_d[1]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymax (LITTLE)
    double_output = bb_min_d[2]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmin (LITTLE)
    double_output = bb_max_d[2]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmax (LITTLE)
  }

  // close the reader

  lasreader->close();

  // close the input file
  
  fclose(file_in);

  // close the output file

  fclose(file_out);

  fprintf(stderr,"converted %d points to SHP.\n", lasreader->npoints);

  byebye(argc==1);

  return 0;
}
