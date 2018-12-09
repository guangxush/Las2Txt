/*
===============================================================================

  FILE:  lasview.cpp
  
  CONTENTS:
  
    This little tool is just a quick little hack to visualize an LAS files.
    Notice that we tranlate all the lidar points such that the min coordinate
    of the bounding box becomes the origin. This is an easy way to render and
    compute with floating point numbers without too much round-off errors.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2008 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    6 August 2009 -- added 'skip_all_headers' to deal with LAS files with bad headers
   12 March 2009 -- updated to ask for input if started without arguments 
   17 September 2008 -- added the option to display per vertex colors
   12 May 2008 -- added the option to compute and display a TIN
   11 May 2008 -- added the option to display points by classification
    9 May 2007 -- adapted from my streaming point viewer
  
===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <GL/glut.h>

#include "lasreader.h"
#include "triangulate.h"

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

// MOUSE INTERFACE

static int LeftButtonDown=0;
static int MiddleButtonDown=0;
static int RightButtonDown=0;
static int OldX,OldY;
static float Elevation=0;
static float Azimuth=0;
static float DistX=0;
static float DistY=0;
static float DistZ=2;

// VISUALIZATION SETTINGS

static float boundingBoxMin[3];
static float boundingBoxMax[3];
static float boundingBoxHeight;
static float boundingBoxScale=1.0f;
static float boundingBoxTranslateX = 0.0f;
static float boundingBoxTranslateY = 0.0f;
static float boundingBoxTranslateZ = 0.0f;

// GLOBAL CONTROL VARIABLES

static int WindowW=1024, WindowH=768;
static int InteractionMode=0;
static int AnimationOn=0;
static int WorkingOn=0;

// COLORS

static float classification_colors[16][3];
static float colours_white[4];
static float colours_light_blue[4];

// DATA STORAGE FOR VISUALIZER

static FILE* file = 0;
static char* file_name = 0;
static LASreader* lasreader = 0;

static bool byebye_wait = false;
static bool skip_all_headers = false;

static int p_count = 0;
static int npoints = 0;
static float* point_buffer = 0;
static int point_buffer_alloc = 0;
static unsigned char* point_properties = 0;
static unsigned char* point_rgb = 0;
static bool scale_rgb_down = false;

typedef enum RenderOnly {
  ONLY_ALL = 0,
  ONLY_FIRST,
  ONLY_LAST,
  ONLY_GROUND,
  ONLY_OBJECT,
  ONLY_BUILDING,
  ONLY_VEGETATION,
  ONLY_MASS_POINTS,
  ONLY_WATER,
  ONLY_UNCLASSIFIED,
  ONLY_OVERLAP
} RenderOnly;

static RenderOnly render_only = ONLY_ALL;

static int EXACTLY_N_STEPS = 50;
static int EVERY_NTH_STEP = -1;
static int NEXT_STEP;

static int EXACTLY_N_POINTS = 1000000;
static int EVERY_NTH_POINT = 0;
static int NEXT_POINT;

static int DIRTY_POINTS=1;
static int REPLAY_IT=0;
static int REPLAY_COUNT=0;
static int COLORING_MODE = 2;
static int SHADING_MODE = 0;
static int POINT_SIZE = 2;
static int RENDER_BOUNDINGBOX = 1;
static int EXTRA_Z_SCALE = 1;
static int EXTRA_XY_SCALE = 1;

static void InitColors()
{
  classification_colors[0][0] = 0.0f; classification_colors[0][1] = 0.0f; classification_colors[0][2] = 0.0f; // created (black) 
  classification_colors[1][0] = 0.3f; classification_colors[1][1] = 0.3f; classification_colors[1][2] = 0.3f; // unclassified (grey)
  classification_colors[2][0] = 0.7f; classification_colors[2][1] = 0.5f; classification_colors[2][2] = 0.5f; // ground (brown)
  classification_colors[3][0] = 0.0f; classification_colors[3][1] = 0.8f; classification_colors[3][2] = 0.0f; // vegetation low (green)
  classification_colors[4][0] = 0.2f; classification_colors[4][1] = 0.8f; classification_colors[4][2] = 0.2f; // vegetation medium (green)
  classification_colors[5][0] = 0.4f; classification_colors[5][1] = 0.8f; classification_colors[5][2] = 0.4f; // vegetation high (green)
  classification_colors[6][0] = 0.2f; classification_colors[6][1] = 0.2f; classification_colors[6][2] = 0.8f; // building (light blue)
  classification_colors[7][0] = 0.9f; classification_colors[7][1] = 0.4f; classification_colors[7][2] = 0.7f; // lowpoint (violett)
  classification_colors[8][0] = 1.0f; classification_colors[8][1] = 0.0f; classification_colors[8][2] = 0.0f; // mass point (red)
  classification_colors[9][0] = 0.0f; classification_colors[9][1] = 0.0f; classification_colors[9][2] = 1.0f; // water (blue)
  for (int i = 10; i < 16; i++)
  {
    classification_colors[i][0] = 0.3f; classification_colors[i][1] = 0.3f; classification_colors[i][2] = 0.3f; // user-defined (grey)
  }
  classification_colors[12][0] = 1.0f; classification_colors[12][1] = 1.0f; classification_colors[12][2] = 0.0f; // overlap (yellow)

  colours_white[0] = 0.7f; colours_white[1] = 0.7f; colours_white[2] = 0.7f; colours_white[3] = 1.0f; // white
  colours_light_blue[0] = 0.2f; colours_light_blue[1] = 0.2f; colours_light_blue[2] = 0.6f; colours_light_blue[3] = 1.0f; // light blue
}

static void InitLight()
{
  float intensity[] = {1,1,1,1};
  float position[] = {1,1,5,0}; // directional behind the viewer
  glLightfv(GL_LIGHT0,GL_DIFFUSE,intensity);
  glLightfv(GL_LIGHT0,GL_SPECULAR,intensity);
  glLightfv(GL_LIGHT0,GL_POSITION,position);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_FALSE);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
}

static void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"lasview -i terrain.las\n");
  fprintf(stderr,"lasview -i terrain.las -win 1600 1200 -steps 10 -points 200000\n");
  fprintf(stderr,"lasview -h\n");
  fprintf(stderr,"\n");
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

static void vizBegin()
{
  REPLAY_IT = 0; // just making sure
  DIRTY_POINTS = 1;

  if (file_name)
  {
    fprintf(stderr,"loading '%s'...\n",file_name);
    if (strstr(file_name, ".gz"))
    {
#ifdef _WIN32
      file = fopenGzipped(file_name, "rb");
#else
      fprintf(stderr,"ERROR: cannot open gzipped file %s\n",file_name);
      byebye(byebye_wait);
#endif
    }
    else
    {
      file = fopen(file_name, "rb");
    }
    if (file == 0)
    {
      fprintf(stderr,"ERROR: cannot open %s\n",file_name);
      byebye(byebye_wait);
    }
    lasreader = new LASreader();
    if (lasreader->open(file, skip_all_headers) == false)
    {
      fprintf(stderr, "ERROR: could not open lasreader\n");
      byebye(byebye_wait);
    }
  }
  else
  {
    fprintf(stderr,"ERROR: no input\n");
    byebye(byebye_wait);
  }
  
  // scale and translate bounding box for rendering

  boundingBoxMin[0] = 0;
  boundingBoxMin[1] = 0;
  boundingBoxMin[2] = 0;

  boundingBoxMax[0] = lasreader->header.max_x - lasreader->header.min_x;
  boundingBoxMax[1] = lasreader->header.max_y - lasreader->header.min_y;
  boundingBoxMax[2] = lasreader->header.max_z - lasreader->header.min_z;

  boundingBoxHeight = boundingBoxMax[2]-boundingBoxMin[2];

  if ((boundingBoxMax[1]-boundingBoxMin[1]) > (boundingBoxMax[0]-boundingBoxMin[0]))
  {
    if ((boundingBoxMax[1]-boundingBoxMin[1]) > (boundingBoxMax[2]-boundingBoxMin[2]))
    {
      boundingBoxScale = 1.0f/(boundingBoxMax[1]-boundingBoxMin[1]);
    }
    else
    {
      boundingBoxScale = 1.0f/(boundingBoxMax[2]-boundingBoxMin[2]);
    }
  }
  else
  {
    if ((boundingBoxMax[0]-boundingBoxMin[0]) > (boundingBoxMax[2]-boundingBoxMin[2]))
    {
      boundingBoxScale = 1.0f/(boundingBoxMax[0]-boundingBoxMin[0]);
    }
    else
    {
      boundingBoxScale = 1.0f/(boundingBoxMax[2]-boundingBoxMin[2]);
    }
  }
  boundingBoxTranslateX = - boundingBoxScale * (boundingBoxMin[0] + 0.5f * (boundingBoxMax[0]-boundingBoxMin[0]));
  boundingBoxTranslateY = - boundingBoxScale * (boundingBoxMin[1] + 0.5f * (boundingBoxMax[1]-boundingBoxMin[1]));
  boundingBoxTranslateZ = - boundingBoxScale * (boundingBoxMin[2] + 0.5f * (boundingBoxMax[2]-boundingBoxMin[2]));

  p_count = 0;
  npoints = lasreader->npoints;

  fprintf(stderr,"number of points in file %d\n", npoints);

  if (EVERY_NTH_STEP == -1)
  {
    EVERY_NTH_STEP = npoints / EXACTLY_N_STEPS;
  }
  if (EVERY_NTH_STEP == 0)
  {
    EVERY_NTH_STEP = 1;
  }
  NEXT_STEP = EVERY_NTH_STEP;

  if (EXACTLY_N_POINTS)
  {
    EVERY_NTH_POINT = npoints/EXACTLY_N_POINTS;
  }
  if (EVERY_NTH_POINT == 0)
  {
    EVERY_NTH_POINT = 1;
  }
  NEXT_POINT = 0;

  // make sure we have enough memory

  if (point_buffer_alloc < ((npoints / EVERY_NTH_POINT) + 500))
  {
    if (point_buffer) free(point_buffer);
    if (point_properties) free(point_properties);
    point_buffer_alloc = ((npoints / EVERY_NTH_POINT) + 500);
    point_buffer = (float*)malloc(sizeof(float)*3*point_buffer_alloc);
    point_properties = (unsigned char*)malloc(sizeof(unsigned char)*point_buffer_alloc);
    if (lasreader->points_have_rgb)
    {
      if (point_rgb) free(point_rgb);
      point_rgb = (unsigned char*)malloc(sizeof(unsigned char)*3*point_buffer_alloc);
    }
  }

  if (lasreader->points_have_rgb) COLORING_MODE = 0;
}

static void vizEnd()
{
  REPLAY_IT = 0; // just making sure
  REPLAY_COUNT = p_count;
  DIRTY_POINTS = 0;

  fprintf(stderr,"number of points sampled %d\n",p_count);

  lasreader->close();
  fclose(file);
  delete lasreader;
  lasreader = 0;
}

static int vizContinue()
{
  int more;
  REPLAY_IT = 0; // just making sure

  double xyz[3];
  while (more = lasreader->read_point(xyz))
  {
    if (lasreader->p_count > NEXT_POINT)
    {
      point_buffer[p_count*3+0] = (float)(xyz[0] - lasreader->header.min_x);
      point_buffer[p_count*3+1] = (float)(xyz[1] - lasreader->header.min_y);
      point_buffer[p_count*3+2] = (float)(xyz[2] - lasreader->header.min_z);
      unsigned char is_last = (lasreader->point.return_number == lasreader->point.number_of_returns_of_given_pulse ? 128 : 0);
      unsigned char is_first = (lasreader->point.return_number == 1 ? 64 : 0);
      point_properties[p_count] = is_last | is_first | (lasreader->point.classification & 63);
      if (lasreader->points_have_rgb)
      {
        if (scale_rgb_down || lasreader->rgb[0] > 255 || lasreader->rgb[1] > 255 || lasreader->rgb[2] > 255)
        {
          point_rgb[p_count*3+0] = (unsigned char)(lasreader->rgb[0]/256);
          point_rgb[p_count*3+1] = (unsigned char)(lasreader->rgb[1]/256);
          point_rgb[p_count*3+2] = (unsigned char)(lasreader->rgb[2]/256);
          scale_rgb_down = true;
        }
        else
        {
          point_rgb[p_count*3+0] = (unsigned char)lasreader->rgb[0];
          point_rgb[p_count*3+1] = (unsigned char)lasreader->rgb[1];
          point_rgb[p_count*3+2] = (unsigned char)lasreader->rgb[2];
        }
      }
      p_count++;
      NEXT_POINT += ((EVERY_NTH_POINT/2) + (rand()%EVERY_NTH_POINT) + 1);
    }

    if (lasreader->p_count > NEXT_STEP)
    {
      NEXT_STEP += EVERY_NTH_STEP;
      break;
    }
  }

  return more;
}

static double systime()
{
#ifdef _WIN32
  struct _timeb timebuffer;
  _ftime( &timebuffer );
  return 1.0e-3 * timebuffer.millitm + timebuffer.time;
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return 1e-6 * tv.tv_usec + tv.tv_sec;
#endif
}

static void TINtriangulate()
{
  int i;
  float* p;
  int count = 0;

  clock_t clock_start, clock_finish;
  double systime_start, systime_finish;

  fprintf(stderr,"computing TIN ... ");

  // start clock and timer
  clock_start = clock();
  systime_start = systime();

  TINclean(point_buffer_alloc);

  for (i = 0; i < p_count; i++)
  {
    p = 0;
    switch (render_only)
    {
    case ONLY_ALL:
      p = &(point_buffer[i*3]);
      break;
    case ONLY_LAST:
      if (point_properties[i] & 128) p = &(point_buffer[i*3]);
      break;
    case ONLY_FIRST:
      if (point_properties[i] & 64) p = &(point_buffer[i*3]);
      break;
    case ONLY_GROUND:
      if ((point_properties[i] & 63) == 2) p = &(point_buffer[i*3]);
      break;
    case ONLY_VEGETATION:
      if ((point_properties[i] & 63) > 2 && (point_properties[i] & 63) < 6) p = &(point_buffer[i*3]);
      break;
    case ONLY_BUILDING:
      if ((point_properties[i] & 63) == 6) p = &(point_buffer[i*3]);
      break;
    case ONLY_OBJECT:
      if ((point_properties[i] & 63) > 2 && (point_properties[i] & 63) < 7) p = &(point_buffer[i*3]);
      break;
    case ONLY_MASS_POINTS:
      if ((point_properties[i] & 63) == 8) p = &(point_buffer[i*3]);
      break;
    case ONLY_WATER:
      if ((point_properties[i] & 63) == 9) p = &(point_buffer[i*3]);
      break;
    case ONLY_UNCLASSIFIED:
      if ((point_properties[i] & 63) == 1) p = &(point_buffer[i*3]);
      break;
    case ONLY_OVERLAP:
      if ((point_properties[i] & 63) == 12) p = &(point_buffer[i*3]);
      break;
    }
    if (p)
    {
      TINadd(p);
      count++;
    }
  }

  TINfinish();

  // stop clock and timer
  systime_finish = systime();
  clock_finish = clock();

  fprintf(stderr,"triangulating %d points took %.3g (%.3g) seconds\n", count, systime_finish-systime_start, (double)(clock_finish-clock_start)/CLOCKS_PER_SEC);
}

static void myReshape(int w, int h)
{
  glutReshapeWindow(WindowW,WindowH);
}

static void myIdle()
{
  if (AnimationOn)
  {
    AnimationOn = vizContinue();
    if (!AnimationOn)
    {
      WorkingOn = 0;
      vizEnd();
    }
    glutPostRedisplay();
  }
  else if (REPLAY_IT)
  {
    REPLAY_COUNT += NEXT_STEP;
    glutPostRedisplay();
  }
  else
  {
#ifdef _WIN32
    Sleep(100);
#endif
  }
}

static void full_resolution_rendering()
{
  if (file_name == 0)
  {
    fprintf(stderr,"ERROR: no input file\n");
  }

  int p_count;

  if (lasreader)
  {
    p_count = lasreader->p_count;
    fprintf(stderr,"out-of-core rendering of %d points ... \n",p_count);
    lasreader->close();
    fclose(file);
    delete lasreader;
    lasreader = 0;
  }
  else
  {
    p_count = 2000000000;
    fprintf(stderr,"out-of-core rendering of all points ... \n");
  }

  if (strstr(file_name, ".gz"))
  {
#ifdef _WIN32
    file = fopenGzipped(file_name, "rb");
#else
    fprintf(stderr,"ERROR: cannot open gzipped file %s\n",file_name);
    byebye(byebye_wait);
#endif
  }
  else
  {
    file = fopen(file_name, "rb");
  }
  if (file == 0)
  {
    fprintf(stderr,"ERROR: cannot open %s\n",file_name);
    byebye(byebye_wait);
  }
  lasreader = new LASreader();
  if (lasreader->open(file, skip_all_headers) == false)
  {
    fprintf(stderr, "ERROR: could not open lasreader\n");
    byebye(byebye_wait);
  }

  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glViewport(0,0,WindowW,WindowH);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0f,(float)WindowW/WindowH,0.0625f,5.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(DistX,DistY,DistZ, DistX,DistY,0, 0,1,0);

  glRotatef(Elevation,1,0,0);
  glRotatef(Azimuth,0,1,0);

  glTranslatef(boundingBoxTranslateX*EXTRA_XY_SCALE,boundingBoxTranslateY*EXTRA_XY_SCALE,boundingBoxTranslateZ*EXTRA_Z_SCALE);
  glScalef(boundingBoxScale*EXTRA_XY_SCALE,boundingBoxScale*EXTRA_XY_SCALE,boundingBoxScale*EXTRA_Z_SCALE);

  glEnable(GL_DEPTH_TEST);

  if (p_count == 0)
  {
    p_count = lasreader->npoints;
  }

  double point[3];
  glBegin(GL_POINTS);
  glColor3f(0,0,0);
  while (lasreader->p_count < p_count)
  {
    if (lasreader->read_point(point))
    {
      glVertex3dv(point);
    }
    else
    {
      glEnd();
      lasreader->close();
      fclose(file);
      delete lasreader;
      lasreader = 0;
      glutSwapBuffers();
      return;
    }
  }
  glEnd();

  glDisable(GL_DEPTH_TEST);

  glutSwapBuffers();
}

static void myMouseFunc(int button, int state, int x, int y)
{
  OldX=x;
  OldY=y;        
  if (button == GLUT_LEFT_BUTTON)
  {
    LeftButtonDown = !state;
    MiddleButtonDown = 0;
    RightButtonDown = 0;
  }
  else if (button == GLUT_RIGHT_BUTTON)
  {
    LeftButtonDown = 0;
    MiddleButtonDown = 0;
    RightButtonDown = !state;
  }
  else if (button == GLUT_MIDDLE_BUTTON)
  {
    LeftButtonDown = 0;
    MiddleButtonDown = !state;
    RightButtonDown = 0;
  }
}

static void myMotionFunc(int x, int y)
{
  float RelX = (x-OldX) / (float)glutGet((GLenum)GLUT_WINDOW_WIDTH);
  float RelY = (y-OldY) / (float)glutGet((GLenum)GLUT_WINDOW_HEIGHT);
  OldX=x;
  OldY=y;
  if (LeftButtonDown) 
  { 
    if (InteractionMode == 0)
    {
      Azimuth += (RelX*180);
      Elevation += (RelY*180);
    }
    else if (InteractionMode == 1)
    {
      DistX-=RelX;
      DistY+=RelY;
    }
    else if (InteractionMode == 2)
    {
      DistZ-=RelY*DistZ;
    }
  }
  else if (MiddleButtonDown)
  {
    DistX-=RelX*1.0f;
    DistY+=RelY*1.0f;
  }
  glutPostRedisplay();
}

static void myKeyboard(unsigned char Key, int x, int y)
{
  switch(Key)
  {
  case 'Q':
  case 'q':
  case 27:
    byebye(byebye_wait);
    break;
  case ' ': // rotate, translate, or zoom
    if (InteractionMode == 2)
    {
      InteractionMode = 0;
    }
    else
    {
      InteractionMode += 1;
    }
    glutPostRedisplay();
    break;
  case '>': //zoom in
    DistZ-=0.1f;
    break;
  case '<': //zoom out
    DistZ+=0.1f;
    break;
  case '_':
    break;
  case '+':
    break;
  case '-':
    POINT_SIZE -= 1;
    if (POINT_SIZE < 0) POINT_SIZE = 0;
    fprintf(stderr,"POINT_SIZE %d\n",POINT_SIZE);
    glutPostRedisplay();
    break;
  case '=':
    POINT_SIZE += 1;
    fprintf(stderr,"POINT_SIZE %d\n",POINT_SIZE);
    glutPostRedisplay();
    break;
  case '[':
    if (EXTRA_Z_SCALE > 1)
    {
      EXTRA_Z_SCALE = EXTRA_Z_SCALE >> 1;
      glutPostRedisplay();
    }
    fprintf(stderr,"EXTRA_Z_SCALE %d\n",EXTRA_Z_SCALE);
    break;
  case ']':
    EXTRA_Z_SCALE = EXTRA_Z_SCALE << 1;
    fprintf(stderr,"EXTRA_Z_SCALE %d\n",EXTRA_Z_SCALE);
    glutPostRedisplay();
    break;
  case '{':
    if (EXTRA_XY_SCALE > 1)
    {
      EXTRA_XY_SCALE = EXTRA_XY_SCALE >> 1;
      glutPostRedisplay();
    }
    fprintf(stderr,"EXTRA_XY_SCALE %d\n",EXTRA_XY_SCALE);
    break;
  case '}':
    EXTRA_XY_SCALE = EXTRA_XY_SCALE << 1;
    fprintf(stderr,"EXTRA_XY_SCALE %d\n",EXTRA_XY_SCALE);
    glutPostRedisplay();
    break;
  case 'B':
    RENDER_BOUNDINGBOX = !RENDER_BOUNDINGBOX;
    fprintf(stderr,"RENDER_BOUNDINGBOX %d\n",RENDER_BOUNDINGBOX);
    break;
  case 'R':
  case 'r':
    if (file_name)
    {
      full_resolution_rendering();
    }
    else
    {
      fprintf(stderr,"WARNING: out-of-core rendering from file not possible in pipe mode.\n");
    }
    break;
  case 'a':
    render_only = ONLY_ALL;
    fprintf(stderr,"all returns\n");
    glutPostRedisplay();
    break;
  case 'l':
    render_only = ONLY_LAST;
    fprintf(stderr,"only last returns\n");
    glutPostRedisplay();
    break;
  case 'f':
    render_only = ONLY_FIRST;
    fprintf(stderr,"only first returns\n");
    glutPostRedisplay();
    break;
  case 'g':
    render_only = ONLY_GROUND;
    fprintf(stderr,"only returns classified as ground (2)\n");
    glutPostRedisplay();
    break;
  case 'm':
    render_only = ONLY_MASS_POINTS;
    fprintf(stderr,"only returns classified as mass points (8)\n");
    glutPostRedisplay();
    break;
  case 'w':
    render_only = ONLY_WATER;
    fprintf(stderr,"only returns classified as water (9)\n");
    glutPostRedisplay();
    break;
  case 'u':
    render_only = ONLY_UNCLASSIFIED;
    fprintf(stderr,"only unclassified returns (1)\n");
    glutPostRedisplay();
    break;
  case 'v':
    render_only = ONLY_VEGETATION;
    fprintf(stderr,"only returns classified as vegetation (3-5)\n");
    glutPostRedisplay();
    break;
  case 'b':
    render_only = ONLY_BUILDING;
    fprintf(stderr,"only returns classified as buildings (6)\n");
    glutPostRedisplay();
    break;
  case 'o':
    render_only = ONLY_OVERLAP;
    fprintf(stderr,"only returns classified as overlap (12)\n");
    glutPostRedisplay();
    break;
  case 'C':
  case 'c':
    COLORING_MODE += 1;
    if (COLORING_MODE > 4) COLORING_MODE = 0;
    if (COLORING_MODE == 0 && point_rgb == 0) COLORING_MODE += 1;
    fprintf(stderr,"COLORING_MODE %d\n",COLORING_MODE);
    glutPostRedisplay();
    break;
  case 'H':
  case 'h':
    SHADING_MODE += 1;
    if (SHADING_MODE > 3) SHADING_MODE = 0;
    fprintf(stderr,"SHADING_MODE %d\n",SHADING_MODE);
    glutPostRedisplay();
    break;
  case 'T':
  case 't':
    TINtriangulate();
    glutPostRedisplay();
    break;
  case 'Z':
    if (DIRTY_POINTS)
    {
      // works only in replay mode
      fprintf(stderr,"tiny steps only work during second play (replay)\n");
    }
    else
    {
      REPLAY_COUNT -= 1;
      if (REPLAY_COUNT < 0)
      {
        REPLAY_COUNT = 0;
      }
    }
    glutPostRedisplay();
    break;
  case 'z':
    if (DIRTY_POINTS)
    {
      NEXT_STEP = lasreader->p_count;
      WorkingOn = vizContinue();
    }
    else
    {
      if (REPLAY_COUNT >= p_count)
      {
        REPLAY_COUNT = 0;
      }
      REPLAY_COUNT += 1;
    }
    glutPostRedisplay();
    break;
  case 'S':
    if (DIRTY_POINTS)
    {
      // works only in replay mode
      fprintf(stderr,"back stepping only work during second play (replay)\n");
    }
    else
    {
      NEXT_STEP = p_count / EXACTLY_N_STEPS;
      if (NEXT_STEP == 0) NEXT_STEP = 1;
      REPLAY_COUNT -= NEXT_STEP;
      if (REPLAY_COUNT < 0)
      {
        REPLAY_COUNT = 0;
      }
    }
    glutPostRedisplay();
    break;
  case 'P':
    if (file_name)
    {
      DIRTY_POINTS = 1;
    }
    else
    {
      fprintf(stderr,"WARNING: cannot replay from file when operating in pipe mode.\n");
    }
  case 'p':
    if (DIRTY_POINTS)
    {
      AnimationOn = !AnimationOn;
    }
    else
    {
      if (REPLAY_IT == 0)
      {
        if (REPLAY_COUNT >= p_count)
        {
          REPLAY_COUNT = 0;
        }
        NEXT_STEP = p_count / EXACTLY_N_STEPS;
        if (NEXT_STEP == 0) NEXT_STEP = 1;
        REPLAY_IT = 1;
      }
      else
      {
        REPLAY_IT = 0;
      }
    }
  case 's':
    if (DIRTY_POINTS)
    {
      if (WorkingOn == 0)
      {
        vizBegin();
        WorkingOn = vizContinue();
      }
      else
      {
        WorkingOn = vizContinue();
      }
      if (WorkingOn == 0)
      {
        vizEnd();
        AnimationOn = 0;
      }
    }
    else
    {
      if (REPLAY_COUNT >= p_count)
      {
        REPLAY_COUNT = 0;
      }
      NEXT_STEP = p_count / EXACTLY_N_STEPS;
      if (NEXT_STEP == 0) NEXT_STEP = 1;
      REPLAY_COUNT += NEXT_STEP;
    }
    glutPostRedisplay();
    break;
  case 'K':
  case 'k':
    printf("-kamera %g %g %g %g %g\n", Azimuth, Elevation, DistX,DistY,DistZ);
    break;
  };
}

static void MyMenuFunc(int value)
{
  if (value >= 100)
  {
    if (value <= 102)
    {
      InteractionMode = value - 100;
      glutPostRedisplay();
    }
    else if (value == 103)
    {
      myKeyboard('s',0,0);
    }
    else if (value == 104)
    {
      myKeyboard('p',0,0);
    }
    else if (value == 109)
    {
      myKeyboard('q',0,0);
    }
    else if (value == 150)
    {
      myKeyboard('c',0,0);
    }
    else if (value == 151)
    {
      myKeyboard('=',0,0);
    }
    else if (value == 152)
    {
      myKeyboard('-',0,0);
    }
    else if (value == 153)
    {
      myKeyboard(']',0,0);
    }
    else if (value == 154)
    {
      myKeyboard('[',0,0);
    }
    else if (value == 155)
    {
      myKeyboard('h',0,0);
    }
  }
  else if (value == 99)
  {
    myKeyboard('t',0,0);
  }
  else if (value == 40)
  {
    EXACTLY_N_STEPS = 5;
  }
  else if (value == 41)
  {
    EXACTLY_N_STEPS = 10;
  }
  else if (value == 42)
  {
    EXACTLY_N_STEPS = 25;
  }
  else if (value == 43)
  {
    EXACTLY_N_STEPS = 50;
  }
  else if (value == 44)
  {
    EXACTLY_N_STEPS = 100;
  }
  else if (value == 45)
  {
    EXACTLY_N_STEPS = 250;
  }
  else if (value == 46)
  {
    EXACTLY_N_STEPS = 500;
  }
  else if (value == 47)
  {
    EXACTLY_N_STEPS = 1000;
  }
  else if (value == 48)
  {
    EXACTLY_N_STEPS = 10000;
  }
  else if (value == 50)
  {
    myKeyboard('a',0,0);
  }
  else if (value == 51)
  {
    myKeyboard('l',0,0);
  }
  else if (value == 52)
  {
    myKeyboard('f',0,0);
  }
  else if (value == 53)
  {
    myKeyboard('g',0,0);
  }
  else if (value == 54)
  {
    fprintf(stderr,"only returns classified as objects (3-6)\n");
    glutPostRedisplay();
    render_only = ONLY_OBJECT;
  }
  else if (value == 55)
  {
    myKeyboard('b',0,0);
  }
  else if (value == 56)
  {
    myKeyboard('v',0,0);
  }
  else if (value == 57)
  {
    myKeyboard('m',0,0);
  }
  else if (value == 58)
  {
    myKeyboard('w',0,0);
  }
  else if (value == 59)
  {
    myKeyboard('u',0,0);
  }
  else if (value == 60)
  {
    myKeyboard('o',0,0);
  }
}

static void displayMessage()
{
  glColor3f( 0.7f, 0.7f, 0.7f );  // Set colour to grey
  glMatrixMode( GL_PROJECTION );
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D( 0.0f, 1.0f, 0.0f, 1.0f );
  glMatrixMode( GL_MODELVIEW );
  glPushMatrix();
  glLoadIdentity();
  glRasterPos2f( 0.03f, 0.95f );
  
  if( InteractionMode == 0 )
  {
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'r');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'o');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'a');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'e');
  }
  else if( InteractionMode == 1 )
  {    
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'r');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'a');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'n');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 's');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'l');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'a');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 't');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'e');
  }
  else
  {
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'z');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'o');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'o');
    glutBitmapCharacter(GLUT_BITMAP_8_BY_13, 'm');
  }

  glPopMatrix();
  glMatrixMode( GL_PROJECTION );
  glPopMatrix();
}

static void VecSubtract3fv(float v[3], float a[3], float b[3])
{
  v[0] = a[0] - b[0];
  v[1] = a[1] - b[1];
  v[2] = a[2] - b[2];
}

static void VecSet3fv(float v[3], float x, float y, float z)
{
  v[0] = x;
  v[1] = y;
  v[2] = z;
}

static void VecCrossProd3fv(float v[3], float a[3], float b[3]) // c = a X b
{ 
  VecSet3fv(v, a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]);
}

static float VecLength3fv(float v[3])
{
  return( (float)sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]) );
}

static void VecSelfNormalize3fv(float v[3])
{
  float l = VecLength3fv(v);
  v[0] /= l;
  v[1] /= l;
  v[2] /= l;
}

static void VecCcwNormNormal3fv(float n[3], float a[3], float b[3], float c[3])
{
  float ab[3], ac[3];
  VecSubtract3fv(ab,b,a);
  VecSubtract3fv(ac,c,a);
  VecCrossProd3fv(n,ab,ac);
  VecSelfNormalize3fv(n);
}

static void myDisplay()
{
  int i,j;

  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  
  glViewport(0,0,WindowW,WindowH);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(30.0f,(float)WindowW/WindowH,0.0625f,5.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(DistX,DistY,DistZ, DistX,DistY,0, 0,1,0);

  glRotatef(Elevation,1,0,0);
  glRotatef(Azimuth,0,1,0);

  glTranslatef(boundingBoxTranslateX*EXTRA_XY_SCALE,boundingBoxTranslateY*EXTRA_XY_SCALE,boundingBoxTranslateZ*EXTRA_Z_SCALE);
  glScalef(boundingBoxScale*EXTRA_XY_SCALE,boundingBoxScale*EXTRA_XY_SCALE,boundingBoxScale*EXTRA_Z_SCALE);

  int rendered_points;

  if (DIRTY_POINTS)
  {
    rendered_points = p_count;
  }
  else
  {
    if (REPLAY_COUNT > p_count)
    {
      rendered_points = p_count;
      REPLAY_IT = 0;
    }
    else
    {
      rendered_points = REPLAY_COUNT;
    }
  }

  glEnable(GL_DEPTH_TEST);

  if (TINget_size())
  {
    TINtriangle* t = TINget_triangle(0);
    if (SHADING_MODE == 0)
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glColor3f( 0.7f, 0.7f, 0.7f );  // Set colour to grey
      glBegin(GL_TRIANGLES);
      for (i = 0; i < TINget_size(); i++, t++)
      {
        if (t->next < 0)
        {
          if (t->V[0])
          {
            glVertex3fv(t->V[0]);
            glVertex3fv(t->V[1]);
            glVertex3fv(t->V[2]);
          }
        }
      }
      glEnd();
    }
    else if (SHADING_MODE == 1)
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glMaterialfv(GL_FRONT, GL_DIFFUSE, colours_white);
      glMaterialfv(GL_BACK, GL_DIFFUSE, colours_light_blue);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHTING);
      glEnable(GL_NORMALIZE);
      glBegin(GL_TRIANGLES);
      for (i = 0; i < TINget_size(); i++, t++)
      {
        if (t->next < 0)
        {
          if (t->V[0])
          {
            if (t->next == -1)
            {
              VecCcwNormNormal3fv((float*)(t->N), t->V[0], t->V[1], t->V[2]);
              t->next = -2;
            }
            glNormal3fv((float*)(t->N));
            glVertex3fv(t->V[0]);
            glVertex3fv(t->V[1]);
            glVertex3fv(t->V[2]);
          }
        }
      }
      glEnd();
      glDisable(GL_NORMALIZE);
      glDisable(GL_LIGHTING);
      glDisable(GL_LIGHT0);
    }
    else if (SHADING_MODE == 2)
    {
      glShadeModel(GL_SMOOTH);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHTING);
      glEnable(GL_NORMALIZE);
      glEnable(GL_COLOR_MATERIAL);
      glBegin(GL_TRIANGLES);
      for (i = 0; i < TINget_size(); i++, t++)
      {
        if (t->next < 0)
        {
          if (t->V[0])
          {
            if (t->next == -1)
            {
              VecCcwNormNormal3fv((float*)(t->N), t->V[0], t->V[1], t->V[2]);
              t->next = -2;
            }
            glNormal3fv((float*)(t->N));
            for (j = 0; j < 3; j++)
            {
              float height = t->V[j][2] - boundingBoxMin[2];
              if (height < (boundingBoxHeight/3))
              {
                glColor3f(0.1f+0.7f*height/(boundingBoxHeight/3),0.1f,0.1f);
              }
              else if (height < 2*(boundingBoxHeight/3))
              {
                glColor3f(0.8f,0.1f+0.7f*(height-(boundingBoxHeight/3))/(boundingBoxHeight/3),0.1f);
              }
              else
              {
                glColor3f(0.8f, 0.8f, 0.1f+0.7f*(height-2*(boundingBoxHeight/3))/(boundingBoxHeight/3));
              }
              glVertex3fv(t->V[j]);
            }
          }
        }
      }
      glEnd();
      glDisable(GL_COLOR_MATERIAL);
      glDisable(GL_NORMALIZE);
      glDisable(GL_LIGHTING);
      glDisable(GL_LIGHT0);
      glShadeModel(GL_FLAT);
    }
    else if (SHADING_MODE == 3)
    {
      glShadeModel(GL_SMOOTH);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
      glEnable(GL_LIGHT0);
      glEnable(GL_LIGHTING);
      glEnable(GL_NORMALIZE);
      glEnable(GL_COLOR_MATERIAL);
      glBegin(GL_TRIANGLES);
      for (i = 0; i < TINget_size(); i++, t++)
      {
        if (t->next < 0)
        {
          if (t->V[0])
          {
            if (t->next == -1)
            {
              VecCcwNormNormal3fv((float*)(t->N), t->V[0], t->V[1], t->V[2]);
              t->next = -2;
            }
            glNormal3fv((float*)(t->N));
            for (j = 0; j < 3; j++)
            {
              float height = t->V[j][2] - boundingBoxMin[2];
              if (height < (boundingBoxHeight/2))
              {
                glColor3f(0.1f+0.7f*height/(boundingBoxHeight/2),0.1f,0.1f+0.8f-0.7f*height/(boundingBoxHeight/2));
              }
              else if (height < 2*(boundingBoxHeight/2))
              {
                glColor3f(0.8f, 0.1f + 0.7f*(height-(boundingBoxHeight/2))/(boundingBoxHeight/2), 0.1f + 0.7f*(height-(boundingBoxHeight/2))/(boundingBoxHeight/2));
              }
              glVertex3fv(t->V[j]);
            }
          }
        }
      }
      glEnd();
      glDisable(GL_COLOR_MATERIAL);
      glDisable(GL_NORMALIZE);
      glDisable(GL_LIGHTING);
      glDisable(GL_LIGHT0);
      glShadeModel(GL_FLAT);
    }
  }

  // draw points

  if (p_count && POINT_SIZE)
  {
    glPointSize(POINT_SIZE);
    glBegin(GL_POINTS);
    if (COLORING_MODE == 0)
    {
      for (i = 0; i < rendered_points; i++)
      {
        bool do_render = false;
        switch (render_only)
        {
        case ONLY_ALL:
          do_render = true;
          break;
        case ONLY_LAST:
          if (point_properties[i] & 128) do_render = true;
          break;
        case ONLY_FIRST:
          if (point_properties[i] & 64) do_render = true;
          break;
        case ONLY_GROUND:
          if ((point_properties[i] & 63) == 2) do_render = true;
          break;
        case ONLY_VEGETATION:
          if ((point_properties[i] & 63) > 2 && (point_properties[i] & 63) < 6) do_render = true;
          break;
        case ONLY_BUILDING:
          if ((point_properties[i] & 63) == 6) do_render = true;
          break;
        case ONLY_OBJECT:
          if ((point_properties[i] & 63) > 2 && (point_properties[i] & 63) < 7) do_render = true;
          break;
        case ONLY_MASS_POINTS:
          if ((point_properties[i] & 63) == 8) do_render = true;
          break;
        case ONLY_WATER:
          if ((point_properties[i] & 63) == 9) do_render = true;
          break;
        case ONLY_UNCLASSIFIED:
          if ((point_properties[i] & 63) == 1) do_render = true;
          break;
        case ONLY_OVERLAP:
          if ((point_properties[i] & 63) == 12) do_render = true;
          break;
        }
        if (do_render)
        {
          glColor3ubv(&(point_rgb[i*3]));
          glVertex3fv(&(point_buffer[i*3]));
        }
      }
    }
    else if (COLORING_MODE == 1)
    {
      for (i = 0; i < rendered_points; i++)
      {
        if (i < p_count/3)
        {
          glColor3f(0.1f+0.7f*i/(p_count/3),0.1f,0.1f);
        }
        else if (i < 2*(p_count/3))
        {
          glColor3f(0.8f,0.1f+0.7f*(i-(p_count/3))/(p_count/3),0.1f);
        }
        else
        {
          glColor3f(0.8f, 0.8f, 0.1f+0.7f*(i-2*(p_count/3))/(p_count/3));
        }
        glVertex3fv(&(point_buffer[i*3]));
      }
    }
    else if (COLORING_MODE == 2)
    {
      for (i = 0; i < rendered_points; i++)
      {
        glColor3fv(classification_colors[point_properties[i] & 15]);
        switch (render_only)
        {
        case ONLY_ALL:
          glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_LAST:
          if (point_properties[i] & 128) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_FIRST:
          if (point_properties[i] & 64) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_GROUND:
          if ((point_properties[i] & 63) == 2) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_VEGETATION:
          if ((point_properties[i] & 63) > 2 && (point_properties[i] & 63) < 6) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_BUILDING:
          if ((point_properties[i] & 63) == 6) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_OBJECT:
          if ((point_properties[i] & 63) > 2 && (point_properties[i] & 63) < 7) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_MASS_POINTS:
          if ((point_properties[i] & 63) == 8) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_WATER:
          if ((point_properties[i] & 63) == 9) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_UNCLASSIFIED:
          if ((point_properties[i] & 63) == 1) glVertex3fv(&(point_buffer[i*3]));
          break;
        case ONLY_OVERLAP:
          if ((point_properties[i] & 63) == 12) glVertex3fv(&(point_buffer[i*3]));
          break;
        }
      }
    }
    else if (COLORING_MODE == 3)
    {
      for (i = 0; i < rendered_points; i++)
      {
        float height = point_buffer[i*3+2] - boundingBoxMin[2];
        if (height < (boundingBoxHeight/3))
        {
          glColor3f(0.1f+0.7f*height/(boundingBoxHeight/3),0.1f,0.1f);
        }
        else if (height < 2*(boundingBoxHeight/3))
        {
          glColor3f(0.8f,0.1f+0.7f*(height-(boundingBoxHeight/3))/(boundingBoxHeight/3),0.1f);
        }
        else
        {
          glColor3f(0.8f, 0.8f, 0.1f+0.7f*(height-2*(boundingBoxHeight/3))/(boundingBoxHeight/3));
        }
        glVertex3fv(&(point_buffer[i*3]));
      }
    }
    else
    {
      for (i = 0; i < rendered_points; i++)
      {
        float height = point_buffer[i*3+2] - boundingBoxMin[2];
        if (height < (boundingBoxHeight/2))
        {
          glColor3f(0.1f+0.7f*height/(boundingBoxHeight/2),0.1f,0.1f+0.8f-0.7f*height/(boundingBoxHeight/2));
        }
        else if (height < 2*(boundingBoxHeight/2))
        {
          glColor3f(0.8f, 0.1f + 0.7f*(height-(boundingBoxHeight/2))/(boundingBoxHeight/2), 0.1f + 0.7f*(height-(boundingBoxHeight/2))/(boundingBoxHeight/2));
        }
        glVertex3fv(&(point_buffer[i*3]));
      }
    }
    glEnd();
  }

  if (RENDER_BOUNDINGBOX)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(0,1,0);
    glLineWidth(1.0f);
    glBegin(GL_QUADS);
    if (RENDER_BOUNDINGBOX == 1)
    {
      glVertex3f(boundingBoxMin[0], boundingBoxMin[1], boundingBoxMin[2]);
      glVertex3f(boundingBoxMin[0], boundingBoxMax[1], boundingBoxMin[2]);
      glVertex3f(boundingBoxMax[0], boundingBoxMax[1], boundingBoxMin[2]);
      glVertex3f(boundingBoxMax[0], boundingBoxMin[1], boundingBoxMin[2]);
      glVertex3f(boundingBoxMin[0], boundingBoxMin[1], boundingBoxMax[2]);
      glVertex3f(boundingBoxMin[0], boundingBoxMax[1], boundingBoxMax[2]);
      glVertex3f(boundingBoxMax[0], boundingBoxMax[1], boundingBoxMax[2]);
      glVertex3f(boundingBoxMax[0], boundingBoxMin[1], boundingBoxMax[2]);
    }
    glEnd();
    glLineWidth(1.0f);
  }

  glDisable(GL_DEPTH_TEST);

  displayMessage();
  glutSwapBuffers();
}

int main(int argc, char *argv[])
{
  int i;

  if (argc == 1)
  {
    fprintf(stderr,"lasview.exe is better run in the command line\n");
    file_name = new char[256];
    fprintf(stderr,"enter input file: "); fgets(file_name, 256, stdin);
    file_name[strlen(file_name)-1] = '\0';
    byebye_wait = true;
  }

  glutInit(&argc, argv);

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"-help") == 0)
    {
      usage();    
    }
    else if (strcmp(argv[i],"-win") == 0)
    {
      i++;
      WindowW = atoi(argv[i]);
      i++;
      WindowH = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-steps") == 0)
    {
      i++;
      EXACTLY_N_STEPS = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-every") == 0)
    {
      i++;
      EVERY_NTH_STEP = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-points") == 0)
    {
      i++;
      EXACTLY_N_POINTS = atoi(argv[i]);
    }
    else if (strcmp(argv[i],"-kamera") == 0)
    {
      i++;
      sscanf(argv[i], "%f", &Azimuth);
      i++;
      sscanf(argv[i], "%f", &Elevation);
      i++;
      sscanf(argv[i], "%f", &DistX);
      i++;
      sscanf(argv[i], "%f", &DistY);
      i++;
      sscanf(argv[i], "%f", &DistZ);
    }
    else if (strcmp(argv[i],"-only_first") == 0)
    {
      render_only = ONLY_FIRST;
    }
    else if (strcmp(argv[i],"-only_last") == 0)
    {
      render_only = ONLY_LAST;
    }
    else if (strcmp(argv[i],"-skip") == 0 || strcmp(argv[i],"-skip_headers") == 0)
    {
      skip_all_headers = true;
    }
    else if (strcmp(argv[i],"-scale_rgb") == 0)
    {
      scale_rgb_down = true;
    }
    else if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      file_name = argv[i];
    }
    else if (i == argc-1)
    {
      file_name = argv[argc-1];
    }
    else
    {
      fprintf(stderr,"cannot understand argument '%s'\n", argv[i]);
      usage();
    }
  }

  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(WindowW,WindowH);
  glutInitWindowPosition(180,100);
  glutCreateWindow("just a little LAS viewer");
  
  glShadeModel(GL_FLAT);
  
  InitColors();
  InitLight();
  
  glutDisplayFunc(myDisplay);
  glutReshapeFunc(myReshape);
  glutIdleFunc(myIdle);
  
  glutMouseFunc(myMouseFunc);
  glutMotionFunc(myMotionFunc);
  glutKeyboardFunc(myKeyboard);

  // steps sub menu
  int menuSteps = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("in 5 steps", 40);
  glutAddMenuEntry("in 10 steps", 41);
  glutAddMenuEntry("in 25 steps", 42);
  glutAddMenuEntry("in 50 steps", 43);
  glutAddMenuEntry("in 100 steps", 44);
  glutAddMenuEntry("in 250 steps", 45);
  glutAddMenuEntry("in 500 steps", 46);
  glutAddMenuEntry("in 1000 steps", 47);
  glutAddMenuEntry("in 10000 steps", 48);

  // render sub menu
  int menuRender = glutCreateMenu(MyMenuFunc);
  glutAddMenuEntry("<a>ll returns", 50);
  glutAddMenuEntry("only <l>ast", 51);
  glutAddMenuEntry("only <f>irst", 52);
  glutAddMenuEntry("only <g>round", 53);
  glutAddMenuEntry("only <b>uilding", 55);
  glutAddMenuEntry("only <v>egetation", 56);
  glutAddMenuEntry("only <m>ass points", 57);
  glutAddMenuEntry("only <w>ater", 58);
  glutAddMenuEntry("only <u>nclassified", 59);
  glutAddMenuEntry("only <o>verlap", 60);

  // main menu
  glutCreateMenu(MyMenuFunc);
  glutAddSubMenu("steps ...", menuSteps);
  glutAddSubMenu("render ...", menuRender);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<t>riangulate", 99);
  glutAddMenuEntry("s<h>ading mode", 155);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("rotate <SPACE>", 100);
  glutAddMenuEntry("translate <SPACE>", 101);
  glutAddMenuEntry("zoom <SPACE>", 102);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<s>tep", 103);
  glutAddMenuEntry("<p>lay/stop", 104);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<c>oloring mode", 150);
  glutAddMenuEntry("points large <=>", 151);
  glutAddMenuEntry("points small <->", 152);
  glutAddMenuEntry("height more <]>", 153);
  glutAddMenuEntry("height less <[>", 154);
  glutAddMenuEntry("", 0);
  glutAddMenuEntry("<Q>UIT", 109);
  glutAttachMenu(GLUT_RIGHT_BUTTON);

  myKeyboard('p',0,0);

  glutMainLoop();

  byebye(byebye_wait);

  return 0;
}

