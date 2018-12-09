/*
===============================================================================

  FILE:  las2iso.cpp
  
  CONTENTS:
  
    This tool reads LIDAR data in the LAS format, triangulates them into a
    TIN, and then extracs isoline contours from this TIN. The output is either
    a simple list of line segments in ASCII format or some other format that
    is still undefined.

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2009 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
   17 August 2009 -- added the possibility to simplify and clean the contours 
    5 August 2009 -- added the possibility to output in ESRI's Shapeformat 
    6 April 2009 -- created after getting more serious about 1881 Chestnut 
  
===============================================================================
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <hash_map.h>

#include "lasreader.h"
#include "triangulate.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"las2iso -i lidar.las -o contours.shp\n");
  fprintf(stderr,"las2iso -i lidar.las -o contours.shp -simplify 1 -clean 10\n");
  fprintf(stderr,"las2iso -i lidar.las -o contours.shp -every 10\n");
  fprintf(stderr,"las2iso -i lidar.las -o contours.shp -every 2 -simplify 0.5\n");
  fprintf(stderr,"las2iso -i lidar.las -first_only -o contours.txt\n");
  fprintf(stderr,"las2iso -last_only -i lidar.las -o contours.shp\n");
  fprintf(stderr,"las2iso -i lidar.las -keep_class 2 -keep_class 3 -keep_class 9 -otxt > lines.txt\n");
  fprintf(stderr,"las2iso -i lidar.las -keep_class 8 -o contours.shp\n");
  fprintf(stderr,"las2iso -h\n");
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

typedef struct ISOvertex
{
  ISOvertex* next; // also used for efficient memory management
  ISOvertex* prev;
  int edge;
} ISOvertex;

typedef struct ISOcomponent
{
  ISOcomponent* parent; // also used for efficient memory management
  int ref_count;
} ISOcomponent;

// efficient memory allocation

static int vertex_buffer_alloc = 512;
static ISOvertex* vertex_buffer_next = 0;
static int vertex_buffer_size = 0;

static ISOvertex* allocVertex()
{
  if (vertex_buffer_next == 0)
  {
    vertex_buffer_next = (ISOvertex*)malloc(sizeof(ISOvertex)*vertex_buffer_alloc);
    if (vertex_buffer_next == 0)
    {
      fprintf(stderr,"malloc for vertex buffer failed\n");
      return 0;
    }
    for (int i = 0; i < vertex_buffer_alloc; i++)
    {
      vertex_buffer_next[i].next = &(vertex_buffer_next[i+1]);
    }
    vertex_buffer_next[vertex_buffer_alloc-1].next = 0;
    vertex_buffer_alloc = 2*vertex_buffer_alloc;
  }
  // get pointer to next available vertex
  ISOvertex* vertex = vertex_buffer_next;
  vertex_buffer_next = vertex->next;
  // clean vertex
  vertex->next = 0;
  vertex->prev = 0;
  vertex->edge = -1;
  vertex_buffer_size++;
  return vertex;
}

static void deallocVertex(ISOvertex* vertex)
{
  vertex->next = vertex_buffer_next;
  vertex_buffer_next = vertex;
  vertex_buffer_size--;
}

static int component_buffer_alloc = 512;
static ISOcomponent* component_buffer_next = 0;
static int component_buffer_size = 0;

static ISOcomponent* allocComponent()
{
  if (component_buffer_next == 0)
  {
    component_buffer_next = (ISOcomponent*)malloc(sizeof(ISOcomponent)*component_buffer_alloc);
    if (component_buffer_next == 0)
    {
      fprintf(stderr,"malloc for component buffer failed\n");
      return 0;
    }
    for (int i = 0; i < component_buffer_alloc; i++)
    {
      component_buffer_next[i].parent = &(component_buffer_next[i+1]);
    }
    component_buffer_next[component_buffer_alloc-1].parent = 0;
    component_buffer_alloc = 2*component_buffer_alloc;
  }
  // get pointer to next available component
  ISOcomponent* component = component_buffer_next;
  component_buffer_next = component->parent;
  // clean component
  component->parent = 0;
  component->ref_count = 0;
  component_buffer_size++;
  return component;
}

static void deallocComponent(ISOcomponent* component)
{
  component->parent = component_buffer_next;
  component_buffer_next = component;
  component_buffer_size--;
}

typedef hash_map<int, ISOvertex*> my_vertex_hash;

void ptime(const char *const msg)
{
  float t= ((float)clock())/CLOCKS_PER_SEC;
  fprintf(stderr, "cumulative CPU time thru %s = %f\n", msg, t);
}

static void quicksort_for_floats(float* a, int i, int j)
{
  int in_i = i;
  int in_j = j;
  float key = a[(i+j)/2];
  float w;
  do
  {
    while ( a[i] < key ) i++;
    while ( a[j] > key ) j--;
    if (i<j)
    {
      w = a[i];
      a[i] = a[j];
      a[j] = w;
    }
  } while (++i<=--j);
  if (i == j+3)
  {
    i--;
    j++;
  }
  if (j>in_i) quicksort_for_floats(a, in_i, j);
  if (i<in_j) quicksort_for_floats(a, i, in_j);
}

static interpolate(float* vertex, float* v0, float* v1, float iso_value)
{
  // to assure consistent floating-point computation switch points into unique order
  if (v0 < v1) { float* help = v0; v0 = v1; v1 = help; }

  float s1 = (iso_value - v0[2])/(v1[2] - v0[2]);
  float s0 = 1.0f - s1;

  vertex[0] = s0*v0[0]+s1*v1[0];
  vertex[1] = s0*v0[1]+s1*v1[1];
}

// global variables

static char* file_name_out = 0;
static bool otxt = false;
static FILE* file_out = 0;
static FILE* file_shx = 0;
static double bb_min_x_d = 0.0;
static double bb_min_y_d = 0.0;
static float bb_min_f[3];
static float bb_max_f[3];
static int file_length = 50;
static int record_number = 0;
static int smooth = 0;
static float simplify_area = 0.0f;
static float simplify_length = 0.0f;
static float clean = 0.0f;
static int simplified_area = 0;
static int simplified_length = 0;
static int simplified_line = 0;
static int simplified_loop = 0;
static int cleaned_segs = 0;
static int cleaned_lines = 0;
static int cleaned_line_segments = 0;
static int cleaned_loops = 0;
static int cleaned_loop_segments = 0;
static int output_lines = 0;
static int output_line_segments = 0;
static int output_loops = 0;
static int output_loop_segments = 0;

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

static void write_header_shp()
{
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
  int_output = 13; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = PolyLineZ
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
    int int_output;
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
    double double_output = 0.0f; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmax (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymax (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmax (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // mmin (LITTLE)
    fwrite(&double_output, sizeof(double), 1, file_shx); // mmax (LITTLE)
  }
}

void update_header_shp()
{
  int int_output;
  fseek(file_out, 24, SEEK_SET);
  int_output = file_length; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // file length (BIG)

  double double_output;
  fseek(file_out, 36, SEEK_SET);
  double_output = bb_min_x_d + bb_min_f[0]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
  double_output = bb_min_y_d + bb_min_f[1]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
  double_output = bb_min_x_d + bb_max_f[0]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
  double_output = bb_min_y_d + bb_max_f[1]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)
  double_output = bb_min_f[2]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
  double_output = bb_max_f[2]; to_little_endian(&double_output); to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)

  if (file_shx)
  {
    int int_output;
    fseek(file_shx, 24, SEEK_SET);
    int_output = 50 + 4 * record_number; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // file length (BIG)

    double double_output;
    fseek(file_shx, 36, SEEK_SET);
    double_output = bb_min_x_d + bb_min_f[0]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmin (LITTLE)
    double_output = bb_min_y_d + bb_min_f[1]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymin (LITTLE)
    double_output = bb_min_x_d + bb_max_f[0]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // xmax (LITTLE)
    double_output = bb_min_y_d + bb_max_f[1]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // ymax (LITTLE)
    double_output = bb_min_f[2]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmin (LITTLE)
    double_output = bb_max_f[2]; to_little_endian(&double_output); to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_shx); // zmax (LITTLE)
  }
}

static void interpolate(int edge, float iso_value, float* x, float* y)
{
  int t_idx = TIN_TRIANGLE(edge);
  TINtriangle* t = TINget_triangle(t_idx);
  int c = TIN_CORNER(edge);
  float* v0 = t->V[TIN_PREV(c)];
  float* v1 = t->V[TIN_NEXT(c)];

  // to assure consistent floating-point computation switch points into unique order
  if (v0 < v1) { float* help = v0; v0 = v1; v1 = help; }

  float s1 = (iso_value - v0[2])/(v1[2] - v0[2]);
  float s0 = 1.0f - s1;

  *x = s0*v0[0]+s1*v1[0];
  *y = s0*v0[1]+s1*v1[1];
}

static void smooth_terrain_elevation()
{
  int count;
  float elev[3];
  TINtriangle* t = TINget_triangle(0);
  for (count = 0; count < TINget_size(); count++, t++)
  {
    if (t->next < 0) // if not deleted
    {
      if (t->V[0]) // if not infinite
      {
        elev[0] = t->V[0][2];
        elev[1] = t->V[1][2];
        elev[2] = t->V[2][2];
        t->V[0][2] = 0.90f*elev[0]+0.05f*elev[1]+0.05f*elev[2];
        t->V[1][2] = 0.05f*elev[0]+0.90f*elev[1]+0.05f*elev[2];
        t->V[2][2] = 0.05f*elev[0]+0.05f*elev[1]+0.90f*elev[2];
      }
    }
  }
}

static ISOvertex* classify_isocontour(ISOvertex* vertex)
{
  ISOvertex* start = vertex;
  ISOvertex* run = vertex;
  ISOvertex* last = vertex->next;

  // do we have a loop or a line
  do
  {
    if (run->prev == 0)
    {
      // we have a line
      break;
    }
    else
    {
      // we are still looping or lining
      if (run->prev == last)
      {
        last = run;
        run = run->next;
      }
      else
      {
        last = run;
        run = run->prev;
      }
    }
  }  while (run != start);

  return run;
}

static int simplify_isoline_area(ISOvertex* vertex, float simplify_area, float iso_value)
{
  double area;
  float x_last,y_last,x_run,y_run,x_next,y_next;
  int simplified = 0;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;
  ISOvertex* next;

  // make sure it is a line
  assert(vertex->prev == 0);

  interpolate(last->edge, iso_value, &x_run, &y_run);
  interpolate(run->edge, iso_value, &x_next, &y_next);

  while (run->prev)
  {
    x_last = x_run;
    y_last = y_run;
    x_run = x_next;
    y_run = y_next;
    if (run->next == last)
    {
      next = run->prev;
    }
    else
    {
      next = run->next;
    }
    interpolate(next->edge, iso_value, &x_next, &y_next);    
    
    area = fabs((x_last-x_run)*(y_run-y_next)-(y_last-y_run)*(x_run-x_next));
    if (area < simplify_area)
    {
      if (last->next == run)
      {
        last->next = next;
      }
      else
      {
        last->prev = next;
      }
      if (next->next == run)
      {
        next->next = last;
      }
      else
      {
        next->prev = last;
      }
      deallocVertex(run);
      simplified++;
      run = last;
    }
    last = run;
    run = next;
  }
  return simplified;
}

static int simplify_isoline_length(ISOvertex* vertex, float simplify_length, float iso_value)
{
  double length;
  float x_last,y_last,x_run,y_run;
  int simplified = 0;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;
  ISOvertex* next;

  // make sure it is a line
  assert(vertex->prev == 0);

  interpolate(last->edge, iso_value, &x_last, &y_last);

  while (run)
  {
    interpolate(run->edge, iso_value, &x_run, &y_run);
    if (run->next == last)
    {
      next = run->prev;
    }
    else
    {
      next = run->next;
    }    
    length = sqrt((x_last-x_run)*(x_last-x_run)+(y_last-y_run)*(y_last-y_run));
    if (length < simplify_length)
    {
      if (next)
      {
        if (last->next == run)
        {
          last->next = next;
        }
        else
        {
          last->prev = next;
        }
        if (next->next == run)
        {
          next->next = last;
        }
        else
        {
          next->prev = last;
        }
      }
      else
      {
        if (last->next == run)
        {
          last->next = last->prev;
        }
        last->prev = 0;
      }
      deallocVertex(run);
      simplified++;
    }
    else
    {
      last = run;
      x_last = x_run;
      y_last = y_run;
    }
    run = next;
  }
  return simplified;
}

static int clean_isoline(ISOvertex* vertex, float clean_length, float iso_value)
{
  double length = 0.0;
  int cleaned = 0;
  float x_last,y_last,x_run,y_run;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;

  // make sure it is a line
  assert(vertex->prev == 0);

  interpolate(last->edge, iso_value, &x_run, &y_run);
  while (run->prev)
  {
    x_last = x_run;
    y_last = y_run;
    interpolate(run->edge, iso_value, &x_run, &y_run);
    length += sqrt((x_last-x_run)*(x_last-x_run)+(y_last-y_run)*(y_last-y_run));
    if (run->next == last)
    {
      last = run;
      run = run->prev;
    }
    else
    {
      last = run;
      run = run->next;
    }
  }
  x_last = x_run;
  y_last = y_run;
  interpolate(run->edge, iso_value, &x_run, &y_run);
  length += sqrt((x_last-x_run)*(x_last-x_run)+(y_last-y_run)*(y_last-y_run));

  // if the isoline is below the cutoff we deallocate it
  if (length < clean_length)
  {
    last = vertex;
    run = vertex->next;
    while (run->prev)
    {
      deallocVertex(last);
      cleaned++;
      if (run->next == last)
      {
        last = run;
        run = run->prev;
      }
      else
      {
        last = run;
        run = run->next;
      }
    }
    deallocVertex(last);
    cleaned++;
    deallocVertex(run);
  }
  return cleaned;
}

static int output_isoline(ISOvertex* vertex, float iso_value)
{
  int i;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;

  // make sure it is a line
  assert(vertex->prev == 0);

  long record_header_position = ftell(file_out);

  int int_output;
  int_output = 0; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // record number (BIG)
  int_output = 0; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // content length (BIG)

  int_output = 13; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = polyline

  double double_output;
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)

  int_output = 1; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of parts (LITTLE)
  int_output = 0; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of points (LITTLE)
  int_output = 0; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // index to first point in each part (here only one part) (LITTLE)

  // compute the bounding box of the polyline / polyloop
  float xmin, ymin, xmax, ymax;

  // count the length of the polyline / polyloop
  int number_points = 0;

  // write points x and y
  float x,y;

  // we have a line 
  number_points++;
  interpolate(last->edge, iso_value, &x, &y);
  xmin = xmax = x;
  ymin = ymax = y;
  double_output = bb_min_x_d + x; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // x of point (LITTLE)
  double_output = bb_min_y_d + y; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // y of point (LITTLE)
  while (run->prev)
  {
    deallocVertex(last);
    number_points++;
    interpolate(run->edge, iso_value, &x, &y);
    if (x < xmin) xmin = x; else if (x > xmax) xmax = x;
    if (y < ymin) ymin = y; else if (y > ymax) ymax = y;
    double_output = bb_min_x_d + x; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // x of point (LITTLE)
    double_output = bb_min_y_d + y; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // y of point (LITTLE)
    if (run->next == last)
    {
      last = run;
      run = run->prev;
    }
    else
    {
      last = run;
      run = run->next;
    }
  }
  deallocVertex(last);
  number_points++;
  interpolate(run->edge, iso_value, &x, &y);
  if (x < xmin) xmin = x; else if (x > xmax) xmax = x;
  if (y < ymin) ymin = y; else if (y > ymax) ymax = y;
  double_output = bb_min_x_d + x; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // x of point (LITTLE)
  double_output = bb_min_y_d + y; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // y of point (LITTLE)
  deallocVertex(run);

  // write z

  double_output = iso_value; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)
  for (i = 0; i < number_points; i++)
  {
    fwrite(&double_output, sizeof(double), 1, file_out); // all the z's (LITTLE)
  }

  // write m

  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // mmin (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // mmax (LITTLE)
  for (i = 0; i < number_points; i++)
  {
    fwrite(&double_output, sizeof(double), 1, file_out); // all the m's (LITTLE)
  }

  // update global bounding box

  if (record_number == 0)
  {
    bb_min_f[0] = xmin; bb_max_f[0] = xmax;
    bb_min_f[1] = ymin; bb_max_f[1] = ymax;
    bb_min_f[2] = bb_max_f[2] = iso_value;
  }
  else
  {
    if (xmin < bb_min_f[0]) bb_min_f[0] = xmin; else if (xmax > bb_max_f[0]) bb_max_f[0] = xmax;
    if (ymin < bb_min_f[1]) bb_min_f[1] = ymin; else if (ymax > bb_max_f[1]) bb_max_f[1] = ymax;
    if (iso_value < bb_min_f[2]) bb_min_f[2] = iso_value; else if (iso_value > bb_max_f[2]) bb_max_f[2] = iso_value;
  }

  // update record header

  record_number++;
  int content_length = (80 + (number_points * 32)) / 2;
  file_length += (4 + content_length);

  fseek(file_out, record_header_position, SEEK_SET);

  int_output = record_number; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // record number (BIG)
  int_output = content_length; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // content length (BIG)

  int_output = 13; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = polyline

  double_output = bb_min_x_d + xmin; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
  double_output = bb_min_y_d + ymin; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
  double_output = bb_min_x_d + xmax; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
  double_output = bb_min_y_d + ymax; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)

  int_output = 1; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of parts (LITTLE)
  int_output = number_points; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of points (LITTLE)
  int_output = 0; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // index to first point in each part (here only one part) (LITTLE)

  fseek(file_out, 0, SEEK_END);

  if (file_shx)
  {
    int_output = file_length - (4 + content_length); to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // offset (BIG)
    int_output = content_length; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // content length (BIG)
  }
  return number_points-1;
};

static int simplify_isoloop_area(ISOvertex* vertex, float simplify_area, float iso_value)
{
  double area;
  float x_last,y_last,x_run,y_run,x_next,y_next;
  int simplified = 0;
  ISOvertex* start = vertex;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;
  ISOvertex* next;

  // make sure it is a loop
  assert(vertex->prev != 0);

  interpolate(last->edge, iso_value, &x_run, &y_run);
  interpolate(run->edge, iso_value, &x_next, &y_next);

  while (run != start)
  {
    x_last = x_run;
    y_last = y_run;
    x_run = x_next;
    y_run = y_next;
    if (run->next == last)
    {
      next = run->prev;
    }
    else
    {
      next = run->next;
    }
    interpolate(next->edge, iso_value, &x_next, &y_next);    
    
    area = fabs((x_last-x_run)*(y_run-y_next)-(y_last-y_run)*(x_run-x_next));
    if (area < simplify_area)
    {
      if (last->next == run)
      {
        last->next = next;
      }
      else
      {
        last->prev = next;
      }
      if (next->next == run)
      {
        next->next = last;
      }
      else
      {
        next->prev = last;
      }
      deallocVertex(run);
      simplified++;
      run = last;
    }
    last = run;
    run = next;
  }
  return simplified;
}

static int simplify_isoloop_length(ISOvertex* vertex, float simplify_length, float iso_value)
{
  double length;
  float x_last,y_last,x_run,y_run;
  int simplified = 0;
  ISOvertex* start = vertex;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;
  ISOvertex* next;

  // make sure it is a loop
  assert(vertex->prev != 0);

  interpolate(last->edge, iso_value, &x_last, &y_last);

  while (run != start)
  {
    interpolate(run->edge, iso_value, &x_run, &y_run);
    if (run->next == last)
    {
      next = run->prev;
    }
    else
    {
      next = run->next;
    }    
    length = sqrt((x_last-x_run)*(x_last-x_run)+(y_last-y_run)*(y_last-y_run));
    if (length < simplify_length)
    {
      if (last->next == run)
      {
        last->next = next;
      }
      else
      {
        last->prev = next;
      }
      if (next->next == run)
      {
        next->next = last;
      }
      else
      {
        next->prev = last;
      }
      deallocVertex(run);
      simplified++;
    }
    else
    {
      last = run;
      x_last = x_run;
      y_last = y_run;
    }
    run = next;
  }
  return simplified;
}

static int clean_isoloop(ISOvertex* vertex, float clean_length, float iso_value)
{
  double length = 0.0;
  int cleaned = 0;
  float x_last,y_last,x_run,y_run;
  ISOvertex* start = vertex;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;

  // make sure it is a loop
  assert(vertex->prev != 0);

  interpolate(last->edge, iso_value, &x_run, &y_run);
  while (run != start)
  {
    x_last = x_run;
    y_last = y_run;
    interpolate(run->edge, iso_value, &x_run, &y_run);
    length += sqrt((x_last-x_run)*(x_last-x_run)+(y_last-y_run)*(y_last-y_run));
    if (run->next == last)
    {
      last = run;
      run = run->prev;
    }
    else
    {
      last = run;
      run = run->next;
    }
  }
  x_last = x_run;
  y_last = y_run;
  interpolate(run->edge, iso_value, &x_run, &y_run);
  length += sqrt((x_last-x_run)*(x_last-x_run)+(y_last-y_run)*(y_last-y_run));

  // if the isoline is below the cutoff we deallocate it
  if (length < clean_length)
  {
    last = vertex;
    run = vertex->next;
    while (run != start)
    {
      deallocVertex(last);
      cleaned++;
      if (run->next == last)
      {
        last = run;
        run = run->prev;
      }
      else
      {
        last = run;
        run = run->next;
      }
    }
    deallocVertex(last);
    cleaned++;
  }
  return cleaned;
}

static int output_isoloop(ISOvertex* vertex, float iso_value)
{
  int i;
  ISOvertex* start = vertex;
  ISOvertex* last = vertex;
  ISOvertex* run = vertex->next;

  // make sure it is a loop
  assert(vertex->prev);

  long record_header_position = ftell(file_out);

  int int_output;
  int_output = 0; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // record number (BIG)
  int_output = 0; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // content length (BIG)

  int_output = 13; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = polyline

  double double_output;
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)

  int_output = 1; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of parts (LITTLE)
  int_output = 0; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of points (LITTLE)
  int_output = 0; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // index to first point in each part (here only one part) (LITTLE)

  // compute the bounding box of the polyline / polyloop
  float xmin, ymin, xmax, ymax;

  // count the length of the polyline / polyloop
  int number_points = 0;

  // write points x and y
  float x,y;

  // we have a loop
  number_points++;
  interpolate(last->edge, iso_value, &x, &y);
  xmin = xmax = x;
  ymin = ymax = y;
  double_output = bb_min_x_d + x; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // x of point (LITTLE)
  double_output = bb_min_y_d + y; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // y of point (LITTLE)
  while (run != start)
  {
    deallocVertex(last);
    number_points++;
    interpolate(run->edge, iso_value, &x, &y);
    if (x < xmin) xmin = x; else if (x > xmax) xmax = x;
    if (y < ymin) ymin = y; else if (y > ymax) ymax = y;
    double_output = bb_min_x_d + x; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // x of point (LITTLE)
    double_output = bb_min_y_d + y; to_little_endian(&double_output);
    fwrite(&double_output, sizeof(double), 1, file_out); // y of point (LITTLE)
    if (run->next == last)
    {
      last = run;
      run = run->prev;
    }
    else
    {
      last = run;
      run = run->next;
    }
  }
  deallocVertex(last);
  number_points++;
  interpolate(run->edge, iso_value, &x, &y);
  if (x < xmin) xmin = x; else if (x > xmax) xmax = x;
  if (y < ymin) ymin = y; else if (y > ymax) ymax = y;
  double_output = bb_min_x_d + x; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // x of point (LITTLE)
  double_output = bb_min_y_d + y; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // y of point (LITTLE)

  // write z

  double_output = iso_value; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // zmin (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // zmax (LITTLE)
  for (i = 0; i < number_points; i++)
  {
    fwrite(&double_output, sizeof(double), 1, file_out); // all the z's (LITTLE)
  }

  // write m

  double_output = 0.0; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // mmin (LITTLE)
  fwrite(&double_output, sizeof(double), 1, file_out); // mmax (LITTLE)
  for (i = 0; i < number_points; i++)
  {
    fwrite(&double_output, sizeof(double), 1, file_out); // all the m's (LITTLE)
  }

  // update global bounding box

  if (record_number == 0)
  {
    bb_min_f[0] = xmin; bb_max_f[0] = xmax;
    bb_min_f[1] = ymin; bb_max_f[1] = ymax;
    bb_min_f[2] = bb_max_f[2] = iso_value;
  }
  else
  {
    if (xmin < bb_min_f[0]) bb_min_f[0] = xmin; else if (xmax > bb_max_f[0]) bb_max_f[0] = xmax;
    if (ymin < bb_min_f[1]) bb_min_f[1] = ymin; else if (ymax > bb_max_f[1]) bb_max_f[1] = ymax;
    if (iso_value < bb_min_f[2]) bb_min_f[2] = iso_value; else if (iso_value > bb_max_f[2]) bb_max_f[2] = iso_value;
  }

  // update record header

  record_number++;
  int content_length = (80 + (number_points * 32)) / 2;
  file_length += (4 + content_length);

  fseek(file_out, record_header_position, SEEK_SET);

  int_output = record_number; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // record number (BIG)
  int_output = content_length; to_big_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // content length (BIG)

  int_output = 13; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // shape type (LITTLE) = polyline

  double_output = bb_min_x_d + xmin; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmin (LITTLE)
  double_output = bb_min_y_d + ymin; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymin (LITTLE)
  double_output = bb_min_x_d + xmax; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // xmax (LITTLE)
  double_output = bb_min_y_d + ymax; to_little_endian(&double_output);
  fwrite(&double_output, sizeof(double), 1, file_out); // ymax (LITTLE)

  int_output = 1; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of parts (LITTLE)
  int_output = number_points; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // number of points (LITTLE)
  int_output = 0; to_little_endian(&int_output);
  fwrite(&int_output, sizeof(int), 1, file_out); // index to first point in each part (here only one part) (LITTLE)

  fseek(file_out, 0, SEEK_END);

  if (file_shx)
  {
    int_output = file_length - (4 + content_length); to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // offset (BIG)
    int_output = content_length; to_big_endian(&int_output);
    fwrite(&int_output, sizeof(int), 1, file_shx); // content length (BIG)
  }
  return number_points-1;
}

static void extract_lines_shp(float v0z, float v1z, float v2z, int e0, int e1, int e2, float* iso_values, my_vertex_hash* iso_hashes, int number_iso_values)
{
  int i,j;
  float iso_value;
  int linindex;

  // for each iso-value look for intersections (assume increasing order)

  for (i = 0; i < number_iso_values; i++)
  {
    // which vertices are above or below the isosurface
    iso_value = iso_values[i];

    linindex = 0;
    if (v0z < iso_value) linindex |= 1;
    if (v1z < iso_value) linindex |= 2;
    if (v2z < iso_value) linindex |= 4;

    // do we have one of the 6 cases that produce isolines?

    if (linindex == 0x00)
    {
      // all values above isovalue ... continue
      continue;
    }
    else if (linindex == 0x07)
    {
      // all values below isovalue ... break
      break;
    }
    else
    {
      int edges[2];

      switch (linindex)
      {
      case 0x01:
        edges[0] = e2;
        edges[1] = e1;
        // interpolate(vertex, v0, v1, iso_value);    
        // interpolate(vertex_other, v0, v2, iso_value);    
        break;
      case 0x06:
        edges[0] = e1;
        edges[1] = e2;
        // interpolate(vertex, v0, v2, iso_value);    
        // interpolate(vertex_other, v0, v1, iso_value);    
        break;
      case 0x02:
        edges[0] = e0;
        edges[1] = e2;
        // interpolate(vertex, v1, v2, iso_value);    
        // interpolate(vertex_other, v1, v0, iso_value);    
        break;
      case 0x05:
        edges[0] = e2;
        edges[1] = e0;
        // interpolate(vertex, v1, v0, iso_value);    
        // interpolate(vertex_other, v1, v2, iso_value);    
        break;
      case 0x04:
        edges[0] = e1;
        edges[1] = e0;
        // interpolate(vertex, v2, v0, iso_value);    
        // interpolate(vertex_other, v2, v1, iso_value);    
        break;
      case 0x03:
        edges[0] = e0;
        edges[1] = e1;
        // interpolate(vertex, v2, v1, iso_value);    
        // interpolate(vertex_other, v2, v0, iso_value);    
        break;
      }
      my_vertex_hash::iterator hash_elements[2];
      ISOvertex* vertices[2];
      ISOcomponent* components[2];
      for (j = 0; j < 2; j++)
      {
        if (edges[j] >= 0) // interior edge
        {
          hash_elements[j] = iso_hashes[i].find(edges[j]);
          if (hash_elements[j] == iso_hashes[i].end())
          {
            vertices[j] = allocVertex();
            vertices[j]->edge = edges[j];
            iso_hashes[i].insert(my_vertex_hash::value_type(edges[j], vertices[j]));
            components[j] = 0;
          }
          else
          {
            vertices[j] = (*(hash_elements[j])).second;
            iso_hashes[i].erase(hash_elements[j]);
            components[j] = (ISOcomponent*)(vertices[j]->prev);
            vertices[j]->prev = 0;
            components[j]->ref_count--;
            while (components[j]->parent)
            {
              ISOcomponent* parent = components[j]->parent;
              if (components[j]->ref_count == 0)
              {
                parent->ref_count--;
                deallocComponent(components[j]);
              }
              components[j] = parent;
            }
          }
        }
        else // boundary edge
        {
          vertices[j] = allocVertex();
          vertices[j]->edge = -edges[j]-1;
          components[j] = 0;
        }
      }
      
      if (components[0] == 0 && components[1] == 0) // if neither isovertex has a component
      {
        components[0] = allocComponent();
        vertices[0]->next = vertices[1];
        vertices[1]->next = vertices[0];
        if (edges[0] >= 0)
        {
          vertices[0]->prev = (ISOvertex*)components[0];
          components[0]->ref_count++;
        }
        if (edges[1] >= 0)
        {
          vertices[1]->prev = (ISOvertex*)components[0];
          components[0]->ref_count++;
        }
      }
      else if (components[1] == 0) // if only vertices[0] has a component
      {
        vertices[0]->prev = vertices[1];
        vertices[1]->next = vertices[0];
        if (edges[1] >= 0)
        {
          vertices[1]->prev = (ISOvertex*)components[0];
          components[0]->ref_count++;
        }
      }
      else if (components[0] == 0) // if only vertices[1] has a component
      {
        vertices[0]->next = vertices[1];
        vertices[1]->prev = vertices[0];
        if (edges[0] >= 0)
        {
          vertices[0]->prev = (ISOvertex*)components[1];
          components[1]->ref_count++;
        }
        components[0] = components[1];
      }
      else // if both vertices have a component
      {
        vertices[0]->prev = vertices[1];
        vertices[1]->prev = vertices[0];
        if (components[0] != components[1]) // if the component is different
        {
          if (components[1]->ref_count)
          {
            components[1]->parent = components[0];
            components[0]->ref_count++;
          }
          else
          {
            deallocComponent(components[1]);
          }
        }
      }

      if (components[0]->ref_count == 0)
      {
        deallocComponent(components[0]);

        vertices[0] = classify_isocontour(vertices[0]);

        if (vertices[0]->prev == 0) // we have an isoline
        {
          if (simplify_area)
          {
            simplified_area += simplify_isoline_area(vertices[0], simplify_area, iso_value);
          }
          if (simplify_length)
          {
            simplified_length += simplify_isoline_length(vertices[0], simplify_length, iso_value);
            if (vertices[0]->next == 0)
            {
              deallocVertex(vertices[0]);
              simplified_line++;
              continue;
            }
          }
          if (clean)
          {
            cleaned_segs = clean_isoline(vertices[0], clean, iso_value);
          }
          if (cleaned_segs)
          {
            cleaned_lines++;
            cleaned_line_segments += cleaned_segs;
          }
          else
          {
            output_line_segments += output_isoline(vertices[0], iso_value);
            output_lines++;
          }
        }
        else // we have an isoloop
        {
          if (simplify_area)
          {
            simplified_area += simplify_isoloop_area(vertices[0], simplify_area, iso_value);
          }
          if (simplify_length)
          {
            simplified_length += simplify_isoloop_length(vertices[0], simplify_length, iso_value);
            if (vertices[0]->next == vertices[0])
            {
              deallocVertex(vertices[0]);
              simplified_loop++;
              continue;
            }
          }
          if (clean)
          {
            cleaned_segs = clean_isoloop(vertices[0], clean, iso_value);
          }
          if (cleaned_segs)
          {
            cleaned_loops++;
            cleaned_loop_segments += cleaned_segs;
          }
          else
          {
            output_loop_segments += output_isoloop(vertices[0], iso_value);
            output_loops++;
          }
        }
      }
    }
  }
}

static void extract_lines_txt(float* v0, float* v1, float* v2, float* iso_values, int number_iso_values)
{
  int i;
  float iso_value;
  int linindex;

  // for each iso-value look for intersections (assume increasing order)

  for (i = 0; i < number_iso_values; i++)
  {
    // which vertices are above or below the isosurface
    iso_value = iso_values[i];

    linindex = 0;
    if (v0[2] < iso_value) linindex |= 1;
    if (v1[2] < iso_value) linindex |= 2;
    if (v2[2] < iso_value) linindex |= 4;

    // do we have one of the 6 cases that produce isolines?

    if (linindex == 0x00)
    {
      // all values above isovalue ... continue
      continue;
    }
    else if (linindex == 0x07)
    {
      // all values below isovalue ... break
      break;
    }
    else
    {
      float vertex[2];
      float vertex_other[2];

      switch (linindex)
      {
      case 0x01:
        interpolate(vertex, v0, v1, iso_value);    
        interpolate(vertex_other, v0, v2, iso_value);    
        break;
      case 0x06:
        interpolate(vertex, v0, v2, iso_value);    
        interpolate(vertex_other, v0, v1, iso_value);    
        break;
      case 0x02:
        interpolate(vertex, v1, v2, iso_value);    
        interpolate(vertex_other, v1, v0, iso_value);    
        break;
      case 0x05:
        interpolate(vertex, v1, v0, iso_value);    
        interpolate(vertex_other, v1, v2, iso_value);    
        break;
      case 0x04:
        interpolate(vertex, v2, v0, iso_value);    
        interpolate(vertex_other, v2, v1, iso_value);    
        break;
      case 0x03:
        interpolate(vertex, v2, v1, iso_value);    
        interpolate(vertex_other, v2, v0, iso_value);    
        break;
      }
      fprintf(file_out, "%.10g %.10g %.8g %.10g %.10g %.8g\012", bb_min_x_d+vertex[0], bb_min_y_d+vertex[1], iso_value, bb_min_x_d+vertex_other[0], bb_min_y_d+vertex_other[1], iso_value);
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
  float iso_values[512];
  int iso_value_number = 0;
  int iso_value_count = 0;
  int iso_value_create = 0;
  double iso_spacing = 0.0;

  if (argc == 1)
  {
    fprintf(stderr,"las2iso.exe is better run in the command line\n");
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
    else if (strcmp(argv[i],"-simplify_area") == 0 || strcmp(argv[i],"-area") == 0 || strcmp(argv[i],"-simplify_bump") == 0 ||strcmp(argv[i],"-bump") == 0)
    {
      i++;
      simplify_area = atof(argv[i]);
    }
    else if (strcmp(argv[i],"-simplify_length") == 0 || strcmp(argv[i],"-length") == 0 || strcmp(argv[i],"-simplify") == 0)
    {
      i++;
      simplify_length = atof(argv[i]);
    }
    else if (strcmp(argv[i],"-clean") == 0)
    {
      i++;
      clean = atof(argv[i]);
    }
    else if (strcmp(argv[i],"-smooth") == 0)
    {
      i++;
      smooth = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-w") == 0 || strcmp(argv[i],"-value") == 0)
    {
      i++;
      if (iso_value_number < 512)
      {
        sscanf(argv[i], "%f", &(iso_values[iso_value_number]));
        iso_value_number++;
      }
      else
      {
        fprintf(stderr,"ERROR: more than 512 iso values\n");
        byebye(argc==1);
      }
    }
    else if (strcmp(argv[i],"-every") == 0 || strcmp(argv[i],"-feet") == 0 || strcmp(argv[i],"-meter") == 0)
    {
      i++;
      iso_spacing = atof(argv[i]);
    }
    else if (strcmp(argv[i],"-range") == 0)
    {
      float iso_from, iso_to, iso_step;
      i++;
      sscanf(argv[i], "%f", &iso_from);
      i++;
      sscanf(argv[i], "%f", &iso_to);
      i++;
      sscanf(argv[i], "%f", &iso_step);

      while (iso_from <= iso_to)
      {
        if (iso_value_number < 256)
        {
          iso_values[iso_value_number] = iso_from;
          iso_value_number++;
        }
        else
        {
          fprintf(stderr,"ERROR: more than 512 iso values\n");
          byebye(argc==1);
        }
        iso_from += iso_step;
      }
    }
    else if (strcmp(argv[i],"-number") == 0)
    {
      i++;
      iso_value_create = atoi(argv[i]);
      if (iso_value_create > 512)
      {
        fprintf(stderr,"ERROR: more than 512 iso values\n");
        byebye(argc==1);
      }
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

  // if needed set the default
  if (iso_value_number == 0 && iso_value_create == 0)
  {
    iso_value_create = 10;
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

  // loop over points, store points, and triangulate

  int count = 0;
  float* point_buffer = (float*)malloc(sizeof(float)*3*npoints);
  int* index_map = (npoints == lasreader->npoints ? 0 : (int*)malloc(sizeof(int)*npoints));
  
  TINclean(npoints);

  fprintf(stderr, "reading %d points and triangulating %d points\n", lasreader->npoints, npoints);
  if (verbose) ptime("start triangulation pass.");

  bb_min_x_d = lasreader->header.min_x;
  bb_min_y_d = lasreader->header.min_y;

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
    point_buffer[3*count+0] = (float)(coordinates[0] - bb_min_x_d);
    point_buffer[3*count+1] = (float)(coordinates[1] - bb_min_y_d);
    point_buffer[3*count+2] = (float)(coordinates[2]);
    if (index_map) index_map[count] = lasreader->p_count -1;
    // add the point to the triangulation
    TINadd(&(point_buffer[3*count]));
    count++;
  }
  TINfinish();

  lasreader->close();
  fclose(file_in);

  if (smooth) fprintf(stderr, "smoothing terrain with %d passes because of '-smooth %d'\n",smooth,smooth);

  while (smooth)
  {
    smooth_terrain_elevation();
    smooth--;
  }

  if (verbose) ptime("start output.");

  // open output file

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
  else if (otxt)
  {
    file_out = stdout;
  }
  else
  {
    fprintf (stderr, "ERROR: no output specified\n");
    byebye(argc==1);
  }

  // prepare isovalues

  if (iso_spacing)
  {
    double start = ((int)(lasreader->header.min_z/iso_spacing))*iso_spacing;
    double run = start;
    while (run < lasreader->header.max_z)
    {
      iso_values[iso_value_number] = (float)run;
      run += iso_spacing;
      iso_value_number++;
    }
    fprintf(stderr,"extracting contours at %d elevations starting from %g with spacing of %g.\n", iso_value_number, start, iso_spacing);
  }
  else if (iso_value_create)
  {
    double spacing = (lasreader->header.max_z - lasreader->header.min_z) / (iso_value_create + 1);
    double start = lasreader->header.min_z + spacing/2;
    double run = start;
    for (i = 0; i < iso_value_create; i++)
    {
      iso_values[iso_value_number] = (float)run;
      run += spacing;
      iso_value_number++;
    }
    fprintf(stderr,"extracting contours at %d elevations starting from %g with spacing of %g.\n", iso_value_create, start, spacing);
  }

  quicksort_for_floats(iso_values, 0, iso_value_number-1);

  // extract line segments

  if ((file_name_out && strstr(file_name_out, ".txt")) || otxt)
  {
    // loop over triangles and output line segments
    TINtriangle* t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          extract_lines_txt(t->V[0], t->V[1], t->V[2], iso_values, iso_value_number);
        }
      }
    }
  }
  else if (file_name_out && strstr(file_name_out, ".shx"))
  {
    int e[3];
    // output SHP file header
    write_header_shp();
    // create hash tables
    my_vertex_hash* iso_hashes = new my_vertex_hash[iso_value_number];
    // loop over triangles and output line segments
    TINtriangle* t = TINget_triangle(0);
    for (count = 0; count < TINget_size(); count++, t++)
    {
      if (t->next < 0) // if not deleted
      {
        if (t->V[0]) // if not infinite
        {
          for (i = 0; i < 3; i++)
          {
            e[i] = t->N[i];
            int t_idx = TIN_TRIANGLE(e[i]);
            TINtriangle* h = TINget_triangle(t_idx);
            if (h->V[0]) // if not infinite
            {
              int e_idx = TIN_CORNER(e[i]);
              if (h->N[e_idx] < e[i])
              {
                e[i] = h->N[e_idx];
              }
            }
            else
            {
              e[i] = -1-e[i];
            }
          }
          extract_lines_shp(t->V[0][2], t->V[1][2], t->V[2][2], e[0], e[1], e[2], iso_values, iso_hashes, iso_value_number);
        }
      }
    }
    // update SHP file header
    update_header_shp();

    if (verbose) fprintf(stderr, "vertex_buffer_size %d component_buffer_size %d\n",vertex_buffer_size,component_buffer_size);

    if (simplify_area) fprintf(stderr, "simplified away %d segments with '-simplify_area %g'\n",simplified_area,simplify_area);
    if (simplify_length) fprintf(stderr, "simplified away %d segments (including %d lines and %d loops) with '-simplify %g'\n",simplified_length,simplified_line,simplified_loop,simplify_length);
    if (clean) fprintf(stderr, "cleaned away %d lines with %d segments and %d loops with %d segments with '-clean %g'\n",cleaned_lines,cleaned_line_segments,cleaned_loops,cleaned_loop_segments,clean);
    fprintf(stderr, "output %d lines with %d segments and %d loops with %d segments\n",output_lines,output_line_segments,output_loops,output_loop_segments);
  }

  if (verbose) ptime("done.");

  if (file_out && file_out != stdout) fclose(file_out);

  byebye(argc==1);

  return 0;
}
