/*
===============================================================================

  FILE:  triangulate.h
  
  CONTENTS:
  
    A simple point-insertion Delaunay triangulator a la Bowyer-Watson.
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
  
  COPYRIGHT:
  
    copyright (C) 2009 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
   31 March 2009 -- separated from lasview on flight UA 927 from FRA to SFO
  
===============================================================================
*/
#ifndef TRIANGULATE_H
#define TRIANGULATE_H

typedef struct TINtriangle {
  float* V[3];
  int N[3];
  int next;
} TINtriangle;

void TINclean(int num);
void TINadd(float* p);
void TINfinish();

int TINget_size();
TINtriangle* TINget_triangle(int t);

// macros for accessing triangle corners

#define TIN_TRIANGLE(i) (i/3)
#define TIN_CORNER(i) (i%3)
#define TIN_INDEX(t,c) (t*3+c)
#define TIN_NEXT(c) ((c+1)%3)
#define TIN_PREV(c) ((c+2)%3)

#endif
