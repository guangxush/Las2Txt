/*
===============================================================================

  FILE:  geoprojectionconverter.cpp
  
  CONTENTS:
  
    see corresponding header file
  
  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
    chuck.gantz@globalstar.com
    gpotts@imagelinks.com
  
  COPYRIGHT:
  
    copyright (C) 2007 martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
    This is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation.

  CHANGE HISTORY:
  
    see corresponding header file
  
===============================================================================
*/
#include "geoprojectionconverter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double PI = 3.141592653589793238462643383279502884197169399375105820974944592;
static const double TWO_PI = PI * 2;
static const double PI_OVER_2 = PI / 2;
static const double PI_OVER_4 = PI / 4;
static const double deg2rad = PI / 180.0;
static const double rad2deg = 180.0 / PI;

static const double feet2meter = 0.3048;
static const double surveyfeet2meter = 0.3048006096012;

class ReferenceEllipsoid
{
public:
  ReferenceEllipsoid(int id, char* name, double equatorialRadius, double eccentricitySquared, double inverseFlattening)
  {
    this->id = id;
    this->name = name; 
    this->equatorialRadius = equatorialRadius;
    this->eccentricitySquared = eccentricitySquared;
    this->inverseFlattening = inverseFlattening;
  }
  int id;
  char* name;
  double equatorialRadius; 
  double eccentricitySquared;  
  double inverseFlattening;  
};

static const ReferenceEllipsoid ellipsoid_list[] = 
{
  //  d, Ellipsoid name, Equatorial Radius, square of eccentricity  
  ReferenceEllipsoid( -1, "Placeholder", 0, 0, 0),  //placeholder to allow array indices to match id numbers
  ReferenceEllipsoid( 1, "Airy", 6377563.396, 0.00667054, 299.3249646),
  ReferenceEllipsoid( 2, "Australian National", 6378160.0, 0.006694542, 298.25),
  ReferenceEllipsoid( 3, "Bessel 1841", 6377397.155, 0.006674372, 299.1528128),
  ReferenceEllipsoid( 4, "Bessel 1841 (Nambia) ", 6377483.865, 0.006674372, 299.1528128),
  ReferenceEllipsoid( 5, "Clarke 1866 (NAD-27)", 6378206.4, 0.006768658, 294.9786982),
  ReferenceEllipsoid( 6, "Clarke 1880", 6378249.145, 0.006803511, 293.465),
  ReferenceEllipsoid( 7, "Everest 1830", 6377276.345, 0.006637847, 300.8017),
  ReferenceEllipsoid( 8, "Fischer 1960 (Mercury) ", 6378166, 0.006693422, 298.3),
  ReferenceEllipsoid( 9, "Fischer 1968", 6378150, 0.006693422, 298.3),
  ReferenceEllipsoid( 10, "GRS 1967", 6378160, 0.006694605, 298.247167427),
  ReferenceEllipsoid( 11, "GRS 1980 (NAD-83)", 6378137, 0.00669438002290, 298.257222101),
  ReferenceEllipsoid( 12, "Helmert 1906", 6378200, 0.006693422, 298.3),
  ReferenceEllipsoid( 13, "Hough", 6378270, 0.00672267, 297.0),
  ReferenceEllipsoid( 14, "International", 6378388, 0.00672267, 297.0),
  ReferenceEllipsoid( 15, "Krassovsky", 6378245, 0.006693422, 298.3),
  ReferenceEllipsoid( 16, "Modified Airy", 6377340.189, 0.00667054, 299.3249646),
  ReferenceEllipsoid( 17, "Modified Everest", 6377304.063, 0.006637847, 300.8017),
  ReferenceEllipsoid( 18, "Modified Fischer 1960", 6378155, 0.006693422, 298.3),
  ReferenceEllipsoid( 19, "South American 1969", 6378160, 0.006694542, 298.25),
  ReferenceEllipsoid( 20, "WGS 60", 6378165, 0.006693422, 298.3),
  ReferenceEllipsoid( 21, "WGS 66", 6378145, 0.006694542, 298.25),
  ReferenceEllipsoid( 22, "WGS-72", 6378135, 0.006694318, 298.26),
  ReferenceEllipsoid( 23, "WGS-84", 6378137, 0.00669437999013, 298.257223563)
};

class StatePlaneLCC
{
public:
  StatePlaneLCC(char* zone, double falseEastingMeter, double falseNorthingMeter, double latOriginDegree, double longMeridianDegree, double firstStdParallelDegree, double secondStdParallelDegree)
  {
    this->zone = zone;
    this->falseEastingMeter = falseEastingMeter;
    this->falseNorthingMeter = falseNorthingMeter;
    this->latOriginDegree = latOriginDegree;
    this->longMeridianDegree = longMeridianDegree;
    this->firstStdParallelDegree = firstStdParallelDegree;
    this->secondStdParallelDegree = secondStdParallelDegree;
  }
  char* zone;
  double falseEastingMeter;
  double falseNorthingMeter;
  double latOriginDegree;
  double longMeridianDegree;
  double firstStdParallelDegree;
  double secondStdParallelDegree;
};

static const StatePlaneLCC state_plane_lcc_nad27_list[] =
{
  // zone, false east [m], false north [m], ProjOrig(Lat), CentMerid(Long), 1st std para, 2nd std para 
  StatePlaneLCC("AK_10",914401.8288,0,51,-176,51.83333333,53.83333333),
  StatePlaneLCC("AR_N",609601.2192,0,34.33333333,-92,34.93333333,36.23333333),
  StatePlaneLCC("AR_S",609601.2192,0,32.66666667,-92,33.3,34.76666667),
  StatePlaneLCC("CA_I",609601.2192,0,39.33333333,-122,40,41.66666667),
  StatePlaneLCC("CA_II",609601.2192,0,37.66666667,-122,38.33333333,39.83333333),
  StatePlaneLCC("CA_III",609601.2192,0,36.5,-120.5,37.06666667,38.43333333),
  StatePlaneLCC("CA_IV",609601.2192,0,35.33333333,-119,36,37.25),
  StatePlaneLCC("CA_V",609601.2192,0,33.5,-118,34.03333333,35.46666667),
  StatePlaneLCC("CA_VI",609601.2192,0,32.16666667,-116.25,32.78333333,33.88333333),
  StatePlaneLCC("CA_VII",1276106.451,1268253.007,34.13333333,-118.3333333,33.86666667,34.41666667),
  StatePlaneLCC("CO_N",609601.2192,0,39.33333333,-105.5,39.71666667,40.78333333),
  StatePlaneLCC("CO_C",609601.2192,0,37.83333333,-105.5,38.45,39.75),
  StatePlaneLCC("CO_S",609601.2192,0,36.66666667,-105.5,37.23333333,38.43333333),
  StatePlaneLCC("CT",182880.3658,0,40.83333333,-72.75,41.2,41.86666667),
  StatePlaneLCC("FL_N",609601.2192,0,29,-84.5,29.58333333,30.75),
  StatePlaneLCC("IA_N",609601.2192,0,41.5,-93.5,42.06666667,43.26666667),
  StatePlaneLCC("IA_S",609601.2192,0,40,-93.5,40.61666667,41.78333333),
  StatePlaneLCC("KS_N",609601.2192,0,38.33333333,-98,38.71666667,39.78333333),
  StatePlaneLCC("KS_S",609601.2192,0,36.66666667,-98.5,37.26666667,38.56666667),
  StatePlaneLCC("KY_N",609601.2192,0,37.5,-84.25,37.96666667,38.96666667),
  StatePlaneLCC("KY_S",609601.2192,0,36.33333333,-85.75,36.73333333,37.93333333),
  StatePlaneLCC("LA_N",609601.2192,0,30.66666667,-92.5,31.16666667,32.66666667),
  StatePlaneLCC("LA_S",609601.2192,0,28.66666667,-91.33333333,29.3,30.7),
  StatePlaneLCC("LA_O",609601.2192,0,25.66666667,-91.33333333,26.16666667,27.83333333),
  StatePlaneLCC("MD",243840.4877,0,37.83333333,-77,38.3,39.45),
  StatePlaneLCC("MA_M",182880.3658,0,41,-71.5,41.71666667,42.68333333),
  StatePlaneLCC("MA_I",60960.12192,0,41,-70.5,41.28333333,41.48333333),
  StatePlaneLCC("MI_N",609601.2192,0,44.78333333,-87,45.48333333,47.08333333),
  StatePlaneLCC("MI_C",609601.2192,0,43.31666667,-84.33333333,44.18333333,45.7),
  StatePlaneLCC("MI_S",609601.2192,0,41.5,-84.33333333,42.1,43.66666667),
  StatePlaneLCC("MN_N",609601.2192,0,46.5,-93.1,47.03333333,48.63333333),
  StatePlaneLCC("MN_C",609601.2192,0,45,-94.25,45.61666667,47.05),
  StatePlaneLCC("MN_S",609601.2192,0,43,-94,43.78333333,45.21666667),
  StatePlaneLCC("MT_N",609601.2192,0,47,-109.5,47.85,48.71666667),
  StatePlaneLCC("MT_C",609601.2192,0,45.83333333,-109.5,46.45,47.88333333),
  StatePlaneLCC("MT_S",609601.2192,0,44,-109.5,44.86666667,46.4),
  StatePlaneLCC("NE_N",609601.2192,0,41.33333333,-100,41.85,42.81666667),
  StatePlaneLCC("NE_S",609601.2192,0,39.66666667,-99.5,40.28333333,41.71666667),
  StatePlaneLCC("NY_LI",609601.2192,30480.06096,40.5,-74,40.66666667,41.03333333),
  StatePlaneLCC("NC",609601.2192,0,33.75,-79,34.33333333,36.16666667),
  StatePlaneLCC("ND_N",609601.2192,0,47,-100.5,47.43333333,48.73333333),
  StatePlaneLCC("ND_S",609601.2192,0,45.66666667,-100.5,46.18333333,47.48333333),
  StatePlaneLCC("OH_N",609601.2192,0,39.66666667,-82.5,40.43333333,41.7),
  StatePlaneLCC("OH_S",609601.2192,0,38,-82.5,38.73333333,40.03333333),
  StatePlaneLCC("OK_N",609601.2192,0,35,-98,35.56666667,36.76666667),
  StatePlaneLCC("OK_S",609601.2192,0,33.33333333,-98,33.93333333,35.23333333),
  StatePlaneLCC("OR_N",609601.2192,0,43.66666667,-120.5,44.33333333,46),
  StatePlaneLCC("OR_S",609601.2192,0,41.66666667,-120.5,42.33333333,44),
  StatePlaneLCC("PA_N",609601.2192,0,40.16666667,-77.75,40.88333333,41.95),
  StatePlaneLCC("PA_S",609601.2192,0,39.33333333,-77.75,39.93333333,40.96666667),
  StatePlaneLCC("PR",152400.3048,0,17.83333333,-66.43333333,18.03333333,18.43333333),
  StatePlaneLCC("St.Croix",152400.3048,30480.06096,17.83333333,-66.43333333,18.03333333,18.43333333),
  StatePlaneLCC("SC_N",609601.2192,0,33,-81,33.76666667,34.96666667),
  StatePlaneLCC("SC_S",609601.2192,0,31.83333333,-81,32.33333333,33.66666667),
  StatePlaneLCC("SD_N",609601.2192,0,43.83333333,-100,44.41666667,45.68333333),
  StatePlaneLCC("SD_S",609601.2192,0,42.33333333,-100.3333333,42.83333333,44.4),
  StatePlaneLCC("TN",609601.2192,30480.06096,34.66666667,-86,35.25,36.41666667),
  StatePlaneLCC("TX_N",609601.2192,0,34,-101.5,34.65,36.18333333),
  StatePlaneLCC("TX_NC",609601.2192,0,31.66666667,-97.5,32.13333333,33.96666667),
  StatePlaneLCC("TX_C",609601.2192,0,29.66666667,-100.3333333,30.11666667,31.88333333),
  StatePlaneLCC("TX_SC",609601.2192,0,27.83333333,-99,28.38333333,30.28333333),
  StatePlaneLCC("TX_S",609601.2192,0,25.66666667,-98.5,26.16666667,27.83333333),
  StatePlaneLCC("UT_N",609601.2192,0,40.33333333,-111.5,40.71666667,41.78333333),
  StatePlaneLCC("UT_C",609601.2192,0,38.33333333,-111.5,39.01666667,40.65),
  StatePlaneLCC("UT_S",609601.2192,0,36.66666667,-111.5,37.21666667,38.35),
  StatePlaneLCC("VA_N",609601.2192,0,37.66666667,-78.5,38.03333333,39.2),
  StatePlaneLCC("VA_S",609601.2192,0,36.33333333,-78.5,36.76666667,37.96666667),
  StatePlaneLCC("WA_N",609601.2192,0,47,-120.8333333,47.5,48.73333333),
  StatePlaneLCC("WA_S",609601.2192,0,45.33333333,-120.5,45.83333333,47.33333333),
  StatePlaneLCC("WV_N",609601.2192,0,38.5,-79.5,39,40.25),
  StatePlaneLCC("WV_S",609601.2192,0,37,-81,37.48333333,38.88333333),
  StatePlaneLCC("WI_N",609601.2192,0,45.16666667,-90,45.56666667,46.76666667),
  StatePlaneLCC("WI_C",609601.2192,0,43.83333333,-90,44.25,45.5),
  StatePlaneLCC("WI_S",609601.2192,0,42,-90,42.73333333,44.06666667),
  StatePlaneLCC(0,-1,-1,-1,-1,-1,-1)
};

static const StatePlaneLCC state_plane_lcc_nad83_list[] =
{
  // zone, false east [m], false north [m], ProjOrig(Lat), CentMerid(Long), 1st std para, 2nd std para 
  StatePlaneLCC("AK_10",1000000,0,51.000000,-176.000000,51.833333,53.833333),
  StatePlaneLCC("AR_N",400000,0,34.333333,-92.000000,34.933333,36.233333),
  StatePlaneLCC("AR_S",400000,400000,32.666667,-92.000000,33.300000,34.766667),
  StatePlaneLCC("CA_I",2000000,500000,39.333333,-122.000000,40.000000,41.666667),
  StatePlaneLCC("CA_II",2000000,500000,37.666667,-122.000000,38.333333,39.833333),
  StatePlaneLCC("CA_III",2000000,500000,36.500000,-120.500000,37.066667,38.433333),
  StatePlaneLCC("CA_IV",2000000,500000,35.333333,-119.000000,36.000000,37.250000),
  StatePlaneLCC("CA_V",2000000,500000,33.500000,-118.000000,34.033333,35.466667),
  StatePlaneLCC("CA_VI",2000000,500000,32.166667,-116.250000,32.783333,33.883333),
  StatePlaneLCC("CO_N",914401.8289,304800.6096,39.333333,-105.500000,39.716667,40.783333),
  StatePlaneLCC("CO_C",914401.8289,304800.6096,37.833333,-105.500000,38.450000,39.750000),
  StatePlaneLCC("CO_S",914401.8289,304800.6096,36.666667,-105.500000,37.233333,38.433333),
  StatePlaneLCC("CT",304800.6096,152400.3048,40.833333,-72.750000,41.200000,41.866667),
  StatePlaneLCC("FL_N",600000,0,29.000000,-84.500000,29.583333,30.750000),
  StatePlaneLCC("IA_N",1500000,1000000,41.500000,-93.500000,42.066667,43.266667),
  StatePlaneLCC("IA_S",500000,0,40.000000,-93.500000,40.616667,41.783333),
  StatePlaneLCC("KS_N",400000,0,38.333333,-98.000000,38.716667,39.783333),
  StatePlaneLCC("KS_S",400000,400000,36.666667,-98.500000,37.266667,38.566667),
  StatePlaneLCC("KY_N",500000,0,37.500000,-84.250000,37.966667,38.966667),
  StatePlaneLCC("KY_S",500000,500000,36.333333,-85.750000,36.733333,37.933333),
  StatePlaneLCC("LA_N",1000000,0,30.500000,-92.500000,31.166667,32.666667),
  StatePlaneLCC("LA_S",1000000,0,28.500000,-91.333333,29.300000,30.700000),
  StatePlaneLCC("LA_O",1000000,0,25.500000,-91.333333,26.166667,27.833333),
  StatePlaneLCC("MD",400000,0,37.666667,-77.000000,38.300000,39.450000),
  StatePlaneLCC("MA_M",200000,750000,41.000000,-71.500000,41.716667,42.683333),
  StatePlaneLCC("MA_I",500000,0,41.000000,-70.500000,41.283333,41.483333),
  StatePlaneLCC("MI_N",8000000,0,44.783333,-87.000000,45.483333,47.083333),
  StatePlaneLCC("MI_C",6000000,0,43.316667,-84.366667,44.183333,45.700000),
  StatePlaneLCC("MI_S",4000000,0,41.500000,-84.366667,42.100000,43.666667),
  StatePlaneLCC("MN_N",800000,100000,46.500000,-93.100000,47.033333,48.633333),
  StatePlaneLCC("MN_C",800000,100000,45.000000,-94.250000,45.616667,47.050000),
  StatePlaneLCC("MN_S",800000,100000,43.000000,-94.000000,43.783333,45.216667),
  StatePlaneLCC("MT",600000,0,44.250000,-109.500000,45.000000,49.000000),
  StatePlaneLCC("NE",500000,0,39.833333,-100.000000,40.000000,43.000000),
  StatePlaneLCC("NY_LI",300000,0,40.166667,-74.000000,40.666667,41.033333),
  StatePlaneLCC("NC",609601.22,0,33.750000,-79.000000,34.333333,36.166667),
  StatePlaneLCC("ND_N",600000,0,47.000000,-100.500000,47.433333,48.733333),
  StatePlaneLCC("ND_S",600000,0,45.666667,-100.500000,46.183333,47.483333),
  StatePlaneLCC("OH_N",600000,0,39.666667,-82.500000,40.433333,41.700000),
  StatePlaneLCC("OH_S",600000,0,38.000000,-82.500000,38.733333,40.033333),
  StatePlaneLCC("OK_N",600000,0,35.000000,-98.000000,35.566667,36.766667),
  StatePlaneLCC("OK_S",600000,0,33.333333,-98.000000,33.933333,35.233333),
  StatePlaneLCC("OR_N",2500000,0,43.666667,-120.500000,44.333333,46.000000),
  StatePlaneLCC("OR_S",1500000,0,41.666667,-120.500000,42.333333,44.000000),
  StatePlaneLCC("PA_N",600000,0,40.166667,-77.750000,40.883333,41.950000),
  StatePlaneLCC("PA_S",600000,0,39.333333,-77.750000,39.933333,40.966667),
  StatePlaneLCC("PR",200000,200000,17.833333,-66.433333,18.033333,18.433333),
  StatePlaneLCC("SC",609600,0,31.833333,-81.000000,32.500000,34.833333),
  StatePlaneLCC("SD_N",600000,0,43.833333,-100.000000,44.416667,45.683333),
  StatePlaneLCC("SD_S",600000,0,42.333333,-100.333333,42.833333,44.400000),
  StatePlaneLCC("TN",600000,0,34.333333,-86.000000,35.250000,36.416667),
  StatePlaneLCC("TX_N",200000,1000000,34.000000,-101.500000,34.650000,36.183333),
  StatePlaneLCC("TX_NC",600000,2000000,31.666667,-98.500000,32.133333,33.966667),
  StatePlaneLCC("TX_C",700000,3000000,29.666667,-100.333333,30.116667,31.883333),
  StatePlaneLCC("TX_SC",600000,4000000,27.833333,-99.000000,28.383333,30.283333),
  StatePlaneLCC("TX_S",300000,5000000,25.666667,-98.500000,26.166667,27.833333),
  StatePlaneLCC("UT_N",500000,1000000,40.333333,-111.500000,40.716667,41.783333),
  StatePlaneLCC("UT_C",500000,2000000,38.333333,-111.500000,39.016667,40.650000),
  StatePlaneLCC("UT_S",500000,3000000,36.666667,-111.500000,37.216667,38.350000),
  StatePlaneLCC("VA_N",3500000,2000000,37.666667,-78.500000,38.033333,39.200000),
  StatePlaneLCC("VA_S",3500000,1000000,36.333333,-78.500000,36.766667,37.966667),
  StatePlaneLCC("WA_N",500000,0,47.000000,-120.833333,47.500000,48.733333),
  StatePlaneLCC("WA_S",500000,0,45.333333,-120.500000,45.833333,47.333333),
  StatePlaneLCC("WV_N",600000,0,38.500000,-79.500000,39.000000,40.250000),
  StatePlaneLCC("WV_S",600000,0,37.000000,-81.000000,37.483333,38.883333),
  StatePlaneLCC("WI_N",600000,0,45.166667,-90.000000,45.566667,46.766667),
  StatePlaneLCC("WI_C",600000,0,43.833333,-90.000000,44.250000,45.500000),
  StatePlaneLCC("WI_S",600000,0,42.000000,-90.000000,42.733333,44.066667),
  StatePlaneLCC(0,-1,-1,-1,-1,-1,-1)
};

class StatePlaneTM
{
public:
  StatePlaneTM(char* zone, double falseEastingMeter, double falseNorthingMeter, double latOriginDegree, double longMeridianDegree, double scaleFactor)
  {
    this->zone = zone;
    this->falseEastingMeter = falseEastingMeter;
    this->falseNorthingMeter = falseNorthingMeter;
    this->latOriginDegree = latOriginDegree;
    this->longMeridianDegree = longMeridianDegree;
    this->scaleFactor = scaleFactor;
  }
  char* zone;
  double falseEastingMeter;
  double falseNorthingMeter;
  double latOriginDegree;
  double longMeridianDegree;
  double scaleFactor;
};

static const StatePlaneTM state_plane_tm_nad27_list[] =
{
  // zone, false east [m], false north [m], ProjOrig(Lat), CentMerid(Long), scale factor
  StatePlaneTM("AL_E",152400.3048,0,30.5,-85.83333333,0.99996),
  StatePlaneTM("AL_W",152400.3048,0,30,-87.5,0.999933333),
  StatePlaneTM("AK_2",152400.3048,0,54,-142,0.9999),
  StatePlaneTM("AK_3",152400.3048,0,54,-146,0.9999),
  StatePlaneTM("AK_4",152400.3048,0,54,-150,0.9999),
  StatePlaneTM("AK_5",152400.3048,0,54,-154,0.9999),
  StatePlaneTM("AK_6",152400.3048,0,54,-158,0.9999),
  StatePlaneTM("AK_7",213360.4267,0,54,-162,0.9999),
  StatePlaneTM("AK_8",152400.3048,0,54,-166,0.9999),
  StatePlaneTM("AK_9",182880.3658,0,54,-170,0.9999),
  StatePlaneTM("AZ_E",152400.3048,0,31,-110.1666667,0.9999),
  StatePlaneTM("AZ_C",152400.3048,0,31,-111.9166667,0.9999),
  StatePlaneTM("AZ_W",152400.3048,0,31,-113.75,0.999933333),
  StatePlaneTM("DE",152400.3048,0,38,-75.41666667,0.999995),
  StatePlaneTM("FL_E",152400.3048,0,24.33333333,-81,0.999941177),
  StatePlaneTM("FL_W",152400.3048,0,24.33333333,-82,0.999941177),
  StatePlaneTM("GA_E",152400.3048,0,30,-82.16666667,0.9999),
  StatePlaneTM("GA_W",152400.3048,0,30,-84.16666667,0.9999),
  StatePlaneTM("HI_1",152400.3048,0,18.83333333,-155.5,0.999966667),
  StatePlaneTM("HI_2",152400.3048,0,20.33333333,-156.6666667,0.999966667),
  StatePlaneTM("HI_3",152400.3048,0,21.16666667,-158,0.99999),
  StatePlaneTM("HI_4",152400.3048,0,21.83333333,-159.5,0.99999),
  StatePlaneTM("HI_5",152400.3048,0,21.66666667,-160.1666667,1),
  StatePlaneTM("ID_E",152400.3048,0,41.66666667,-112.1666667,0.999947368),
  StatePlaneTM("ID_C",152400.3048,0,41.66666667,-114,0.999947368),
  StatePlaneTM("ID_W",152400.3048,0,41.66666667,-115.75,0.999933333),
  StatePlaneTM("IL_E",152400.3048,0,36.66666667,-88.33333333,0.999975),
  StatePlaneTM("IL_W",152400.3048,0,36.66666667,-90.16666667,0.999941177),
  StatePlaneTM("IN_E",152400.3048,0,37.5,-85.66666667,0.999966667),
  StatePlaneTM("IN_W",152400.3048,0,37.5,-87.08333333,0.999966667),
  StatePlaneTM("ME_E",152400.3048,0,43.83333333,-68.5,0.9999),
  StatePlaneTM("ME_W",152400.3048,0,42.83333333,-70.16666667,0.999966667),
  StatePlaneTM("MI_E",152400.3048,0,41.5,-83.66666667,0.999942857),
  StatePlaneTM("MI_C",152400.3048,0,41.5,-85.75,0.999909091),
  StatePlaneTM("MI_W",152400.3048,0,41.5,-88.75,0.999909091),
  StatePlaneTM("MS_E",152400.3048,0,29.66666667,-88.83333333,0.99996),
  StatePlaneTM("MS_W",152400.3048,0,30.5,-90.33333333,0.999941177),
  StatePlaneTM("MO_E",152400.3048,0,35.83333333,-90.5,0.999933333),
  StatePlaneTM("MO_C",152400.3048,0,35.83333333,-92.5,0.999933333),
  StatePlaneTM("MO_W",152400.3048,0,36.16666667,-94.5,0.999941177),
  StatePlaneTM("NV_E",152400.3048,0,34.75,-115.5833333,0.9999),
  StatePlaneTM("NV_C",152400.3048,0,34.75,-116.6666667,0.9999),
  StatePlaneTM("NV_W",152400.3048,0,34.75,-118.5833333,0.9999),
  StatePlaneTM("NH",152400.3048,0,42.5,-71.66666667,0.999966667),
  StatePlaneTM("NJ",609601.2192,0,38.83333333,-74.66666667,0.999975),
  StatePlaneTM("NM_E",152400.3048,0,31,-104.3333333,0.999909091),
  StatePlaneTM("NM_C",152400.3048,0,31,-106.25,0.9999),
  StatePlaneTM("NM_W",152400.3048,0,31,-107.8333333,0.999916667),
  StatePlaneTM("NY_E",152400.3048,0,40,-74.33333333,0.999966667),
  StatePlaneTM("NY_C",152400.3048,0,40,-76.58333333,0.9999375),
  StatePlaneTM("NY_W",152400.3048,0,40,-78.58333333,0.9999375),
  StatePlaneTM("RI",152400.3048,0,41.08333333,-71.5,0.99999375),
  StatePlaneTM("VT",152400.3048,0,42.5,-72.5,0.999964286),
  StatePlaneTM("WY_E",152400.3048,0,40.66666667,-105.1666667,0.999941177),
  StatePlaneTM("WY_EC",152400.3048,0,40.66666667,-107.3333333,0.999941177),
  StatePlaneTM("WY_WC",152400.3048,0,40.66666667,-108.75,0.999941177),
  StatePlaneTM("WY_W",152400.3048,0,40.66666667,-110.0833333,0.999941177),
  StatePlaneTM(0,-1,-1,-1,-1,-1)
};

static const StatePlaneTM state_plane_tm_nad83_list[] =
{
  // zone, false east [m], false north [m], ProjOrig(Lat), CentMerid(Long), scale factor
  StatePlaneTM("AL_E",200000,0,30.5,-85.83333333,0.99996),
  StatePlaneTM("AL_W",600000,0,30,-87.5,0.999933333),
  StatePlaneTM("AK_2",500000,0,54,-142,0.9999),
  StatePlaneTM("AK_3",500000,0,54,-146,0.9999),
  StatePlaneTM("AK_4",500000,0,54,-150,0.9999),
  StatePlaneTM("AK_5",500000,0,54,-154,0.9999),
  StatePlaneTM("AK_6",500000,0,54,-158,0.9999),
  StatePlaneTM("AK_7",500000,0,54,-162,0.9999),
  StatePlaneTM("AK_8",500000,0,54,-166,0.9999),
  StatePlaneTM("AK_9",500000,0,54,-170,0.9999),
  StatePlaneTM("AZ_E",213360,0,31,-110.1666667,0.9999),
  StatePlaneTM("AZ_C",213360,0,31,-111.9166667,0.9999),
  StatePlaneTM("AZ_W",213360,0,31,-113.75,0.999933333),
  StatePlaneTM("DE",200000,0,38,-75.41666667,0.999995),
  StatePlaneTM("FL_E",200000,0,24.33333333,-81,0.999941177),
  StatePlaneTM("FL_W",200000,0,24.33333333,-82,0.999941177),
  StatePlaneTM("GA_E",200000,0,30,-82.16666667,0.9999),
  StatePlaneTM("GA_W",700000,0,30,-84.16666667,0.9999),
  StatePlaneTM("HI_1",500000,0,18.83333333,-155.5,0.999966667),
  StatePlaneTM("HI_2",500000,0,20.33333333,-156.6666667,0.999966667),
  StatePlaneTM("HI_3",500000,0,21.16666667,-158,0.99999),
  StatePlaneTM("HI_4",500000,0,21.83333333,-159.5,0.99999),
  StatePlaneTM("HI_5",500000,0,21.66666667,-160.1666667,1),
  StatePlaneTM("ID_E",200000,0,41.66666667,-112.1666667,0.999947368),
  StatePlaneTM("ID_C",500000,0,41.66666667,-114,0.999947368),
  StatePlaneTM("ID_W",800000,0,41.66666667,-115.75,0.999933333),
  StatePlaneTM("IL_E",300000,0,36.66666667,-88.33333333,0.999975),
  StatePlaneTM("IL_W",700000,0,36.66666667,-90.16666667,0.999941177),
  StatePlaneTM("IN_E",100000,250000,37.5,-85.66666667,0.999966667),
  StatePlaneTM("IN_W",900000,250000,37.5,-87.08333333,0.999966667),
  StatePlaneTM("ME_E",300000,0,43.66666667,-68.5,0.9999),
  StatePlaneTM("ME_W",900000,0,42.83333333,-70.16666667,0.999966667),
  StatePlaneTM("MI_E",500000,0,41.5,-83.66666667,0.999942857),
  StatePlaneTM("MI_C",500000,0,41.5,-85.75,0.999909091),
  StatePlaneTM("MI_W",500000,0,41.5,-88.75,0.999909091),
  StatePlaneTM("MS_E",300000,0,29.5,-88.83333333,0.99995),
  StatePlaneTM("MS_W",700000,0,29.5,-90.33333333,0.99995),
  StatePlaneTM("MO_E",250000,0,35.83333333,-90.5,0.999933333),
  StatePlaneTM("MO_C",500000,0,35.83333333,-92.5,0.999933333),
  StatePlaneTM("MO_W",850000,0,36.16666667,-94.5,0.999941177),
  StatePlaneTM("NV_E",200000,8000000,34.75,-115.5833333,0.9999),
  StatePlaneTM("NV_C",500000,6000000,34.75,-116.6666667,0.9999),
  StatePlaneTM("NV_W",800000,4000000,34.75,-118.5833333,0.9999),
  StatePlaneTM("NH",300000,0,42.5,-71.66666667,0.999966667),
  StatePlaneTM("NJ",150000,0,38.83333333,-74.5,0.9999),
  StatePlaneTM("NM_E",165000,0,31,-104.3333333,0.999909091),
  StatePlaneTM("NM_C",500000,0,31,-106.25,0.9999),
  StatePlaneTM("NM_W",830000,0,31,-107.8333333,0.999916667),
  StatePlaneTM("NY_E",150000,0,38.83333333,-74.5,0.9999),
  StatePlaneTM("NY_C",250000,0,40,-76.58333333,0.9999375),
  StatePlaneTM("NY_W",350000,0,40,-78.58333333,0.9999375),
  StatePlaneTM("RI",100000,0,41.08333333,-71.5,0.99999375),
  StatePlaneTM("VT",500000,0,42.5,-72.5,0.999964286),
  StatePlaneTM("WY_E",200000,0,40.5,-105.1666667,0.9999375),
  StatePlaneTM("WY_EC",400000,100000,40.5,-107.3333333,0.9999375),
  StatePlaneTM("WY_WC",600000,0,40.5,-108.75,0.9999375),
  StatePlaneTM("WY_W",800000,100000,40.5,-110.0833333,0.9999375),
  StatePlaneTM(0,-1,-1,-1,-1,-1)
};

bool GeoProjectionConverter::set_projection_from_geo_keys(int num_geo_keys, GeoProjectionConverterGeoKeys* geo_keys, char* geo_ascii_params, double* geo_double_params, char* description)
{
  int ellipsoid = -1;
  int utm_zone = -1;
  bool utm_northern = false;
  char* sp = 0;
  bool sp_nad27 = false;

  this->num_geo_keys = num_geo_keys;
  this->geo_keys = geo_keys;
  this->geo_ascii_params = geo_ascii_params;
  this->geo_double_params = geo_double_params;

  for (int i = 0; i < num_geo_keys; i++)
  {
    switch (geo_keys[i].key_id)
    {
    case 2048: // GeographicTypeGeoKey
      switch (geo_keys[i].value_offset)
      {
      case 4001: // GCSE_Airy1830
        ellipsoid = 1;
        break;
      case 4002: // GCSE_AiryModified1849 
        ellipsoid = 16;
        break;
      case 4003: // GCSE_AustralianNationalSpheroid
        ellipsoid = 2;
        break;
      case 4004: // GCSE_Bessel1841
      case 4005: // GCSE_Bessel1841Modified
        ellipsoid = 3;
        break;
      case 4006: // GCSE_BesselNamibia
        ellipsoid = 4;
        break;
      case 4008: // GCSE_Clarke1866
      case 4009: // GCSE_Clarke1866Michigan
        ellipsoid = 5;
        break;
      case 4010: // GCSE_Clarke1880_Benoit
      case 4011: // GCSE_Clarke1880_IGN
      case 4012: // GCSE_Clarke1880_RGS
      case 4013: // GCSE_Clarke1880_Arc
      case 4014: // GCSE_Clarke1880_SGA1922
      case 4034: // GCSE_Clarke1880
        ellipsoid = 6;
        break;
      case 4015: // GCSE_Everest1830_1937Adjustment
      case 4016: // GCSE_Everest1830_1967Definition
      case 4017: // GCSE_Everest1830_1975Definition
        ellipsoid = 7;
        break;
      case 4018: // GCSE_Everest1830Modified
        ellipsoid = 17;
        break;
      case 4019: // GCSE_GRS1980
        ellipsoid = 11;
        break;
      case 4020: // GCSE_Helmert1906
        ellipsoid = 12;
        break;
      case 4022: // GCSE_International1924
      case 4023: // GCSE_International1967
        ellipsoid = 14;
        break;
      case 4024: // GCSE_Krassowsky1940
        ellipsoid = 15;
        break;
      case 4030: // GCSE_WGS84
        ellipsoid = 23;
        break;
      case 4267: // GCS_NAD27
        ellipsoid = 5;
        break;
      case 4269: // GCS_NAD83
        ellipsoid = 11;
        break;
      case 4322: // GCS_WGS_72
        ellipsoid = 22;
        break;
      case 4326: // GCS_WGS_84
        ellipsoid = 23;
        break;
      default:
        fprintf(stderr, "GeographicTypeGeoKey: look-up for %d not implemented\n", geo_keys[i].value_offset);
      }
      break;
    case 2050: // GeogGeodeticDatumGeoKey 
      switch (geo_keys[i].value_offset)
      {
      case 6202: // Datum_Australian_Geodetic_Datum_1966
      case 6203: // Datum_Australian_Geodetic_Datum_1984
        ellipsoid = 2;
        break;
      case 6267: // Datum_North_American_Datum_1927
        ellipsoid = 5;
        break;
      case 6269: // Datum_North_American_Datum_1983
        ellipsoid = 11;
        break;
      case 6322: // Datum_WGS72
        ellipsoid = 22;
        break;
      case 6326: // Datum_WGS84
        ellipsoid = 23;
        break;
      case 6001: // DatumE_Airy1830
        ellipsoid = 1;
        break;
      case 6002: // DatumE_AiryModified1849
        ellipsoid = 16;
        break;
      case 6003: // DatumE_AustralianNationalSpheroid
        ellipsoid = 2;
        break;
      case 6004: // DatumE_Bessel1841
      case 6005: // DatumE_BesselModified
        ellipsoid = 3;
        break;
      case 6006: // DatumE_BesselNamibia
        ellipsoid = 4;
        break;
      case 6008: // DatumE_Clarke1866
      case 6009: // DatumE_Clarke1866Michigan
        ellipsoid = 5;
        break;
      case 6010: // DatumE_Clarke1880_Benoit
      case 6011: // DatumE_Clarke1880_IGN
      case 6012: // DatumE_Clarke1880_RGS
      case 6013: // DatumE_Clarke1880_Arc
      case 6014: // DatumE_Clarke1880_SGA1922
      case 6034: // DatumE_Clarke1880
        ellipsoid = 6;
        break;
      case 6015: // DatumE_Everest1830_1937Adjustment
      case 6016: // DatumE_Everest1830_1967Definition
      case 6017: // DatumE_Everest1830_1975Definition
        ellipsoid = 7;
        break;
      case 6018: // DatumE_Everest1830Modified
        ellipsoid = 17;
        break;
      case 6019: // DatumE_GRS1980
        ellipsoid = 11;
        break;
      case 6020: // DatumE_Helmert1906
        ellipsoid = 12;
        break;
      case 6022: // DatumE_International1924
      case 6023: // DatumE_International1967
        ellipsoid = 14;
        break;
      case 6024: // DatumE_Krassowsky1940
        ellipsoid = 15;
        break;
      case 6030: // DatumE_WGS84
        ellipsoid = 23;
        break;
      default:
        fprintf(stderr, "GeogGeodeticDatumGeoKey: look-up for %d not implemented\n", geo_keys[i].value_offset);
      }
      break;
      case 2052: // GeogLinearUnitsGeoKey 
      switch (geo_keys[i].value_offset)
      {
      case 9001: // Linear_Meter
        set_coordinates_in_meter();
        break;
      case 9002: // Linear_Foot
        set_coordinates_in_feet();
        break;
      case 9003: // Linear_Foot_US_Survey
        set_coordinates_in_survey_feet();
        break;
      default:
        fprintf(stderr, "GeogLinearUnitsGeoKey: look-up for %d not implemented\n", geo_keys[i].value_offset);
      }
    case 2056: // GeogEllipsoidGeoKey
      switch (geo_keys[i].value_offset)
      {
      case 7001: // Ellipse_Airy_1830
        ellipsoid = 1;
        break;
      case 7002: // Ellipse_Airy_Modified_1849
        ellipsoid = 16;
        break;
      case 7003: // Ellipse_Australian_National_Spheroid
        ellipsoid = 2;
        break;
      case 7004: // Ellipse_Bessel_1841
      case 7005: // Ellipse_Bessel_Modified
        ellipsoid = 3;
        break;
      case 7006: // Ellipse_Bessel_Namibia
        ellipsoid = 4;
        break;
      case 7008: // Ellipse_Clarke_1866
      case 7009: // Ellipse_Clarke_1866_Michigan
        ellipsoid = 5;
        break;
      case 7010: // Ellipse_Clarke1880_Benoit
      case 7011: // Ellipse_Clarke1880_IGN
      case 7012: // Ellipse_Clarke1880_RGS
      case 7013: // Ellipse_Clarke1880_Arc
      case 7014: // Ellipse_Clarke1880_SGA1922
      case 7034: // Ellipse_Clarke1880
        ellipsoid = 6;
        break;
      case 7015: // Ellipse_Everest1830_1937Adjustment
      case 7016: // Ellipse_Everest1830_1967Definition
      case 7017: // Ellipse_Everest1830_1975Definition
        ellipsoid = 7;
        break;
      case 7018: // Ellipse_Everest1830Modified
        ellipsoid = 17;
        break;
      case 7019: // Ellipse_GRS_1980
        ellipsoid = 11;
        break;
      case 7020: // Ellipse_Helmert1906
        ellipsoid = 12;
        break;
      case 7022: // Ellipse_International1924
      case 7023: // Ellipse_International1967
        ellipsoid = 14;
        break;
      case 7024: // Ellipse_Krassowsky1940
        ellipsoid = 15;
        break;
      case 7030: // Ellipse_WGS_84
        ellipsoid = 23;
        break;
      default:
        fprintf(stderr, "GeogEllipsoidGeoKey: look-up for %d not implemented\n", geo_keys[i].value_offset);
      }
      break;
    case 3072: // ProjectedCSTypeGeoKey 
      switch (geo_keys[i].value_offset)
      {
      case 20137: // PCS_Adindan_UTM_zone_37N
      case 20138: // PCS_Adindan_UTM_zone_38N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-20100;
        break;
      case 20437: // PCS_Ain_el_Abd_UTM_zone_37N
      case 20438: // PCS_Ain_el_Abd_UTM_zone_38N
      case 20439: // PCS_Ain_el_Abd_UTM_zone_39N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-20400;
        break;                
      case 20538: // PCS_Afgooye_UTM_zone_38N
      case 20539: // PCS_Afgooye_UTM_zone_39N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-20500;
        break;
      case 20822: // PCS_Aratu_UTM_zone_22S
      case 20823: // PCS_Aratu_UTM_zone_23S
      case 20824: // PCS_Aratu_UTM_zone_24S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-20800;
        break;
      case 21148: // PCS_Batavia_UTM_zone_48S
      case 21149: // PCS_Batavia_UTM_zone_49S
      case 21150: // PCS_Batavia_UTM_zone_50S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-21100;
        break;
      case 21817: // PCS_Bogota_UTM_zone_17N
      case 21818: // PCS_Bogota_UTM_zone_18N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-21800;
        break;
      case 22032: // PCS_Camacupa_UTM_32S
      case 22033: // PCS_Camacupa_UTM_33S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-22000;
        break; 
      case 22332: // PCS_Carthage_UTM_zone_32N
        utm_northern = true; utm_zone = 32;
        break; 
      case 22523: // PCS_Corrego_Alegre_UTM_23S
      case 22524: // PCS_Corrego_Alegre_UTM_24S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-22500;
        break;
      case 22832: // PCS_Douala_UTM_zone_32N
        utm_northern = true; utm_zone = 32;
        break;
      case 23028: // PCS_ED50_UTM_zone_28N
      case 23029: // PCS_ED50_UTM_zone_29N
      case 23030: 
      case 23031: 
      case 23032: 
      case 23033: 
      case 23034: 
      case 23035: 
      case 23036: 
      case 23037: 
      case 23038: // PCS_ED50_UTM_zone_38N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-23000;
        break;
      case 23239: // PCS_Fahud_UTM_zone_39N
      case 23240: // PCS_Fahud_UTM_zone_40N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-23200;
        break;
      case 23433: // PCS_Garoua_UTM_zone_33N
        utm_northern = true; utm_zone = 33;
        break;
      case 23846: // PCS_ID74_UTM_zone_46N
      case 23847: // PCS_ID74_UTM_zone_47N
      case 23848:
      case 23849:
      case 23850:
      case 23851:
      case 23852:
      case 23853: // PCS_ID74_UTM_zone_53N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-23800;
        break;
      case 23886: // PCS_ID74_UTM_zone_46S
      case 23887: // PCS_ID74_UTM_zone_47S
      case 23888:
      case 23889:
      case 23890:
      case 23891:
      case 23892:
      case 23893:
      case 23894: // PCS_ID74_UTM_zone_54S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-23840;
        break;
      case 23947: // PCS_Indian_1954_UTM_47N
      case 23948: // PCS_Indian_1954_UTM_48N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-23900;
        break;
      case 24047: // PCS_Indian_1975_UTM_47N
      case 24048: // PCS_Indian_1975_UTM_48N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-24000;
        break;
      case 24547: // PCS_Kertau_UTM_zone_47N
      case 24548: // PCS_Kertau_UTM_zone_48N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-24500;
        break;
      case 24720: // PCS_La_Canoa_UTM_zone_20N
      case 24721: // PCS_La_Canoa_UTM_zone_21N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-24700;
        break;
      case 24818: // PCS_PSAD56_UTM_zone_18N
      case 24819: // PCS_PSAD56_UTM_zone_19N
      case 24820: // PCS_PSAD56_UTM_zone_20N
      case 24821: // PCS_PSAD56_UTM_zone_21N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-24800;
        break;
      case 24877: // PCS_PSAD56_UTM_zone_17S
      case 24878: // PCS_PSAD56_UTM_zone_18S
      case 24879: // PCS_PSAD56_UTM_zone_19S
      case 24880: // PCS_PSAD56_UTM_zone_20S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-24860;
        break;
      case 25231: // PCS_Lome_UTM_zone_31N
        utm_northern = true; utm_zone = 31;
        break;
      case 25932: // PCS_Malongo_1987_UTM_32S
        utm_northern = false; utm_zone = 32;
        break;
      case 26237: // PCS_Massawa_UTM_zone_37N
        utm_northern = true; utm_zone = 37;
        break;
      case 26331: // PCS_Minna_UTM_zone_31N
      case 26332: // PCS_Minna_UTM_zone_32N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-26300;
        break;
      case 26432: // PCS_Mhast_UTM_zone_32S
        utm_northern = false; utm_zone = 32;
        break;
      case 26632: // PCS_M_poraloko_UTM_32N
        utm_northern = true; utm_zone = 32;
        break;
      case 26692: // PCS_Minna_UTM_zone_32S
        utm_northern = false; utm_zone = 32;
        break;
      case 26703: // PCS_NAD27_UTM_zone_3N
      case 26704: // PCS_NAD27_UTM_zone_4N
      case 26705: // PCS_NAD27_UTM_zone_5N
      case 26706: // PCS_NAD27_UTM_zone_6N
      case 26707: // PCS_NAD27_UTM_zone_7N
      case 26708: // PCS_NAD27_UTM_zone_7N
      case 26709: // PCS_NAD27_UTM_zone_9N
      case 26710: // PCS_NAD27_UTM_zone_10N
      case 26711: // PCS_NAD27_UTM_zone_11N
      case 26712: // PCS_NAD27_UTM_zone_12N
      case 26713: // PCS_NAD27_UTM_zone_13N
      case 26714: // PCS_NAD27_UTM_zone_14N
      case 26715: // PCS_NAD27_UTM_zone_15N
      case 26716: // PCS_NAD27_UTM_zone_16N
      case 26717: // PCS_NAD27_UTM_zone_17N
      case 26718: // PCS_NAD27_UTM_zone_18N
      case 26719: // PCS_NAD27_UTM_zone_19N
      case 26720: // PCS_NAD27_UTM_zone_20N
      case 26721: // PCS_NAD27_UTM_zone_21N
      case 26722: // PCS_NAD27_UTM_zone_22N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-26700;
        break;
      case 26729: // PCS_NAD27_Alabama_East
        sp_nad27 = true; sp = "AL_E";
        break;
      case 26730: // PCS_NAD27_Alabama_West
        sp_nad27 = true; sp = "AL_W";
        break;
      case 26731: // PCS_NAD27_Alaska_zone_1
        sp_nad27 = true; sp = "AK_1";
        break;
      case 26732: // PCS_NAD27_Alaska_zone_2
        sp_nad27 = true; sp = "AK_2";
        break;
      case 26733: // PCS_NAD27_Alaska_zone_3
        sp_nad27 = true; sp = "AK_3";
        break;
      case 26734: // PCS_NAD27_Alaska_zone_4
        sp_nad27 = true; sp = "AK_4";
        break;
      case 26735: // PCS_NAD27_Alaska_zone_5
        sp_nad27 = true; sp = "AK_5";
        break;
      case 26736: // PCS_NAD27_Alaska_zone_6
        sp_nad27 = true; sp = "AK_6";
        break;
      case 26737: // PCS_NAD27_Alaska_zone_7
        sp_nad27 = true; sp = "AK_7";
        break;
      case 26738: // PCS_NAD27_Alaska_zone_8
        sp_nad27 = true; sp = "AK_8";
        break;
      case 26739: // PCS_NAD27_Alaska_zone_9
        sp_nad27 = true; sp = "AK_9";
        break;
      case 26740: // PCS_NAD27_Alaska_zone_10
        sp_nad27 = true; sp = "AK_10";
        break;
      case 26741: // PCS_NAD27_California_I
        sp_nad27 = true; sp = "CA_I";
        break;
      case 26742: // PCS_NAD27_California_II
        sp_nad27 = true; sp = "CA_II";
        break;
      case 26743: // PCS_NAD27_California_III
        sp_nad27 = true; sp = "CA_III";
        break;
      case 26744: // PCS_NAD27_California_IV
        sp_nad27 = true; sp = "CA_IV";
        break;
      case 26745: // PCS_NAD27_California_V
        sp_nad27 = true; sp = "CA_V";
        break;
      case 26746: // PCS_NAD27_California_VI
        sp_nad27 = true; sp = "CA_VI";
        break;
      case 26747: // PCS_NAD27_California_VII
        sp_nad27 = true; sp = "CA_VII";
        break;
      case 26748: // PCS_NAD27_Arizona_East
        sp_nad27 = true; sp = "AZ_E";
        break;
      case 26749: // PCS_NAD27_Arizona_Central
        sp_nad27 = true; sp = "AZ_C";
        break;
      case 26750: // PCS_NAD27_Arizona_West
        sp_nad27 = true; sp = "AZ_W";
        break;
      case 26751: // PCS_NAD27_Arkansas_North
        sp_nad27 = true; sp = "AR_N";
        break;
      case 26752: // PCS_NAD27_Arkansas_South
        sp_nad27 = true; sp = "AR_S";
        break;
      case 26753: // PCS_NAD27_Colorado_North
        sp_nad27 = true; sp = "CO_N";
        break;
      case 26754: // PCS_NAD27_Colorado_Central
        sp_nad27 = true; sp = "CO_C";
        break;
      case 26755: // PCS_NAD27_Colorado_South
        sp_nad27 = true; sp = "CO_S";
        break;
      case 26756: // PCS_NAD27_Connecticut
        sp_nad27 = true; sp = "CT";
        break;
      case 26757: // PCS_NAD27_Delaware
        sp_nad27 = true; sp = "DE";
        break;
      case 26758: // PCS_NAD27_Florida_East
        sp_nad27 = true; sp = "FL_E";
        break;
      case 26759: // PCS_NAD27_Florida_West
        sp_nad27 = true; sp = "FL_W";
        break;
      case 26760: // PCS_NAD27_Florida_North
        sp_nad27 = true; sp = "FL_N";
        break;
      case 26761: // PCS_NAD27_Hawaii_zone_1
        sp_nad27 = true; sp = "HI_1";
        break;
      case 26762: // PCS_NAD27_Hawaii_zone_2
        sp_nad27 = true; sp = "HI_2";
        break;
      case 26763: // PCS_NAD27_Hawaii_zone_3
        sp_nad27 = true; sp = "HI_3";
        break;
      case 26764: // PCS_NAD27_Hawaii_zone_4
        sp_nad27 = true; sp = "HI_4";
        break;
      case 26765: // PCS_NAD27_Hawaii_zone_5
        sp_nad27 = true; sp = "HI_5";
        break;
      case 26766: // PCS_NAD27_Georgia_East
        sp_nad27 = true; sp = "GA_E";
        break;
      case 26767: // PCS_NAD27_Georgia_West
        sp_nad27 = true; sp = "GA_W";
        break;
      case 26768: // PCS_NAD27_Idaho_East
        sp_nad27 = true; sp = "ID_E";
        break;
      case 26769: // PCS_NAD27_Idaho_Central
        sp_nad27 = true; sp = "ID_C";
        break;
      case 26770: // PCS_NAD27_Idaho_West
        sp_nad27 = true; sp = "ID_W";
        break;
      case 26771: // PCS_NAD27_Illinois_East
        sp_nad27 = true; sp = "IL_E";
        break;
      case 26772: // PCS_NAD27_Illinois_West
        sp_nad27 = true; sp = "IL_W";
        break;
      case 26773: // PCS_NAD27_Indiana_East
        sp_nad27 = true; sp = "IN_E";
        break;
      case 26774: // PCS_NAD27_Indiana_West
        sp_nad27 = true; sp = "IN_W";
        break;
      case 26775: // PCS_NAD27_Iowa_North
        sp_nad27 = true; sp = "IA_N";
        break;
      case 26776: // PCS_NAD27_Iowa_South
        sp_nad27 = true; sp = "IA_S";
        break;
      case 26777: // PCS_NAD27_Kansas_North
        sp_nad27 = true; sp = "KS_N";
        break;
      case 26778: // PCS_NAD27_Kansas_South
        sp_nad27 = true; sp = "KS_S";
        break;
      case 26779: // PCS_NAD27_Kentucky_North
        sp_nad27 = true; sp = "KY_N";
        break;
      case 26780: // PCS_NAD27_Kentucky_South
        sp_nad27 = true; sp = "KY_S";
        break;
      case 26781: // PCS_NAD27_Louisiana_North
        sp_nad27 = true; sp = "LA_N";
        break;
      case 26782: // PCS_NAD27_Louisiana_South
        sp_nad27 = true; sp = "LA_S";
        break;
      case 26783: // PCS_NAD27_Maine_East
        sp_nad27 = true; sp = "ME_E";
        break;
      case 26784: // PCS_NAD27_Maine_West
        sp_nad27 = true; sp = "ME_W";
        break;
      case 26785: // PCS_NAD27_Maryland
        sp_nad27 = true; sp = "MD";
        break;
      case 26786: // PCS_NAD27_Massachusetts
        sp_nad27 = true; sp = "M_M";
        break;
      case 26787: // PCS_NAD27_Massachusetts_Is
        sp_nad27 = true; sp = "M_I";
        break;
      case 26788: // PCS_NAD27_Michigan_North
        sp_nad27 = true; sp = "MI_N";
        break;
      case 26789: // PCS_NAD27_Michigan_Central
        sp_nad27 = true; sp = "MI_C";
        break;
      case 26790: // PCS_NAD27_Michigan_South
        sp_nad27 = true; sp = "MI_S";
        break;
      case 26791: // PCS_NAD27_Minnesota_North
        sp_nad27 = true; sp = "MN_N";
        break;
      case 26792: // PCS_NAD27_Minnesota_Cent
        sp_nad27 = true; sp = "MN_C";
        break;
      case 26793: // PCS_NAD27_Minnesota_South
        sp_nad27 = true; sp = "MN_S";
        break;
      case 26794: // PCS_NAD27_Mississippi_East
        sp_nad27 = true; sp = "MS_E";
        break;
      case 26795: // PCS_NAD27_Mississippi_West
        sp_nad27 = true; sp = "MS_W";
        break;
      case 26796: // PCS_NAD27_Missouri_East
        sp_nad27 = true; sp = "MO_E";
        break;
      case 26797: // PCS_NAD27_Missouri_Central
        sp_nad27 = true; sp = "MO_C";
        break;
      case 26798: // PCS_NAD27_Missouri_West
        sp_nad27 = true; sp = "MO_W";
        break;
      case 26903: // PCS_NAD83_UTM_zone_3N
      case 26904: // PCS_NAD83_UTM_zone_4N
      case 26905: // PCS_NAD83_UTM_zone_5N
      case 26906: // PCS_NAD83_UTM_zone_6N
      case 26907: // PCS_NAD83_UTM_zone_7N
      case 26908: // PCS_NAD83_UTM_zone_7N
      case 26909: // PCS_NAD83_UTM_zone_9N
      case 26910: // PCS_NAD83_UTM_zone_10N
      case 26911: // PCS_NAD83_UTM_zone_11N
      case 26912: // PCS_NAD83_UTM_zone_12N
      case 26913: // PCS_NAD83_UTM_zone_13N
      case 26914: // PCS_NAD83_UTM_zone_14N
      case 26915: // PCS_NAD83_UTM_zone_15N
      case 26916: // PCS_NAD83_UTM_zone_16N
      case 26917: // PCS_NAD83_UTM_zone_17N
      case 26918: // PCS_NAD83_UTM_zone_18N
      case 26919: // PCS_NAD83_UTM_zone_19N
      case 26920: // PCS_NAD83_UTM_zone_20N
      case 26921: // PCS_NAD83_UTM_zone_21N
      case 26922: // PCS_NAD83_UTM_zone_22N
      case 26923: // PCS_NAD83_UTM_zone_23N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-26900;
        break;
      case 26929: // PCS_NAD83_Alabama_East
        sp_nad27 = false; sp = "AL_E";
        break;
      case 26930: // PCS_NAD83_Alabama_West
        sp_nad27 = false; sp = "AL_W";
        break;
      case 26931: // PCS_NAD83_Alaska_zone_1
        sp_nad27 = false; sp = "AK_1";
        break;
      case 26932: // PCS_NAD83_Alaska_zone_2
        sp_nad27 = false; sp = "AK_2";
        break;
      case 26933: // PCS_NAD83_Alaska_zone_3
        sp_nad27 = false; sp = "AK_3";
        break;
      case 26934: // PCS_NAD83_Alaska_zone_4
        sp_nad27 = false; sp = "AK_4";
        break;
      case 26935: // PCS_NAD83_Alaska_zone_5
        sp_nad27 = false; sp = "AK_5";
        break;
      case 26936: // PCS_NAD83_Alaska_zone_6
        sp_nad27 = false; sp = "AK_6";
        break;
      case 26937: // PCS_NAD83_Alaska_zone_7
        sp_nad27 = false; sp = "AK_7";
        break;
      case 26938: // PCS_NAD83_Alaska_zone_8
        sp_nad27 = false; sp = "AK_8";
        break;
      case 26939: // PCS_NAD83_Alaska_zone_9
        sp_nad27 = false; sp = "AK_9";
        break;
      case 26940: // PCS_NAD83_Alaska_zone_10
        sp_nad27 = false; sp = "AK_10";
        break;
      case 26941: // PCS_NAD83_California_I
        sp_nad27 = false; sp = "CA_I";
        break;
      case 26942: // PCS_NAD83_California_II
        sp_nad27 = false; sp = "CA_II";
        break;
      case 26943: // PCS_NAD83_California_III
        sp_nad27 = false; sp = "CA_III";
        break;
      case 26944: // PCS_NAD83_California_IV
        sp_nad27 = false; sp = "CA_IV";
        break;
      case 26945: // PCS_NAD83_California_V
        sp_nad27 = false; sp = "CA_V";
        break;
      case 26946: // PCS_NAD83_California_VI
        sp_nad27 = false; sp = "CA_VI";
        break;
      case 26947: // PCS_NAD83_California_VII
        sp_nad27 = false; sp = "CA_VII";
        break;
      case 26948: // PCS_NAD83_Arizona_East
        sp_nad27 = false; sp = "AZ_E";
        break;
      case 26949: // PCS_NAD83_Arizona_Central
        sp_nad27 = false; sp = "AZ_C";
        break;
      case 26950: // PCS_NAD83_Arizona_West
        sp_nad27 = false; sp = "AZ_W";
        break;
      case 26951: // PCS_NAD83_Arkansas_North
        sp_nad27 = false; sp = "AR_N";
        break;
      case 26952: // PCS_NAD83_Arkansas_South
        sp_nad27 = false; sp = "AR_S";
        break;
      case 26953: // PCS_NAD83_Colorado_North
        sp_nad27 = false; sp = "CO_N";
        break;
      case 26954: // PCS_NAD83_Colorado_Central
        sp_nad27 = false; sp = "CO_C";
        break;
      case 26955: // PCS_NAD83_Colorado_South
        sp_nad27 = false; sp = "CO_S";
        break;
      case 26956: // PCS_NAD83_Connecticut
        sp_nad27 = false; sp = "CT";
        break;
      case 26957: // PCS_NAD83_Delaware
        sp_nad27 = false; sp = "DE";
        break;
      case 26958: // PCS_NAD83_Florida_East
        sp_nad27 = false; sp = "FL_E";
        break;
      case 26959: // PCS_NAD83_Florida_West
        sp_nad27 = false; sp = "FL_W";
        break;
      case 26960: // PCS_NAD83_Florida_North
        sp_nad27 = false; sp = "FL_N";
        break;
      case 26961: // PCS_NAD83_Hawaii_zone_1
        sp_nad27 = false; sp = "HI_1";
        break;
      case 26962: // PCS_NAD83_Hawaii_zone_2
        sp_nad27 = false; sp = "HI_2";
        break;
      case 26963: // PCS_NAD83_Hawaii_zone_3
        sp_nad27 = false; sp = "HI_3";
        break;
      case 26964: // PCS_NAD83_Hawaii_zone_4
        sp_nad27 = false; sp = "HI_4";
        break;
      case 26965: // PCS_NAD83_Hawaii_zone_5
        sp_nad27 = false; sp = "HI_5";
        break;
      case 26966: // PCS_NAD83_Georgia_East
        sp_nad27 = false; sp = "GA_E";
        break;
      case 26967: // PCS_NAD83_Georgia_West
        sp_nad27 = false; sp = "GA_W";
        break;
      case 26968: // PCS_NAD83_Idaho_East
        sp_nad27 = false; sp = "ID_E";
        break;
      case 26969: // PCS_NAD83_Idaho_Central
        sp_nad27 = false; sp = "ID_C";
        break;
      case 26970: // PCS_NAD83_Idaho_West
        sp_nad27 = false; sp = "ID_W";
        break;
      case 26971: // PCS_NAD83_Illinois_East
        sp_nad27 = false; sp = "IL_E";
        break;
      case 26972: // PCS_NAD83_Illinois_West
        sp_nad27 = false; sp = "IL_W";
        break;
      case 26973: // PCS_NAD83_Indiana_East
        sp_nad27 = false; sp = "IN_E";
        break;
      case 26974: // PCS_NAD83_Indiana_West
        sp_nad27 = false; sp = "IN_W";
        break;
      case 26975: // PCS_NAD83_Iowa_North
        sp_nad27 = false; sp = "IA_N";
        break;
      case 26976: // PCS_NAD83_Iowa_South
        sp_nad27 = false; sp = "IA_S";
        break;
      case 26977: // PCS_NAD83_Kansas_North
        sp_nad27 = false; sp = "KS_N";
        break;
      case 26978: // PCS_NAD83_Kansas_South
        sp_nad27 = false; sp = "KS_S";
        break;
      case 26979: // PCS_NAD83_Kentucky_North
        sp_nad27 = false; sp = "KY_N";
        break;
      case 26980: // PCS_NAD83_Kentucky_South
        sp_nad27 = false; sp = "KY_S";
        break;
      case 26981: // PCS_NAD83_Louisiana_North
        sp_nad27 = false; sp = "LA_N";
        break;
      case 26982: // PCS_NAD83_Louisiana_South
        sp_nad27 = false; sp = "LA_S";
        break;
      case 26983: // PCS_NAD83_Maine_East
        sp_nad27 = false; sp = "ME_E";
        break;
      case 26984: // PCS_NAD83_Maine_West
        sp_nad27 = false; sp = "ME_W";
        break;
      case 26985: // PCS_NAD83_Maryland
        sp_nad27 = false; sp = "MD";
        break;
      case 26986: // PCS_NAD83_Massachusetts
        sp_nad27 = false; sp = "M_M";
        break;
      case 26987: // PCS_NAD83_Massachusetts_Is
        sp_nad27 = false; sp = "M_I";
        break;
      case 26988: // PCS_NAD83_Michigan_North
        sp_nad27 = false; sp = "MI_N";
        break;
      case 26989: // PCS_NAD83_Michigan_Central
        sp_nad27 = false; sp = "MI_C";
        break;
      case 26990: // PCS_NAD83_Michigan_South
        sp_nad27 = false; sp = "MI_S";
        break;
      case 26991: // PCS_NAD83_Minnesota_North
        sp_nad27 = false; sp = "MN_N";
        break;
      case 26992: // PCS_NAD83_Minnesota_Cent
        sp_nad27 = false; sp = "MN_C";
        break;
      case 26993: // PCS_NAD83_Minnesota_South
        sp_nad27 = false; sp = "MN_S";
        break;
      case 26994: // PCS_NAD83_Mississippi_East
        sp_nad27 = false; sp = "MS_E";
        break;
      case 26995: // PCS_NAD83_Mississippi_West
        sp_nad27 = false; sp = "MS_W";
        break;
      case 26996: // PCS_NAD83_Missouri_East
        sp_nad27 = false; sp = "MO_E";
        break;
      case 26997: // PCS_NAD83_Missouri_Central
        sp_nad27 = false; sp = "MO_C";
        break;
      case 26998: // PCS_NAD83_Missouri_West
        sp_nad27 = false; sp = "MO_W";
        break;
      case 29118: // PCS_SAD69_UTM_zone_18N
      case 29119: // PCS_SAD69_UTM_zone_19N
      case 29120: // PCS_SAD69_UTM_zone_20N
      case 29121: // PCS_SAD69_UTM_zone_21N
      case 29122: // PCS_SAD69_UTM_zone_22N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-29100;
        break;
      case 29177: // PCS_SAD69_UTM_zone_17S
      case 29178: // PCS_SAD69_UTM_zone_18S
      case 29179: // PCS_SAD69_UTM_zone_19S
      case 29180: // PCS_SAD69_UTM_zone_20S
      case 29181: // PCS_SAD69_UTM_zone_21S
      case 29182: // PCS_SAD69_UTM_zone_22S
      case 29183: // PCS_SAD69_UTM_zone_23S
      case 29184: // PCS_SAD69_UTM_zone_24S
      case 29185: // PCS_SAD69_UTM_zone_25S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-29160;
        break;
      case 29220: // PCS_Sapper_Hill_UTM_20S
      case 29221: // PCS_Sapper_Hill_UTM_21S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-29200;
        break;
      case 29333: // PCS_Schwarzeck_UTM_33S
        utm_northern = false; utm_zone = 33;
        break;
      case 29635: // PCS_Sudan_UTM_zone_35N
      case 29636: // PCS_Sudan_UTM_zone_35N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-29600;
        break;
      case 29738: // PCS_Tananarive_UTM_38S
      case 29739: // PCS_Tananarive_UTM_39S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-29700;
        break;
      case 29849: // PCS_Timbalai_1948_UTM_49N
      case 29850: // PCS_Timbalai_1948_UTM_50N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-29800;
        break;
      case 30339: // PCS_TC_1948_UTM_zone_39N
      case 30340: // PCS_TC_1948_UTM_zone_40N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-30300;
        break;
      case 30729: // PCS_Nord_Sahara_UTM_29N
      case 30730: // PCS_Nord_Sahara_UTM_30N
      case 30731: // PCS_Nord_Sahara_UTM_31N
      case 30732: // PCS_Nord_Sahara_UTM_32N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-30700;
        break;
      case 31028: // PCS_Yoff_UTM_zone_28N
        utm_northern = true; utm_zone = 28;
        break;
      case 31121: // PCS_Zanderij_UTM_zone_21N
        utm_northern = true; utm_zone = 21;
        break;
      case 32001: // PCS_NAD27_Montana_North
        sp_nad27 = true; sp = "MT_N";
        break;
      case 32002: // PCS_NAD27_Montana_Central
        sp_nad27 = true; sp = "MT_C";
        break;
      case 32003: // PCS_NAD27_Montana_South
        sp_nad27 = true; sp = "MT_S";
        break;
      case 32005: // PCS_NAD27_Nebraska_North
        sp_nad27 = true; sp = "NE_N";
        break;
      case 32006: // PCS_NAD27_Nebraska_South
        sp_nad27 = true; sp = "NE_S";
        break;
      case 32007: // PCS_NAD27_Nevada_East
        sp_nad27 = true; sp = "NV_E";
        break;
      case 32008: // PCS_NAD27_Nevada_Central
        sp_nad27 = true; sp = "NV_C";
        break;
      case 32009: // PCS_NAD27_Nevada_West
        sp_nad27 = true; sp = "NV_W";
        break;
      case 32010: // PCS_NAD27_New_Hampshire
        sp_nad27 = true; sp = "NH";
        break;
      case 32011: // PCS_NAD27_New_Jersey
        sp_nad27 = true; sp = "NJ";
        break;
      case 32012: // PCS_NAD27_New_Mexico_East
        sp_nad27 = true; sp = "NM_E";
        break;
      case 32013: // PCS_NAD27_New_Mexico_Cent
        sp_nad27 = true; sp = "NM_C";
        break;
      case 32014: // PCS_NAD27_New_Mexico_West
        sp_nad27 = true; sp = "NM_W";
        break;
      case 32015: // PCS_NAD27_New_York_East
        sp_nad27 = true; sp = "NY_E";
        break;
      case 32016: // PCS_NAD27_New_York_Central
        sp_nad27 = true; sp = "NY_C";
        break;
      case 32017: // PCS_NAD27_New_York_West
        sp_nad27 = true; sp = "NY_W";
        break;
      case 32018: // PCS_NAD27_New_York_Long_Is
        sp_nad27 = true; sp = "NT_LI";
        break;
      case 32019: // PCS_NAD27_North_Carolina
        sp_nad27 = true; sp = "NC";
        break;
      case 32020: // PCS_NAD27_North_Dakota_N
        sp_nad27 = true; sp = "ND_N";
        break;
      case 32021: // PCS_NAD27_North_Dakota_S
        sp_nad27 = true; sp = "ND_S";
        break;
      case 32022: // PCS_NAD27_Ohio_North
        sp_nad27 = true; sp = "OH_N";
        break;
      case 32023: // PCS_NAD27_Ohio_South
        sp_nad27 = true; sp = "OH_S";
        break;
      case 32024: // PCS_NAD27_Oklahoma_North
        sp_nad27 = true; sp = "OK_N";
        break;
      case 32025: // PCS_NAD27_Oklahoma_South
        sp_nad27 = true; sp = "OK_S";
        break;
      case 32026: // PCS_NAD27_Oregon_North
        sp_nad27 = true; sp = "OR_N";
        break;
      case 32027: // PCS_NAD27_Oregon_South
        sp_nad27 = true; sp = "OR_S";
        break;
      case 32028: // PCS_NAD27_Pennsylvania_N
        sp_nad27 = true; sp = "PA_N";
        break;
      case 32029: // PCS_NAD27_Pennsylvania_S
        sp_nad27 = true; sp = "PA_S";
        break;
      case 32030: // PCS_NAD27_Rhode_Island
        sp_nad27 = true; sp = "RI";
        break;
      case 32031: // PCS_NAD27_South_Carolina_N
        sp_nad27 = true; sp = "SC_N";
        break;
      case 32033: // PCS_NAD27_South_Carolina_S
        sp_nad27 = true; sp = "SC_S";
        break;
      case 32034: // PCS_NAD27_South_Dakota_N
        sp_nad27 = true; sp = "SD_N";
        break;
      case 32035: // PCS_NAD27_South_Dakota_S
        sp_nad27 = true; sp = "SD_S";
        break;
      case 32036: // PCS_NAD27_Tennessee
        sp_nad27 = true; sp = "TN";
        break;
      case 32037: // PCS_NAD27_Texas_North
        sp_nad27 = true; sp = "TX_N";
        break;
      case 32038: // PCS_NAD27_Texas_North_Cen
        sp_nad27 = true; sp = "TX_NC";
        break;
      case 32039: // PCS_NAD27_Texas_Central
        sp_nad27 = true; sp = "TX_C";
        break;
      case 32040: // PCS_NAD27_Texas_South_Cen
        sp_nad27 = true; sp = "TX_SC";
        break;
      case 32041: // PCS_NAD27_Texas_South
        sp_nad27 = true; sp = "TX_S";
        break;
      case 32042: // PCS_NAD27_Utah_North
        sp_nad27 = true; sp = "UT_N";
        break;
      case 32043: // PCS_NAD27_Utah_Central
        sp_nad27 = true; sp = "UT_C";
        break;
      case 32044: // PCS_NAD27_Utah_South
        sp_nad27 = true; sp = "UT_S";
        break;
      case 32045: // PCS_NAD27_Vermont
        sp_nad27 = true; sp = "VT";
        break;
      case 32046: // PCS_NAD27_Virginia_North
        sp_nad27 = true; sp = "VA_N";
        break;
      case 32047: // PCS_NAD27_Virginia_South
        sp_nad27 = true; sp = "VA_S";
        break;
      case 32048: // PCS_NAD27_Washington_North
        sp_nad27 = true; sp = "WA_N";
        break;
      case 32049: // PCS_NAD27_Washington_South
        sp_nad27 = true; sp = "WA_S";
        break;
      case 32050: // PCS_NAD27_West_Virginia_N
        sp_nad27 = true; sp = "WV_N";
        break;
      case 32051: // PCS_NAD27_West_Virginia_S
        sp_nad27 = true; sp = "WV_S";
        break;
      case 32052: // PCS_NAD27_Wisconsin_North
        sp_nad27 = true; sp = "WI_N";
        break;
      case 32053: // PCS_NAD27_Wisconsin_Cen
        sp_nad27 = true; sp = "WI_C";
        break;
      case 32054: // PCS_NAD27_Wisconsin_South
        sp_nad27 = true; sp = "WI_S";
        break;
      case 32055: // PCS_NAD27_Wyoming_East
        sp_nad27 = true; sp = "WY_E";
        break;
      case 32056: // PCS_NAD27_Wyoming_E_Cen
        sp_nad27 = true; sp = "WY_EC";
        break;
      case 32057: // PCS_NAD27_Wyoming_W_Cen
        sp_nad27 = true; sp = "WY_WC";
        break;
      case 32058: // PCS_NAD27_Wyoming_West
        sp_nad27 = true; sp = "WY_W";
        break;
      case 32059: // PCS_NAD27_Puerto_Rico
        sp_nad27 = true; sp = "PR";
        break;
      case 32060: // PCS_NAD27_St_Croix
        sp_nad27 = true; sp = "St.Croix";
        break;
      case 32100: // PCS_NAD83_Montana
        sp_nad27 = false; sp = "Montana";
        break;
      case 32104: // PCS_NAD83_Nebraska
        sp_nad27 = false; sp = "NE";
        break;
      case 32107: // PCS_NAD83_Nevada_East
        sp_nad27 = false; sp = "NV_E";
        break;
      case 32108: // PCS_NAD83_Nevada_Central
        sp_nad27 = false; sp = "NV_C";
        break;
      case 32109: // PCS_NAD83_Nevada_West
        sp_nad27 = false; sp = "NV_W";
        break;
      case 32110: // PCS_NAD83_New_Hampshire
        sp_nad27 = false; sp = "NH";
        break;
      case 32111: // PCS_NAD83_New_Jersey
        sp_nad27 = false; sp = "NJ";
        break;
      case 32112: // PCS_NAD83_New_Mexico_East
        sp_nad27 = false; sp = "NM_E";
        break;
      case 32113: // PCS_NAD83_New_Mexico_Cent
        sp_nad27 = false; sp = "NM_C";
        break;
      case 32114: // PCS_NAD83_New_Mexico_West
        sp_nad27 = false; sp = "NM_W";
        break;
      case 32115: // PCS_NAD83_New_York_East
        sp_nad27 = false; sp = "NY_E";
        break;
      case 32116: // PCS_NAD83_New_York_Central
        sp_nad27 = false; sp = "NY_C";
        break;
      case 32117: // PCS_NAD83_New_York_West
        sp_nad27 = false; sp = "NY_W";
        break;
      case 32118: // PCS_NAD83_New_York_Long_Is
        sp_nad27 = false; sp = "NT_LI";
        break;
      case 32119: // PCS_NAD83_North_Carolina
        sp_nad27 = false; sp = "NC";
        break;
      case 32120: // PCS_NAD83_North_Dakota_N
        sp_nad27 = false; sp = "ND_N";
        break;
      case 32121: // PCS_NAD83_North_Dakota_S
        sp_nad27 = false; sp = "ND_S";
        break;
      case 32122: // PCS_NAD83_Ohio_North
        sp_nad27 = false; sp = "OH_N";
        break;
      case 32123: // PCS_NAD83_Ohio_South
        sp_nad27 = false; sp = "OH_S";
        break;
      case 32124: // PCS_NAD83_Oklahoma_North
        sp_nad27 = false; sp = "OK_N";
        break;
      case 32125: // PCS_NAD83_Oklahoma_South
        sp_nad27 = false; sp = "OK_S";
        break;
      case 32126: // PCS_NAD83_Oregon_North
        sp_nad27 = false; sp = "OR_N";
        break;
      case 32127: // PCS_NAD83_Oregon_South
        sp_nad27 = false; sp = "OR_S";
        break;
      case 32128: // PCS_NAD83_Pennsylvania_N
        sp_nad27 = false; sp = "PA_N";
        break;
      case 32129: // PCS_NAD83_Pennsylvania_S
        sp_nad27 = false; sp = "PA_S";
        break;
      case 32130: // PCS_NAD83_Rhode_Island
        sp_nad27 = false; sp = "RI";
        break;
      case 32133: // PCS_NAD83_South_Carolina
        sp_nad27 = false; sp = "SC";
        break;
      case 32134: // PCS_NAD83_South_Dakota_N
        sp_nad27 = false; sp = "SD_N";
        break;
      case 32135: // PCS_NAD83_South_Dakota_S
        sp_nad27 = false; sp = "SD_S";
        break;
      case 32136: // PCS_NAD83_Tennessee
        sp_nad27 = false; sp = "TN";
        break;
      case 32137: // PCS_NAD83_Texas_North
        sp_nad27 = false; sp = "TX_N";
        break;
      case 32138: // PCS_NAD83_Texas_North_Cen
        sp_nad27 = false; sp = "TX_NC";
        break;
      case 32139: // PCS_NAD83_Texas_Central
        sp_nad27 = false; sp = "TX_C";
        break;
      case 32140: // PCS_NAD83_Texas_South_Cen
        sp_nad27 = false; sp = "TX_SC";
        break;
      case 32141: // PCS_NAD83_Texas_South
        sp_nad27 = false; sp = "TX_S";
        break;
      case 32142: // PCS_NAD83_Utah_North
        sp_nad27 = false; sp = "UT_N";
        break;
      case 32143: // PCS_NAD83_Utah_Central
        sp_nad27 = false; sp = "UT_C";
        break;
      case 32144: // PCS_NAD83_Utah_South
        sp_nad27 = false; sp = "UT_S";
        break;
      case 32145: // PCS_NAD83_Vermont
        sp_nad27 = false; sp = "VT";
        break;
      case 32146: // PCS_NAD83_Virginia_North
        sp_nad27 = false; sp = "VA_N";
        break;
      case 32147: // PCS_NAD83_Virginia_South
        sp_nad27 = false; sp = "VA_S";
        break;
      case 32148: // PCS_NAD83_Washington_North
        sp_nad27 = false; sp = "WA_N";
        break;
      case 32149: // PCS_NAD83_Washington_South
        sp_nad27 = false; sp = "WA_S";
        break;
      case 32150: // PCS_NAD83_West_Virginia_N
        sp_nad27 = false; sp = "WV_N";
        break;
      case 32151: // PCS_NAD83_West_Virginia_S
        sp_nad27 = false; sp = "WV_S";
        break;
      case 32152: // PCS_NAD83_Wisconsin_North
        sp_nad27 = false; sp = "WI_N";
        break;
      case 32153: // PCS_NAD83_Wisconsin_Cen
        sp_nad27 = false; sp = "WI_C";
        break;
      case 32154: // PCS_NAD83_Wisconsin_South
        sp_nad27 = false; sp = "WI_S";
        break;
      case 32155: // PCS_NAD83_Wyoming_East
        sp_nad27 = false; sp = "WY_E";
        break;
      case 32156: // PCS_NAD83_Wyoming_E_Cen
        sp_nad27 = false; sp = "WY_EC";
        break;
      case 32157: // PCS_NAD83_Wyoming_W_Cen
        sp_nad27 = false; sp = "WY_WC";
        break;
      case 32158: // PCS_NAD83_Wyoming_West
        sp_nad27 = false; sp = "WY_W";
        break;
      case 32161: // PCS_NAD83_Puerto_Rico_Virgin_Is
        sp_nad27 = false; sp = "PR";
        break;
      case 32201: // PCS_WGS72_UTM_zone_1N 
      case 32202: // PCS_WGS72_UTM_zone_2N 
      case 32203: // PCS_WGS72_UTM_zone_3N 
      case 32204: // PCS_WGS72_UTM_zone_4N 
      case 32205: // PCS_WGS72_UTM_zone_5N 
      case 32206: // PCS_WGS72_UTM_zone_6N 
      case 32207: // PCS_WGS72_UTM_zone_7N 
      case 32208:
      case 32209:
      case 32210:
      case 32211:
      case 32212:
      case 32213:
      case 32214:
      case 32215:
      case 32216:
      case 32217:
      case 32218:
      case 32219:
      case 32220:
      case 32221:
      case 32222:
      case 32223:
      case 32224:
      case 32225:
      case 32226:
      case 32227:
      case 32228:
      case 32229:
      case 32230:
      case 32231:
      case 32232:
      case 32233:
      case 32234:
      case 32235:
      case 32236:
      case 32237:
      case 32238:
      case 32239:
      case 32240:
      case 32241:
      case 32242:
      case 32243:
      case 32244:
      case 32245:
      case 32246:
      case 32247:
      case 32248:
      case 32249:
      case 32250:
      case 32251:
      case 32252:
      case 32253:
      case 32254:
      case 32255:
      case 32256:
      case 32257:
      case 32258:
      case 32259: // PCS_WGS72_UTM_zone_59N 
      case 32260: // PCS_WGS72_UTM_zone_60N 
        utm_northern = true; utm_zone = geo_keys[i].value_offset-32200;
        ellipsoid = 23;
        break;
      case 32301: // PCS_WGS72_UTM_zone_1S
      case 32302: // PCS_WGS72_UTM_zone_2S
      case 32303: // PCS_WGS72_UTM_zone_3S
      case 32304: // PCS_WGS72_UTM_zone_4S
      case 32305: // PCS_WGS72_UTM_zone_5S
      case 32306: // PCS_WGS72_UTM_zone_6S
      case 32307: // PCS_WGS72_UTM_zone_7S
      case 32308:
      case 32309:
      case 32310:
      case 32311:
      case 32312:
      case 32313:
      case 32314:
      case 32315:
      case 32316:
      case 32317:
      case 32318:
      case 32319:
      case 32320:
      case 32321:
      case 32322:
      case 32323:
      case 32324:
      case 32325:
      case 32326:
      case 32327:
      case 32328:
      case 32329:
      case 32330:
      case 32331:
      case 32332:
      case 32333:
      case 32334:
      case 32335:
      case 32336:
      case 32337:
      case 32338:
      case 32339:
      case 32340:
      case 32341:
      case 32342:
      case 32343:
      case 32344:
      case 32345:
      case 32346:
      case 32347:
      case 32348:
      case 32349:
      case 32350:
      case 32351:
      case 32352:
      case 32353:
      case 32354:
      case 32355:
      case 32356:
      case 32357:
      case 32358:
      case 32359: // PCS_WGS72_UTM_zone_59S
      case 32360: // PCS_WGS72_UTM_zone_60S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-32300;
        ellipsoid = 23;
        break;
      case 32401: // PCS_WGS72BE_UTM_zone_1N
      case 32402: // PCS_WGS72BE_UTM_zone_2N
      case 32403: // PCS_WGS72BE_UTM_zone_3N
      case 32404: // PCS_WGS72BE_UTM_zone_4N
      case 32405: // PCS_WGS72BE_UTM_zone_5N
      case 32406: // PCS_WGS72BE_UTM_zone_6N
      case 32407: // PCS_WGS72BE_UTM_zone_7N
      case 32408:
      case 32409:
      case 32410:
      case 32411:
      case 32412:
      case 32413:
      case 32414:
      case 32415:
      case 32416:
      case 32417:
      case 32418:
      case 32419:
      case 32420:
      case 32421:
      case 32422:
      case 32423:
      case 32424:
      case 32425:
      case 32426:
      case 32427:
      case 32428:
      case 32429:
      case 32430:
      case 32431:
      case 32432:
      case 32433:
      case 32434:
      case 32435:
      case 32436:
      case 32437:
      case 32438:
      case 32439:
      case 32440:
      case 32441:
      case 32442:
      case 32443:
      case 32444:
      case 32445:
      case 32446:
      case 32447:
      case 32448:
      case 32449:
      case 32450:
      case 32451:
      case 32452:
      case 32453:
      case 32454:
      case 32455:
      case 32456:
      case 32457:
      case 32458:
      case 32459: // PCS_WGS72BE_UTM_zone_59N
      case 32460: // PCS_WGS72BE_UTM_zone_60N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-32400;
        ellipsoid = 23;
        break;
      case 32501: // PCS_WGS72BE_UTM_zone_1S
      case 32502: // PCS_WGS72BE_UTM_zone_2S
      case 32503: // PCS_WGS72BE_UTM_zone_3S
      case 32504: // PCS_WGS72BE_UTM_zone_4S
      case 32505: // PCS_WGS72BE_UTM_zone_5S
      case 32506: // PCS_WGS72BE_UTM_zone_6S
      case 32507: // PCS_WGS72BE_UTM_zone_7S
      case 32508:
      case 32509:
      case 32510:
      case 32511:
      case 32512:
      case 32513:
      case 32514:
      case 32515:
      case 32516:
      case 32517:
      case 32518:
      case 32519:
      case 32520:
      case 32521:
      case 32522:
      case 32523:
      case 32524:
      case 32525:
      case 32526:
      case 32527:
      case 32528:
      case 32529:
      case 32530:
      case 32531:
      case 32532:
      case 32533:
      case 32534:
      case 32535:
      case 32536:
      case 32537:
      case 32538:
      case 32539:
      case 32540:
      case 32541:
      case 32542:
      case 32543:
      case 32544:
      case 32545:
      case 32546:
      case 32547:
      case 32548:
      case 32549:
      case 32550:
      case 32551:
      case 32552:
      case 32553:
      case 32554:
      case 32555:
      case 32556:
      case 32557:
      case 32558:
      case 32559: // PCS_WGS72BE_UTM_zone_59S
      case 32560: // PCS_WGS72BE_UTM_zone_60S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-32500;
        ellipsoid = 23;
        break;
      case 32601: // PCS_WGS84_UTM_zone_1N
      case 32602: // PCS_WGS84_UTM_zone_2N
      case 32603: // PCS_WGS84_UTM_zone_3N
      case 32604: // PCS_WGS84_UTM_zone_4N
      case 32605: // PCS_WGS84_UTM_zone_5N
      case 32606: // PCS_WGS84_UTM_zone_6N
      case 32607: // PCS_WGS84_UTM_zone_7N
      case 32608:
      case 32609:
      case 32610:
      case 32611:
      case 32612:
      case 32613:
      case 32614:
      case 32615:
      case 32616:
      case 32617:
      case 32618:
      case 32619:
      case 32620:
      case 32621:
      case 32622:
      case 32623:
      case 32624:
      case 32625:
      case 32626:
      case 32627:
      case 32628:
      case 32629:
      case 32630:
      case 32631:
      case 32632:
      case 32633:
      case 32634:
      case 32635:
      case 32636:
      case 32637:
      case 32638:
      case 32639:
      case 32640:
      case 32641:
      case 32642:
      case 32643:
      case 32644:
      case 32645:
      case 32646:
      case 32647:
      case 32648:
      case 32649:
      case 32650:
      case 32651:
      case 32652:
      case 32653:
      case 32654:
      case 32655:
      case 32656:
      case 32657:
      case 32658:
      case 32659: // PCS_WGS84_UTM_zone_59N
      case 32660: // PCS_WGS84_UTM_zone_60N
        utm_northern = true; utm_zone = geo_keys[i].value_offset-32600;
        ellipsoid = 23;
        break;
      case 32701: // PCS_WGS84_UTM_zone_1S
      case 32702: // PCS_WGS84_UTM_zone_2S
      case 32703: // PCS_WGS84_UTM_zone_3S
      case 32704: // PCS_WGS84_UTM_zone_4S
      case 32705: // PCS_WGS84_UTM_zone_5S
      case 32706: // PCS_WGS84_UTM_zone_6S
      case 32707: // PCS_WGS84_UTM_zone_7S
      case 32708:
      case 32709:
      case 32710:
      case 32711:
      case 32712:
      case 32713:
      case 32714:
      case 32715:
      case 32716:
      case 32717:
      case 32718:
      case 32719:
      case 32720:
      case 32721:
      case 32722:
      case 32723:
      case 32724:
      case 32725:
      case 32726:
      case 32727:
      case 32728:
      case 32729:
      case 32730:
      case 32731:
      case 32732:
      case 32733:
      case 32734:
      case 32735:
      case 32736:
      case 32737:
      case 32738:
      case 32739:
      case 32740:
      case 32741:
      case 32742:
      case 32743:
      case 32744:
      case 32745:
      case 32746:
      case 32747:
      case 32748:
      case 32749:
      case 32750:
      case 32751:
      case 32752:
      case 32753:
      case 32754:
      case 32755:
      case 32756:
      case 32757:
      case 32758:
      case 32759: // PCS_WGS84_UTM_zone_59S
      case 32760: // PCS_WGS84_UTM_zone_60S
        utm_northern = false; utm_zone = geo_keys[i].value_offset-32700;
        ellipsoid = 23;
        break;
      default:
        fprintf(stderr, "ProjectedCSTypeGeoKey: look-up for %d not implemented\n", geo_keys[i].value_offset);
      }
      break;
    case 3076: // ProjLinearUnitsGeoKey
      switch (geo_keys[i].value_offset)
      {
      case 9001: // Linear_Meter
        set_coordinates_in_meter();
        break;
      case 9002: // Linear_Foot
        set_coordinates_in_feet();
        break;
      case 9003: // Linear_Foot_US_Survey
        set_coordinates_in_survey_feet();
        break;
      default:
        fprintf(stderr, "ProjLinearUnitsGeoKey: look-up for %d not implemented\n", geo_keys[i].value_offset);
      }
      break;
    }
  }

  if (ellipsoid == -1 && utm_zone != -1) ellipsoid = 23;
  if (ellipsoid == -1 && sp != 0) if (sp_nad27) ellipsoid = 5; else ellipsoid = 11;

  if (ellipsoid != -1)
  {
    set_reference_ellipsoid(ellipsoid, description);
    if (utm_zone != -1)
    {
      return set_utm_projection(utm_zone, utm_northern, description);
    }
    if (sp)
    {
      if (sp_nad27)
      {
        if (set_state_plane_nad27_lcc(sp, description))
        {
          return true;
        }
        else if (set_state_plane_nad27_tm(sp, description))
        {
          return true;
        }
      }
      else
      {
        if (set_state_plane_nad83_lcc(sp, description))
        {
          return true;
        }
        else if (set_state_plane_nad83_tm(sp, description))
        {
          return true;
        }
      }
    }
  }
  return false;
}

short GeoProjectionConverter::get_GTModelTypeGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 1024)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  return 2; // assume ModelTypeGeographic
}

short GeoProjectionConverter::get_GTRasterTypeGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 1025)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  return 1; // assume RasterPixelIsArea
}

short GeoProjectionConverter::get_GeographicTypeGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2048)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  switch (ellipsoid_id)
  {
  case 1: // GCSE_Airy1830
    return 4001;
  case 2: // GCSE_AustralianNationalSpheroid
    return 4003;
  case 3: // GCSE_Bessel1841
    return 4004;
  case 4: // GCSE_BesselNamibia
    return 4006;
  case 5: // GCS_NAD27
    return 4267;
  case 6: // GCSE_Clarke1880
    return 4034;
  case 11: // GCS_NAD83
    return 4269;
  case 12: // GCSE_Helmert1906
    return 4020;
  case 15: // GCSE_Krassowsky1940
    return 4024;
  case 16: // GCSE_AiryModified1849 
    return 4002;
  case 17: // GCSE_Everest1830Modified
    return 4018;
  case 22: // GCS_WGS_72
    return 4322;
  case 23: // GCS_WGS_84
    return 4326;
  default:
    fprintf(stderr, "GeographicTypeGeoKey: look-up for ellipsoid_id %d not implemented\n",ellipsoid_id);
  }
  return 0;
}

short GeoProjectionConverter::get_GeogGeodeticDatumGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2050)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  switch (ellipsoid_id)
  {
  case 1: // DatumE_Airy1830
    return 6001;
  case 2: // DatumE_AustralianNationalSpheroid
    return 6003;
  case 3: // DatumE_Bessel1841
    return 6004;
  case 4: // DatumE_BesselNamibia
    return 6006;
  case 5: // Datum_North_American_Datum_1927
    return 6267;
  case 6: // DatumE_Clarke1880
    return 6034;
  case 11: // Datum_North_American_Datum_1983
    return 6269;
  case 12: // DatumE_Helmert1906
    return 6020;
  case 15: // DatumE_Krassowsky1940
    return 6024;
  case 16: // DatumE_AiryModified1849 
    return 6002;
  case 17: // DatumE_Everest1830Modified
    return 6018;
  case 22: // Datum_WGS72
    return 6322;
  case 23: // Datum_WGS84
    return 6326;
  default:
    fprintf(stderr, "GeogGeodeticDatumGeoKey: look-up for ellipsoid_id %d not implemented\n",ellipsoid_id);
  }
  return 0;
}

short GeoProjectionConverter::get_GeogPrimeMeridianGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2051)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  return 0;
}

short GeoProjectionConverter::get_GeogLinearUnitsGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2052)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  if (coordinates2meter == 1.0)
  {
    return 9001; // Linear_Meter
  }
  else if (coordinates2meter == 0.3048)
  {
    return 9002; // Linear_Foot
  }
  else
  {
    return 9003; // assume Linear_Foot_US_Survey
  }
}

double GeoProjectionConverter::get_GeogLinearUnitSizeGeoKey()
{
  if (num_geo_keys && geo_double_params)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2053)
      {
        return geo_double_params[geo_keys[i].value_offset];
      }
    }
  }
  return 0;
}

short GeoProjectionConverter::get_GeogAngularUnitsGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2054)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  return 9102; // assume Angular_Degree
}

double GeoProjectionConverter::get_GeogAngularUnitSizeGeoKey()
{
  if (num_geo_keys && geo_double_params)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2055)
      {
        return geo_double_params[geo_keys[i].value_offset];
      }
    }
  }
  return 0;
}

short GeoProjectionConverter::get_GeogEllipsoidGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2056)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  switch (ellipsoid_id)
  {
  case 1: // Ellipse_Airy_1830
    return 7001;
  case 2: // Ellipse_Australian_National_Spheroid
    return 7003;
  case 3: // Ellipse_Bessel_1841
    return 7004;
  case 4: // Ellipse_Bessel_Namibia
    return 7006;
  case 5: // Ellipse_Clarke_1866
    return 7008;
  case 6: // Ellipse_Clarke1880
    return 7034;
  case 11: // Ellipse_GRS_1980
    return 7019;
  case 12: // Ellipse_Helmert1906
    return 7020;
  case 15: // Ellipse_Krassowsky1940
    return 7024;
  case 23: // Ellipse_WGS_84
    return 7030;
  default:
    fprintf(stderr, "GeogEllipsoidGeoKey: look-up for ellipsoid_id %d not implemented\n",ellipsoid_id);
  }
  return 0;
}

double GeoProjectionConverter::get_GeogSemiMajorAxisGeoKey()
{
  if (num_geo_keys && geo_double_params)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2057)
      {
        return geo_double_params[geo_keys[i].value_offset];
      }
    }
  }
  return 0;
}

double GeoProjectionConverter::get_GeogSemiMinorAxisGeoKey()
{
  if (num_geo_keys && geo_double_params)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2058)
      {
        return geo_double_params[geo_keys[i].value_offset];
      }
    }
  }
  return 0;
}

double GeoProjectionConverter::get_GeogInvFlatteningGeoKey()
{
  if (num_geo_keys && geo_double_params)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2059)
      {
        return geo_double_params[geo_keys[i].value_offset];
      }
    }
  }
  return 0;
}

short GeoProjectionConverter::get_GeogAzimuthUnitsGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2060)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  return 9102; // assume Angular_Degree
}

double GeoProjectionConverter::get_GeogPrimeMeridianLongGeoKey()
{
  if (num_geo_keys && geo_double_params)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 2061)
      {
        return geo_double_params[geo_keys[i].value_offset];
      }
    }
  }
  return 0;
}

short GeoProjectionConverter::get_ProjectedCSTypeGeoKey()
{
  if (num_geo_keys)
  {
    for (int i = 0; i < num_geo_keys; i++)
    {
      if (geo_keys[i].key_id == 3072)
      {
        return geo_keys[i].value_offset;
      }
    }
  }
  if (has_projection())
  {
    if (projection_name[0] == 'U')
    {
      if (utm_northern_hemisphere)
      {
        return 32200 + utm_zone_number;
      }
      else
      {
        return 32300 + utm_zone_number;
      }
    }
    else
    {
      // state planes ...
    }
  }
  return 0;
}

bool GeoProjectionConverter::set_utm_projection(char* zone, char* description)
{
  char* letter;

  utm_zone_number = strtoul(zone, &letter, 10);
  utm_zone_letter = *letter;

  if (utm_zone_letter < 'C' || utm_zone_letter > 'X')
  {
    return false;
  }

  sprintf(projection_name,"UTM zone %d%c",utm_zone_number,utm_zone_letter);

  if((*letter - 'N') >= 0)
  {
    utm_northern_hemisphere = true; // point is in northern hemisphere
  }
  else
  {
    utm_northern_hemisphere = false; //point is in southern hemisphere
  }

  utm_long_origin = (utm_zone_number - 1) * 6 - 180 + 3;  // + 3 puts origin in middle of zone

  if (description)
  {
    sprintf(description, "%d%c - %s", utm_zone_number, utm_zone_letter, (utm_northern_hemisphere ? "northern hemisphere" : "southern hemisphere"));
  }
  return true;
}

bool GeoProjectionConverter::set_utm_projection(int zone, bool northern, char* description)
{
  sprintf(projection_name,"UTM zone %d",zone);
  utm_zone_number = zone;
  utm_northern_hemisphere = northern;
  utm_long_origin = (zone - 1) * 6 - 180 + 3;  // + 3 puts origin in middle of zone
  if (description)
  {
    sprintf(description, "%d - %s", utm_zone_number, utm_zone_letter, (utm_northern_hemisphere ? "northern hemisphere" : "southern hemisphere"));
  }
  return true;
}

bool GeoProjectionConverter::set_reference_ellipsoid(int id, char* description)
{
  if (id <= 0 || id >= 24)
  {
    return false;
  }

  ellipsoid_id = id;
  ellipsoid_name = ellipsoid_list[id].name;
  equatorial_radius = ellipsoid_list[id].equatorialRadius;
  eccentricity_squared = ellipsoid_list[id].eccentricitySquared;
  eccentricity_prime_squared = (eccentricity_squared)/(1-eccentricity_squared);
  polar_radius = equatorial_radius*sqrt(1-eccentricity_squared);    
  eccentricity = sqrt(eccentricity_squared);
  eccentricity_e1 = (1-sqrt(1-eccentricity_squared))/(1+sqrt(1-eccentricity_squared));

  // recompute LCC parameters
  compute_lcc_parameters();
  // recompute TM parameters
  compute_tm_parameters();

  if (description)
  {
    sprintf(description, "%2d - %s (%g %g)", ellipsoid_id, ellipsoid_name, equatorial_radius, eccentricity_squared);
  }

  return true;
}

const char* GeoProjectionConverter::get_ellipsoid_name()
{
  return ellipsoid_name;
}

// Configure a Lambert Conic Conformal Projection
//
// The function Set_Lambert_Parameters receives the ellipsoid parameters and
// Lambert Conformal Conic projection parameters as inputs, and sets the
// corresponding state variables.
//
// falseEastingMeter & falseNorthingMeter are just an offset in meters added 
// to the final coordinate calculated.
//
// latOriginDegree & longMeridianDegree are the "center" latitiude and
// longitude in decimal degrees of the area being projected. All coordinates
// will be calculated in meters relative to this point on the earth.
//
// firstStdParallelDegree & secondStdParallelDegree are the two lines of
// longitude in decimal degrees (that is they run east-west) that define
// where the "cone" intersects the earth. They bracket the area being projected.
bool GeoProjectionConverter::set_lambert_conformal_conic_projection(double falseEastingMeter, double falseNorthingMeter, double latOriginDegree, double longMeridianDegree, double firstStdParallelDegree, double secondStdParallelDegree, char* description)
{
  lcc_false_easting_meter = falseEastingMeter;
  lcc_false_northing_meter = falseNorthingMeter;
  lcc_lat_origin_degree = latOriginDegree;
  lcc_long_meridian_degree = longMeridianDegree;
  lcc_first_std_parallel_degree = firstStdParallelDegree;
  lcc_second_std_parallel_degree = secondStdParallelDegree;

  lcc_lat_origin_radian = deg2rad*lcc_lat_origin_degree;
  lcc_long_meridian_radian = deg2rad*lcc_long_meridian_degree;
  lcc_first_std_parallel_radian = deg2rad*lcc_first_std_parallel_degree;
  lcc_second_std_parallel_radian = deg2rad*lcc_second_std_parallel_degree;
  
  compute_lcc_parameters();
  sprintf(projection_name,"Lambert Conformal Conic");

  if (description)
  {
    sprintf(description, "false east/north: %g/%g [m], origin lat/ meridian long: %g/%g, parallel 1st/2nd: %g/%g", lcc_false_easting_meter, lcc_false_northing_meter, lcc_lat_origin_degree, lcc_long_meridian_degree, lcc_first_std_parallel_degree, lcc_second_std_parallel_degree);
  }
  return true;
}

/*
  * The function set_transverse_mercator_projection receives the Tranverse
  * Mercator projection parameters as input and sets the corresponding state
  * variables. 
  * falseEastingMeter   : Easting/X in meters at the center of the projection
  * falseNorthingMeter  : Northing/Y in meters at the center of the projection
  * latOriginDegree     : Latitude in decimal degree at the origin of the projection
  * longMeridianDegree  : Longitude n decimal degree at the center of the projection
  * scaleFactor         : Projection scale factor
*/
bool GeoProjectionConverter::set_transverse_mercator_projection(double falseEastingMeter, double falseNorthingMeter, double latOriginDegree, double longMeridianDegree, double scaleFactor, char* description)
{
  tm_false_easting_meter = falseEastingMeter;
  tm_false_northing_meter = falseNorthingMeter;
  tm_lat_origin_degree = latOriginDegree;
  tm_long_meridian_degree = longMeridianDegree;
  tm_scale_factor = scaleFactor;

  tm_lat_origin_radian = deg2rad*tm_lat_origin_degree;
  tm_long_meridian_radian = deg2rad*tm_long_meridian_degree;

  compute_tm_parameters();
  sprintf(projection_name,"Transverse Mercator");

  if (description)
  {
    sprintf(description, "false east/north: %g/%g [m], origin lat/ meridian long: %g/%g, scale factor: %g", tm_false_easting_meter, tm_false_northing_meter, tm_lat_origin_degree, tm_long_meridian_degree, tm_scale_factor);
  }

  return true;
}

bool GeoProjectionConverter::set_state_plane_nad27_lcc(const char* zone, char* description)
{
  int i = 0;
  while (state_plane_lcc_nad27_list[i].zone)
  {
    if (strcmp(zone, state_plane_lcc_nad27_list[i].zone) == 0)
    {
      set_reference_ellipsoid(5);
      return set_lambert_conformal_conic_projection(state_plane_lcc_nad27_list[i].falseEastingMeter, state_plane_lcc_nad27_list[i].falseNorthingMeter,state_plane_lcc_nad27_list[i].latOriginDegree,state_plane_lcc_nad27_list[i].longMeridianDegree,state_plane_lcc_nad27_list[i].firstStdParallelDegree,state_plane_lcc_nad27_list[i].secondStdParallelDegree,description);
    }
    i++;
  }
  return false;
}

void GeoProjectionConverter::print_all_state_plane_nad27_lcc()
{
  int i = 0;
  while (state_plane_lcc_nad27_list[i].zone)
  {
    fprintf(stderr, "%s - false east/north: %g/%g [m], origin lat/meridian long: %g/%g, parallel 1st/2nd: %g/%g\n", state_plane_lcc_nad27_list[i].zone, state_plane_lcc_nad27_list[i].falseEastingMeter, state_plane_lcc_nad27_list[i].falseNorthingMeter,state_plane_lcc_nad27_list[i].latOriginDegree,state_plane_lcc_nad27_list[i].longMeridianDegree,state_plane_lcc_nad27_list[i].firstStdParallelDegree,state_plane_lcc_nad27_list[i].secondStdParallelDegree);
    i++;
  }
}

bool GeoProjectionConverter::set_state_plane_nad83_lcc(const char* zone, char* description)
{
  int i = 0;
  while (state_plane_lcc_nad83_list[i].zone)
  {
    if (strcmp(zone, state_plane_lcc_nad83_list[i].zone) == 0)
    {
      set_reference_ellipsoid(11);
      return set_lambert_conformal_conic_projection(state_plane_lcc_nad83_list[i].falseEastingMeter, state_plane_lcc_nad83_list[i].falseNorthingMeter,state_plane_lcc_nad83_list[i].latOriginDegree,state_plane_lcc_nad83_list[i].longMeridianDegree,state_plane_lcc_nad83_list[i].firstStdParallelDegree,state_plane_lcc_nad83_list[i].secondStdParallelDegree,description);
    }
    i++;
  }
  return false;
}

void GeoProjectionConverter::print_all_state_plane_nad83_lcc()
{
  int i = 0;
  while (state_plane_lcc_nad83_list[i].zone)
  {
    fprintf(stderr, "%s - false east/north: %g/%g [m], origin lat/meridian long: %g/%g, parallel 1st/2nd: %g/%g\n", state_plane_lcc_nad83_list[i].zone, state_plane_lcc_nad83_list[i].falseEastingMeter, state_plane_lcc_nad83_list[i].falseNorthingMeter,state_plane_lcc_nad83_list[i].latOriginDegree,state_plane_lcc_nad83_list[i].longMeridianDegree,state_plane_lcc_nad83_list[i].firstStdParallelDegree,state_plane_lcc_nad83_list[i].secondStdParallelDegree);
    i++;
  }
}

bool GeoProjectionConverter::set_state_plane_nad27_tm(const char* zone, char* description)
{
  int i = 0;
  while (state_plane_tm_nad27_list[i].zone)
  {
    if (strcmp(zone, state_plane_tm_nad27_list[i].zone) == 0)
    {
      set_reference_ellipsoid(5);
      return set_transverse_mercator_projection(state_plane_tm_nad27_list[i].falseEastingMeter, state_plane_tm_nad27_list[i].falseNorthingMeter,state_plane_tm_nad27_list[i].latOriginDegree,state_plane_tm_nad27_list[i].longMeridianDegree,state_plane_tm_nad27_list[i].scaleFactor,description);
    }
    i++;
  }
  return false;
}

void GeoProjectionConverter::print_all_state_plane_nad27_tm()
{
  int i = 0;
  while (state_plane_tm_nad27_list[i].zone)
  {
    fprintf(stderr, "%s - false east/north: %g/%g [m], origin lat/meridian long: %g/%g, scale factor: %g\n", state_plane_tm_nad27_list[i].zone, state_plane_tm_nad27_list[i].falseEastingMeter, state_plane_tm_nad27_list[i].falseNorthingMeter,state_plane_tm_nad27_list[i].latOriginDegree,state_plane_tm_nad27_list[i].longMeridianDegree,state_plane_tm_nad27_list[i].scaleFactor);
    i++;
  }
}

bool GeoProjectionConverter::set_state_plane_nad83_tm(const char* zone, char* description)
{
  int i = 0;
  while (state_plane_tm_nad83_list[i].zone)
  {
    if (strcmp(zone, state_plane_tm_nad83_list[i].zone) == 0)
    {
      set_reference_ellipsoid(11);
      return set_transverse_mercator_projection(state_plane_tm_nad83_list[i].falseEastingMeter, state_plane_tm_nad83_list[i].falseNorthingMeter,state_plane_tm_nad83_list[i].latOriginDegree,state_plane_tm_nad83_list[i].longMeridianDegree,state_plane_tm_nad83_list[i].scaleFactor,description);
    }
    i++;
  }
  return false;
}

void GeoProjectionConverter::print_all_state_plane_nad83_tm()
{
  int i = 0;
  while (state_plane_tm_nad83_list[i].zone)
  {
    fprintf(stderr, "%s - false east/north: %g/%g [m], origin lat/meridian long: %g/%g, scale factor: %g\n", state_plane_tm_nad83_list[i].zone, state_plane_tm_nad83_list[i].falseEastingMeter, state_plane_tm_nad83_list[i].falseNorthingMeter,state_plane_tm_nad83_list[i].latOriginDegree,state_plane_tm_nad83_list[i].longMeridianDegree,state_plane_tm_nad83_list[i].scaleFactor);
    i++;
  }
}

bool GeoProjectionConverter::has_projection()
{
  return (projection_name[0] != '\0');
}

const char* GeoProjectionConverter::get_projection_name()
{
  return projection_name;
}

void GeoProjectionConverter::compute_lcc_parameters()
{
  double es_sin;
  double t0,t1,t2;
  double m1,m2;

  es_sin = eccentricity * sin(lcc_lat_origin_radian);
  t0 = tan(PI_OVER_4 - lcc_lat_origin_radian / 2) /  pow((1.0 - es_sin) / (1.0 + es_sin), (eccentricity / 2.0));
  es_sin = eccentricity * sin(lcc_first_std_parallel_radian);
  t1 = tan(PI_OVER_4 - lcc_first_std_parallel_radian / 2) /  pow((1.0 - es_sin) / (1.0 + es_sin), (eccentricity / 2.0));
  m1 = cos(lcc_first_std_parallel_radian) / sqrt(1.0 - es_sin * es_sin);

  // compute lcc_n
  if (fabs(lcc_first_std_parallel_radian - lcc_second_std_parallel_radian) > 1.0e-10)
  {
    es_sin = eccentricity * sin(lcc_second_std_parallel_radian);
    t2 = tan(PI_OVER_4 - lcc_second_std_parallel_radian / 2) /  pow((1.0 - es_sin) / (1.0 + es_sin), (eccentricity / 2.0));
    m2 = cos(lcc_second_std_parallel_radian) / sqrt(1.0 - es_sin * es_sin);
    lcc_n = log(m1 / m2) / log(t1 / t2);
  }
  else
  {
    lcc_n = sin(lcc_first_std_parallel_radian);
  }

  // compute lcc_aF
  lcc_aF = equatorial_radius * m1 / (lcc_n * pow(t1, lcc_n));

  // compute lcc_rho0;
  if ((t0 == 0) && (lcc_n < 0))
    lcc_rho0 = 0.0;
  else
    lcc_rho0 = lcc_aF * pow(t0, lcc_n);
}

void GeoProjectionConverter::compute_tm_parameters()
{
  /*True meridianal constants  */
  double tn = (equatorial_radius - polar_radius) / (equatorial_radius + polar_radius);
  double tn_other = eccentricity_e1;
  double tn2 = tn * tn;
  double tn3 = tn2 * tn;
  double tn4 = tn3 * tn;
  double tn5 = tn4 * tn;

  tm_ap = equatorial_radius * (1.e0 - tn + 5.e0 * (tn2 - tn3)/4.e0 + 81.e0 * (tn4 - tn5)/64.e0 );
  tm_bp = 3.e0 * equatorial_radius * (tn - tn2 + 7.e0 * (tn3 - tn4) /8.e0 + 55.e0 * tn5/64.e0 )/2.e0;
  tm_cp = 15.e0 * equatorial_radius * (tn2 - tn3 + 3.e0 * (tn4 - tn5 )/4.e0) /16.0;
  tm_dp = 35.e0 * equatorial_radius * (tn3 - tn4 + 11.e0 * tn5 / 16.e0) / 48.e0;
  tm_ep = 315.e0 * equatorial_radius * (tn4 - tn5) / 512.e0;
}
// converts lat/long to UTM coords.  Equations from USGS Bulletin 1532 
// East Longitudes are positive, West longitudes are negative. 
// North latitudes are positive, South latitudes are negative
// LatDegree and LongDegree are in decimal degrees
// adapted from code written by Chuck Gantz- chuck.gantz@globalstar.com

bool GeoProjectionConverter::LLtoUTM(const double LatDegree, const double LongDegree, double &UTMEastingMeter, double &UTMNorthingMeter, char* UTMZone)
{
  const double k0 = 0.9996;
  
  // Make sure the longitude is between -180.00 .. 179.9
  double LongTemp = (LongDegree+180)-int((LongDegree+180)/360)*360-180; // -180.00 .. 179.9;
  double LatRad = LatDegree*deg2rad;
  double LongRad = LongTemp*deg2rad;

  int ZoneNumber = int((LongTemp + 180)/6) + 1;
  
  if( LatDegree >= 56.0 && LatDegree < 64.0 && LongTemp >= 3.0 && LongTemp < 12.0 ) ZoneNumber = 32;

  // Special zones for Svalbard
  if( LatDegree >= 72.0 && LatDegree < 84.0 ) 
  {
    if(      LongTemp >= 0.0  && LongTemp <  9.0 ) ZoneNumber = 31;
    else if( LongTemp >= 9.0  && LongTemp < 21.0 ) ZoneNumber = 33;
    else if( LongTemp >= 21.0 && LongTemp < 33.0 ) ZoneNumber = 35;
    else if( LongTemp >= 33.0 && LongTemp < 42.0 ) ZoneNumber = 37;
  }

  double LongOriginRad = ((ZoneNumber - 1)*6 - 180 + 3) * deg2rad;  // + 3 puts origin in middle of zone

  // compute the UTM Zone from the latitude and longitude
  sprintf(UTMZone, "%d%c", ZoneNumber, UTMLetterDesignator(LatDegree));

  double N = equatorial_radius/sqrt(1-eccentricity_squared*sin(LatRad)*sin(LatRad));
  double T = tan(LatRad)*tan(LatRad);
  double C = eccentricity_prime_squared*cos(LatRad)*cos(LatRad);
  double A = cos(LatRad)*(LongRad-LongOriginRad);

  double M = equatorial_radius*((1  - eccentricity_squared/4 - 3*eccentricity_squared*eccentricity_squared/64  - 5*eccentricity_squared*eccentricity_squared*eccentricity_squared/256)*LatRad 
              - (3*eccentricity_squared/8  + 3*eccentricity_squared*eccentricity_squared/32  + 45*eccentricity_squared*eccentricity_squared*eccentricity_squared/1024)*sin(2*LatRad)
             + (15*eccentricity_squared*eccentricity_squared/256 + 45*eccentricity_squared*eccentricity_squared*eccentricity_squared/1024)*sin(4*LatRad) 
             - (35*eccentricity_squared*eccentricity_squared*eccentricity_squared/3072)*sin(6*LatRad));
  
  UTMEastingMeter = (double)(k0*N*(A+(1-T+C)*A*A*A/6
          + (5-18*T+T*T+72*C-58*eccentricity_prime_squared)*A*A*A*A*A/120)
          + 500000.0);

  UTMNorthingMeter = (double)(k0*(M+N*tan(LatRad)*(A*A/2+(5-T+9*C+4*C*C)*A*A*A*A/24
         + (61-58*T+T*T+600*C-330*eccentricity_prime_squared)*A*A*A*A*A*A/720)));

  if(LatDegree < 0)
  {
    UTMNorthingMeter += 10000000.0; //10000000 meter offset for southern hemisphere
  }

  return true;
}

// This routine determines the correct UTM letter designator for the given latitude
// returns 'Z' if latitude is outside the UTM limits of 84N to 80S
// Written by Chuck Gantz- chuck.gantz@globalstar.com
char GeoProjectionConverter::UTMLetterDesignator(double LatDegree)
{
  char LetterDesignator;

  if((84 >= LatDegree) && (LatDegree >= 72)) LetterDesignator = 'X';
  else if((72 > LatDegree) && (LatDegree >= 64)) LetterDesignator = 'W';
  else if((64 > LatDegree) && (LatDegree >= 56)) LetterDesignator = 'V';
  else if((56 > LatDegree) && (LatDegree >= 48)) LetterDesignator = 'U';
  else if((48 > LatDegree) && (LatDegree >= 40)) LetterDesignator = 'T';
  else if((40 > LatDegree) && (LatDegree >= 32)) LetterDesignator = 'S';
  else if((32 > LatDegree) && (LatDegree >= 24)) LetterDesignator = 'R';
  else if((24 > LatDegree) && (LatDegree >= 16)) LetterDesignator = 'Q';
  else if((16 > LatDegree) && (LatDegree >= 8)) LetterDesignator = 'P';
  else if(( 8 > LatDegree) && (LatDegree >= 0)) LetterDesignator = 'N';
  else if(( 0 > LatDegree) && (LatDegree >= -8)) LetterDesignator = 'M';
  else if((-8> LatDegree) && (LatDegree >= -16)) LetterDesignator = 'L';
  else if((-16 > LatDegree) && (LatDegree >= -24)) LetterDesignator = 'K';
  else if((-24 > LatDegree) && (LatDegree >= -32)) LetterDesignator = 'J';
  else if((-32 > LatDegree) && (LatDegree >= -40)) LetterDesignator = 'H';
  else if((-40 > LatDegree) && (LatDegree >= -48)) LetterDesignator = 'G';
  else if((-48 > LatDegree) && (LatDegree >= -56)) LetterDesignator = 'F';
  else if((-56 > LatDegree) && (LatDegree >= -64)) LetterDesignator = 'E';
  else if((-64 > LatDegree) && (LatDegree >= -72)) LetterDesignator = 'D';
  else if((-72 > LatDegree) && (LatDegree >= -80)) LetterDesignator = 'C';
  else LetterDesignator = 'Z'; //This is here as an error flag to show that the Latitude is outside the UTM limits

  return LetterDesignator;
}

// converts UTM coords to lat/long.  Equations from USGS Bulletin 1532 
// East Longitudes are positive, West longitudes are negative. 
// North latitudes are positive, South latitudes are negative
// Lat and LongDegree are in decimal degrees. 
// adapted from code written by Chuck Gantz- chuck.gantz@globalstar.com

bool GeoProjectionConverter::UTMtoLL(const double UTMEastingMeter, const double UTMNorthingMeter, double& LatDegree,  double& LongDegree)
{
  const double k0 = 0.9996;

  double x = UTMEastingMeter - 500000.0; // remove 500,000 meter offset for longitude
  double y = UTMNorthingMeter;

  if (!utm_northern_hemisphere)
  {
    y -= 10000000.0; //remove 10,000,000 meter offset used for southern hemisphere
  }

  double M = y / k0;
  double mu = M/(equatorial_radius*(1-eccentricity_squared/4-3*eccentricity_squared*eccentricity_squared/64-5*eccentricity_squared*eccentricity_squared*eccentricity_squared/256));

  double phi1Rad = mu  + (3*eccentricity_e1/2-27*eccentricity_e1*eccentricity_e1*eccentricity_e1/32)*sin(2*mu) 
                      + (21*eccentricity_e1*eccentricity_e1/16-55*eccentricity_e1*eccentricity_e1*eccentricity_e1*eccentricity_e1/32)*sin(4*mu)
                      + (151*eccentricity_e1*eccentricity_e1*eccentricity_e1/96)*sin(6*mu);
  double phi1 = phi1Rad*rad2deg;

  double N1 = equatorial_radius/sqrt(1-eccentricity_squared*sin(phi1Rad)*sin(phi1Rad));
  double T1 = tan(phi1Rad)*tan(phi1Rad);
  double C1 = eccentricity_prime_squared*cos(phi1Rad)*cos(phi1Rad);
  double R1 = equatorial_radius*(1-eccentricity_squared)/pow(1-eccentricity_squared*sin(phi1Rad)*sin(phi1Rad), 1.5);
  double D = x/(N1*k0);

  LatDegree = phi1Rad - (N1*tan(phi1Rad)/R1)*(D*D/2-(5+3*T1+10*C1-4*C1*C1-9*eccentricity_prime_squared)*D*D*D*D/24
                + (61+90*T1+298*C1+45*T1*T1-252*eccentricity_prime_squared-3*C1*C1)*D*D*D*D*D*D/720);
  LatDegree = LatDegree * rad2deg;

  LongDegree = (D-(1+2*T1+C1)*D*D*D/6+(5-2*C1+28*T1-3*C1*C1+8*eccentricity_prime_squared+24*T1*T1)*D*D*D*D*D/120)/cos(phi1Rad);
  LongDegree = LongDegree * rad2deg + utm_long_origin;

  return true;
}

/*
An alternate way to convert Lambert Conic Conformal Northing/Easting coordinates
into Latitude & Longitude coordinates. The code adapted from Brenor Brophy
(brenor dot brophy at gmail dot com) Homepage:  www.brenorbrophy.com 
*/
void lcc2ll( double e2, // Square of eccentricity
             double a,  // Equatorial Radius
             double firstStdParallel, 
             double secondStdParallel,
             double latOfOrigin,
             double longOfOrigin,
             double lccEasting,  // Lambert coordinates of the point
             double lccNorthing,
             double falseEasting,
             double falseNorthing,
             double &LatDegree, // Latitude & Longitude of the point
             double &LongDegree)
{
  double e = sqrt(e2);
  double phi1  = deg2rad*firstStdParallel;  // Latitude of 1st std parallel
  double phi2  = deg2rad*secondStdParallel; // Latitude of 2nd std parallel
  double phio  = deg2rad*latOfOrigin;       // Latitude of  Origin
  double lamdao  = deg2rad*longOfOrigin;    // Longitude of  Origin
  double E    = lccEasting;
  double N    = lccNorthing;
  double Ef    = falseEasting;
  double Nf    = falseNorthing;

  double m1 = cos(phi1) / sqrt((1 - e2*sin(phi1)*sin(phi1)));
  double m2 = cos(phi2) / sqrt(( 1 - e2*sin(phi2)*sin(phi2)));
  double t1 = tan(PI_OVER_4-(phi1/2)) / pow(( ( 1 - e*sin(phi1) ) / ( 1 + e*sin(phi1) )),e/2);
  double t2 = tan(PI_OVER_4-(phi2/2)) / pow(( ( 1 - e*sin(phi2) ) / ( 1 + e*sin(phi2) )),e/2);
  double to = tan(PI_OVER_4-(phio/2)) / pow(( ( 1 - e*sin(phio) ) / ( 1 + e*sin(phio) )),e/2);
  double n  = (log(m1)-log(m2)) / (log(t1)-log(t2));
  double F  = m1/(n*pow(t1,n));
  double rf  = a*F*pow(to,n);
  double r_  = sqrt( pow((E-Ef),2) + pow((rf-(N-Nf)),2) );
  double t_  = pow(r_/(a*F),(1/n));
  double theta_ = atan((E-Ef)/(rf-(N-Nf)));

  double lamda  = theta_/n + lamdao;
  double phi0  = PI_OVER_2 - 2*atan(t_);
   phi1  = PI_OVER_2 - 2*atan(t_*pow(((1-e*sin(phi0))/(1+e*sin(phi0))),e/2));
   phi2  = PI_OVER_2 - 2*atan(t_*pow(((1-e*sin(phi1))/(1+e*sin(phi1))),e/2));
  double phi  = PI_OVER_2 - 2*atan(t_*pow(((1-e*sin(phi2))/(1+e*sin(phi2))),e/2));
  
  LatDegree = rad2deg*phi;
  LongDegree = rad2deg*lamda;
}

/*
An alternate way to convert a Latitude/Longitude coordinate to an Northing/
Easting coordinate on a Lambert Conic Projection. The Northing/Easting
parameters are calculated are in meters (because the datum used is in
meters) and are relative to the falseNorthing/falseEasting coordinate.
Which in turn is relative to the Lat/Long of origin. The formula were
obtained from URL: http://www.ihsenergy.com/epsg/guid7_2.html.
The code adapted from Brenor Brophy (brenor dot brophy at gmail dot com)
Homepage:  www.brenorbrophy.com 
*/
void ll2lcc( double e2, // Square of eccentricity
             double a,  // Equatorial Radius
             double firstStdParallel, 
             double secondStdParallel,
             double latOfOrigin,
             double longOfOrigin,
             double &lccEasting,  // Lambert coordinates of the point
             double &lccNorthing,
             double falseEasting,
             double falseNorthing,
             double LatDegree, // Latitude & Longitude of the point
             double LongDegree)
{
  double e = sqrt(e2);
  double phi = deg2rad*LatDegree;           // Latitude to convert
  double phi1 = deg2rad*firstStdParallel;   // Latitude of 1st std parallel
  double phi2 = deg2rad*secondStdParallel;  // Latitude of 2nd std parallel
  double lamda = deg2rad*LongDegree;        // Lonitude to convert
  double phio = deg2rad*latOfOrigin;        // Latitude of  Origin
  double lamdao = deg2rad*longOfOrigin;     // Longitude of  Origin

  double m1 = cos(phi1) / sqrt((1 - e2*sin(phi1)*sin(phi1)));
  double m2 = cos(phi2) / sqrt((1 - e2*sin(phi2)*sin(phi2)));
  double t1 = tan(PI_OVER_4-(phi1/2)) / pow(( ( 1 - e*sin(phi1) ) / ( 1 + e*sin(phi1) )),e/2);
  double t2 = tan(PI_OVER_4-(phi2/2)) / pow(( ( 1 - e*sin(phi2) ) / ( 1 + e*sin(phi2) )),e/2);
  double to = tan(PI_OVER_4-(phio/2)) / pow(( ( 1 - e*sin(phio) ) / ( 1 + e*sin(phio) )),e/2);
  double t = tan(PI_OVER_4-(phi /2)) / pow(( ( 1 - e*sin(phi) ) / ( 1 + e*sin(phi) )),e/2);
  double n = (log(m1)-log(m2)) / (log(t1)-log(t2));
  double F = m1/(n*pow(t1,n));
  double rf = a*F*pow(to,n);
  double r = a*F*pow(t,n);
  double theta = n*(lamda - lamdao);

  lccEasting = falseEasting + r*sin(theta);
  lccNorthing = falseNorthing + rf - r*cos(theta);
}

/*
  * The function LCCtoLL() converts Lambert Conformal Conic projection
  * (easting and northing) coordinates to Geodetic (latitude and longitude)
  * coordinates, according to the current ellipsoid and Lambert Conformal
  * Conic projection parameters.
  *
  *   LCCEastingMeter   : input Easting/X in meters 
  *   LLCNorthingMeter  : input Northing/Y in meters
  *   LatDegree         : output Latitude in decimal degrees
  *   LongDegree        : output Longitude in decimal degrees
  *
  * adapted from code by Garrett Potts ((C) 2000 ImageLinks Inc.)
*/
bool GeoProjectionConverter::LCCtoLL(const double LCCEastingMeter, const double LCCNorthingMeter, double& LatDegree,  double& LongDegree) const
{
/* >>> alternate way to compute (but seems less precise) <<<
  lcc2ll(eccentricity_squared,
            equatorial_radius,
            lcc_first_std_parallel_degree,
            lcc_second_std_parallel_degree,
            lcc_lat_origin_degree,
            lcc_long_meridian_degree,
            LCCEastingMeter,
            LCCNorthingMeter,
            lcc_false_easting_meter,
            lcc_false_northing_meter,
            LatDegree,
            LongDegree);
  return true;
*/

  double dx = LCCEastingMeter - lcc_false_easting_meter;
  double dy = LCCNorthingMeter - lcc_false_northing_meter;

  double rho0_MINUS_dy = lcc_rho0 - dy;
  double rho = sqrt(dx * dx + (rho0_MINUS_dy) * (rho0_MINUS_dy));

  if (lcc_n < 0.0)
  {
    rho *= -1.0;
    dy *= -1.0;
    dx *= -1.0;
    rho0_MINUS_dy *= -1.0;
  }

  if (rho != 0.0)
  {
    double theta = atan2(dx, rho0_MINUS_dy);
    double t = pow(rho / (lcc_aF), 1.0 / lcc_n);
    double PHI = PI_OVER_2 - 2.0 * atan(t);
    double tempPHI = 0.0;
    while (fabs(PHI - tempPHI) > 4.85e-10)
    {
      double es_sin= eccentricity * sin(PHI);
      tempPHI = PHI;
      PHI = PI_OVER_2 - 2.0 * atan(t * pow((1.0 - es_sin) / (1.0 + es_sin), eccentricity / 2.0));
    }
    LatDegree = PHI;
    LongDegree = theta / lcc_n + lcc_long_meridian_radian;

    if (fabs(LatDegree) < 2.0e-7)  /* force tiny lat to 0 */
      LatDegree = 0.0;
    else if (LatDegree > PI_OVER_2) /* force distorted lat to 90, -90 degrees */
      LatDegree = 90.0;
    else if (LatDegree < -PI_OVER_2)
      LatDegree = -90.0;
    else
      LatDegree = rad2deg*LatDegree;

    if (fabs(LongDegree) < 2.0e-7)  /* force tiny long to 0 */
      LongDegree = 0.0;
    else if (LongDegree > PI) /* force distorted long to 180, -180 degrees */
      LongDegree = 180.0;
    else if (LongDegree < -PI)
      LongDegree = -180.0;
    else
      LongDegree = rad2deg*LongDegree;
  }
  else
  {
    if (lcc_n > 0.0)
      LatDegree = 90.0;
    else
      LatDegree = -90.0;
    LongDegree = lcc_long_meridian_degree;
  }
  return true;
}

/*
  * The function LLtoLCC() converts Geodetic (latitude and longitude)
  * coordinates to Lambert Conformal Conic projection (easting and
  * northing) coordinates, according to the current ellipsoid and
  * Lambert Conformal Conic projection parameters. 
  *
  *   LatDegree         : input Latitude in decimal degrees
  *   LongDegree        : input Longitude in decimal degrees
  *   LCCEastingMeter   : output Easting/X in meters 
  *   LLCNorthingMeter  : output Northing/Y in meters
  *
  * adapted from code by Garrett Potts ((C) 2000 ImageLinks Inc.)
*/
bool GeoProjectionConverter::LLtoLCC(const double LatDegree, const double LongDegree, double& LCCEastingMeter,  double& LCCNorthingMeter) const
{
/* >>> alternate way to compute (but seems less precise) <<< 
  ll2lcc(eccentricity_squared,
            equatorial_radius,
            lcc_first_std_parallel_degree,
            lcc_second_std_parallel_degree,
            lcc_lat_origin_degree,
            lcc_long_meridian_degree,
            LCCEastingMeter,
            LCCNorthingMeter,
            lcc_false_easting_meter,
            lcc_false_northing_meter,
            LatDegreeAlt,
            LongDegreeAlt);
  return true;
*/
  double rho = 0.0;
  double Latitude = LatDegree*deg2rad;
  double Longitude = LongDegree*deg2rad;

  if (fabs(fabs(Latitude) - PI_OVER_2) > 1.0e-10)
  {
    double slat = sin(Latitude);
    double es_sin = eccentricity*slat;
    double t = tan(PI_OVER_4 - Latitude / 2) / pow((1.0 - es_sin) / (1.0 + es_sin), eccentricity/2);
    rho = lcc_aF * pow(t, lcc_n);
  }
  else
  {
    if ((Latitude * lcc_n) <= 0)
    { // Point can not be projected
      return false;
    }
  }

  double dlam = Longitude - lcc_long_meridian_radian;

  double theta = lcc_n * dlam;

  LCCEastingMeter = rho * sin(theta) + lcc_false_easting_meter;
  LCCNorthingMeter = lcc_rho0 - rho * cos(theta) + lcc_false_northing_meter;

  return true;
}

#define SPHSN(Latitude) ((double) (equatorial_radius / sqrt( 1.e0 - eccentricity_squared * pow(sin(Latitude), 2))))

#define SPHTMD(Latitude) ((double) (tm_ap * Latitude \
                          - tm_bp * sin(2.e0 * Latitude) + tm_cp * sin(4.e0 * Latitude) \
                          - tm_dp * sin(6.e0 * Latitude) + tm_ep * sin(8.e0 * Latitude) ) )

#define DENOM(Latitude) ((double) (sqrt(1.e0 - eccentricity_squared * pow(sin(Latitude),2))))
#define SPHSR(Latitude) ((double) (equatorial_radius * (1.e0 - eccentricity_squared) / pow(DENOM(Latitude), 3)))

/*
  * The function LLtoTM() converts geodetic (latitude and longitude)
  * coordinates to Transverse Mercator projection (easting and northing)
  * coordinates, according to the current ellipsoid and Transverse Mercator 
  * projection parameters.  
  *
  *   LatDegree        : input Latitude in decimal degrees
  *   LongDegree       : input Longitude in decimal degrees
  *   TMEastingMeter   : output Easting/X in meters 
  *   TMNorthingMeter  : output Northing/Y in meters
  *
  * adapted from code by Garrett Potts ((C) 2000 ImageLinks Inc.)
*/
bool GeoProjectionConverter::LLtoTM(const double LatDegree, const double LongDegree, double& TMEastingMeter,  double& TMNorthingMeter) const
{
  double Latitude = LatDegree*deg2rad;
  double Longitude = LongDegree*deg2rad;

  double c;       /* Cosine of latitude                          */
  double c2;
  double c3;
  double c5;
  double c7;
  double dlam;    /* Delta longitude - Difference in Longitude       */
  double eta;     /* constant - eccentricity_prime_squared *c *c                   */
  double eta2;
  double eta3;
  double eta4;
  double s;       /* Sine of latitude                        */
  double sn;      /* Radius of curvature in the prime vertical       */
  double t;       /* Tangent of latitude                             */
  double tan2;
  double tan3;
  double tan4;
  double tan5;
  double tan6;
  double t1;      /* Term in coordinate conversion formula - GP to Y */
  double t2;      /* Term in coordinate conversion formula - GP to Y */
  double t3;      /* Term in coordinate conversion formula - GP to Y */
  double t4;      /* Term in coordinate conversion formula - GP to Y */
  double t5;      /* Term in coordinate conversion formula - GP to Y */
  double t6;      /* Term in coordinate conversion formula - GP to Y */
  double t7;      /* Term in coordinate conversion formula - GP to Y */
  double t8;      /* Term in coordinate conversion formula - GP to Y */
  double t9;      /* Term in coordinate conversion formula - GP to Y */
  double tmd;     /* True Meridional distance                        */
  double tmdo;    /* True Meridional distance for latitude of origin */

  if (Longitude > PI) Longitude -= TWO_PI;

  dlam = Longitude - tm_long_meridian_radian;

  if (dlam > PI)
    dlam -= TWO_PI;
  if (dlam < -PI)
    dlam += TWO_PI;
  if (fabs(dlam) < 2.e-10)
    dlam = 0.0;

  s = sin(Latitude);
  c = cos(Latitude);
  c2 = c * c;
  c3 = c2 * c;
  c5 = c3 * c2;
  c7 = c5 * c2;
  t = tan (Latitude);
  tan2 = t * t;
  tan3 = tan2 * t;
  tan4 = tan3 * t;
  tan5 = tan4 * t;
  tan6 = tan5 * t;
  eta = eccentricity_prime_squared * c2;
  eta2 = eta * eta;
  eta3 = eta2 * eta;
  eta4 = eta3 * eta;

  /* radius of curvature in prime vertical */
  sn = SPHSN(Latitude);

  /* True Meridianal Distances */
  tmd = SPHTMD(Latitude);

  /*  Origin  */
  tmdo = SPHTMD (tm_lat_origin_radian);

  /* northing */
  t1 = (tmd - tmdo) * tm_scale_factor;
  t2 = sn * s * c * tm_scale_factor/ 2.e0;
  t3 = sn * s * c3 * tm_scale_factor * (5.e0 - tan2 + 9.e0 * eta 
                                             + 4.e0 * eta2) /24.e0; 

  t4 = sn * s * c5 * tm_scale_factor * (61.e0 - 58.e0 * tan2
                                             + tan4 + 270.e0 * eta - 330.e0 * tan2 * eta + 445.e0 * eta2
                                             + 324.e0 * eta3 -680.e0 * tan2 * eta2 + 88.e0 * eta4 
                                             -600.e0 * tan2 * eta3 - 192.e0 * tan2 * eta4) / 720.e0;

  t5 = sn * s * c7 * tm_scale_factor * (1385.e0 - 3111.e0 * 
                                             tan2 + 543.e0 * tan4 - tan6) / 40320.e0;

  TMNorthingMeter = tm_false_northing_meter + t1 + pow(dlam,2.e0) * t2 + pow(dlam,4.e0) * t3 + pow(dlam,6.e0) * t4 + pow(dlam,8.e0) * t5; 

  /* Easting */
  t6 = sn * c * tm_scale_factor;
  t7 = sn * c3 * tm_scale_factor * (1.e0 - tan2 + eta ) /6.e0;
  t8 = sn * c5 * tm_scale_factor * (5.e0 - 18.e0 * tan2 + tan4
                                         + 14.e0 * eta - 58.e0 * tan2 * eta + 13.e0 * eta2 + 4.e0 * eta3 
                                         - 64.e0 * tan2 * eta2 - 24.e0 * tan2 * eta3 )/ 120.e0;
  t9 = sn * c7 * tm_scale_factor * ( 61.e0 - 479.e0 * tan2
                                          + 179.e0 * tan4 - tan6 ) /5040.e0;

  TMEastingMeter = tm_false_easting_meter + dlam * t6 + pow(dlam,3.e0) * t7 + pow(dlam,5.e0) * t8 + pow(dlam,7.e0) * t9;

  return true;
}
 
/*
  * The function TMtoLL() converts Transverse Mercator projection (easting and
  * northing) coordinates to geodetic (latitude and longitude) coordinates, 
  * according to the current ellipsoid and Transverse Mercator projection
  * parameters.
  *
  *   TMEastingMeter   : input Easting/X in meters 
  *   TMNorthingMeter  : input Northing/Y in meters
  *   LatDegree        : output Latitude in decimal degrees
  *   LongDegree       : output Longitude in decimal degrees
  *
  * adapted from code by Garrett Potts ((C) 2000 ImageLinks Inc.)
*/
bool GeoProjectionConverter::TMtoLL(const double TMEastingMeter, const double TMNorthingMeter, double& LatDegree,  double& LongDegree) const
{
  double c;       /* Cosine of latitude                          */
  double de;      /* Delta easting - Difference in Easting (Easting-Fe)    */
  double dlam;    /* Delta longitude - Difference in Longitude       */
  double eta;     /* constant - eccentricity_prime_squared *c *c                   */
  double eta2;
  double eta3;
  double eta4;
  double ftphi;   /* Footpoint latitude                              */
  int    i;       /* Loop iterator                   */
  double s;       /* Sine of latitude                        */
  double sn;      /* Radius of curvature in the prime vertical       */
  double sr;      /* Radius of curvature in the meridian             */
  double t;       /* Tangent of latitude                             */
  double tan2;
  double tan4;
  double t10;     /* Term in coordinate conversion formula - GP to Y */
  double t11;     /* Term in coordinate conversion formula - GP to Y */
  double t12;     /* Term in coordinate conversion formula - GP to Y */
  double t13;     /* Term in coordinate conversion formula - GP to Y */
  double t14;     /* Term in coordinate conversion formula - GP to Y */
  double t15;     /* Term in coordinate conversion formula - GP to Y */
  double t16;     /* Term in coordinate conversion formula - GP to Y */
  double t17;     /* Term in coordinate conversion formula - GP to Y */
  double tmd;     /* True Meridional distance                        */
  double tmdo;    /* True Meridional distance for latitude of origin */

  /* True Meridional Distances for latitude of origin */
  tmdo = SPHTMD(tm_lat_origin_radian);

  /*  Origin  */
  tmd = tmdo +  (TMNorthingMeter - tm_false_northing_meter) / tm_scale_factor; 

  /* First Estimate */
  sr = SPHSR(0.e0);
  ftphi = tmd/sr;

  for (i = 0; i < 5 ; i++)
  {
   t10 = SPHTMD (ftphi);
   sr = SPHSR(ftphi);
   ftphi = ftphi + (tmd - t10) / sr;
  }

  /* Radius of Curvature in the meridian */
  sr = SPHSR(ftphi);

  /* Radius of Curvature in the meridian */
  sn = SPHSN(ftphi);

  /* Sine Cosine terms */
  s = sin(ftphi);
  c = cos(ftphi);

  /* Tangent Value  */
  t = tan(ftphi);
  tan2 = t * t;
  tan4 = tan2 * tan2;
  eta = eccentricity_prime_squared * pow(c,2);
  eta2 = eta * eta;
  eta3 = eta2 * eta;
  eta4 = eta3 * eta;
  de = TMEastingMeter - tm_false_easting_meter;
  if (fabs(de) < 0.0001)
   de = 0.0;

  /* Latitude */
  double Latitude;
  t10 = t / (2.e0 * sr * sn * pow(tm_scale_factor, 2));
  t11 = t * (5.e0  + 3.e0 * tan2 + eta - 4.e0 * pow(eta,2)
            - 9.e0 * tan2 * eta) / (24.e0 * sr * pow(sn,3) 
                                    * pow(tm_scale_factor,4));
  t12 = t * (61.e0 + 90.e0 * tan2 + 46.e0 * eta + 45.E0 * tan4
            - 252.e0 * tan2 * eta  - 3.e0 * eta2 + 100.e0 
            * eta3 - 66.e0 * tan2 * eta2 - 90.e0 * tan4
            * eta + 88.e0 * eta4 + 225.e0 * tan4 * eta2
            + 84.e0 * tan2* eta3 - 192.e0 * tan2 * eta4)
       / ( 720.e0 * sr * pow(sn,5) * pow(tm_scale_factor, 6) );
  t13 = t * ( 1385.e0 + 3633.e0 * tan2 + 4095.e0 * tan4 + 1575.e0 
             * pow(t,6))/ (40320.e0 * sr * pow(sn,7) * pow(tm_scale_factor,8));
  Latitude = ftphi - pow(de,2) * t10 + pow(de,4) * t11 - pow(de,6) * t12 + pow(de,8) * t13;

  t14 = 1.e0 / (sn * c * tm_scale_factor);

  t15 = (1.e0 + 2.e0 * tan2 + eta) / (6.e0 * pow(sn,3) * c * 
                                     pow(tm_scale_factor,3));

  t16 = (5.e0 + 6.e0 * eta + 28.e0 * tan2 - 3.e0 * eta2
        + 8.e0 * tan2 * eta + 24.e0 * tan4 - 4.e0 
        * eta3 + 4.e0 * tan2 * eta2 + 24.e0 
        * tan2 * eta3) / (120.e0 * pow(sn,5) * c  
                          * pow(tm_scale_factor,5));

  t17 = (61.e0 +  662.e0 * tan2 + 1320.e0 * tan4 + 720.e0 
        * pow(t,6)) / (5040.e0 * pow(sn,7) * c 
                       * pow(tm_scale_factor,7));

  /* Difference in Longitude */
  dlam = de * t14 - pow(de,3) * t15 + pow(de,5) * t16 - pow(de,7) * t17;

  /* Longitude */
  double Longitude = tm_long_meridian_radian + dlam;
  while (Latitude > PI_OVER_2)
  {
    Latitude = PI - Latitude;
    Longitude += PI;
    if (Longitude > PI)
     Longitude -= TWO_PI;
  }

  while (Latitude < -PI_OVER_2)
  {
    Latitude = - (Latitude + PI);
    Longitude += PI;
    if (Longitude > PI)
      Longitude -= TWO_PI;
  }
  if (Longitude > TWO_PI)
    Longitude -= TWO_PI;
  if (Longitude < -PI)
    Longitude += TWO_PI;

  LatDegree = rad2deg*Latitude;
  LongDegree = rad2deg*Longitude;

  return true;
}

GeoProjectionConverter::GeoProjectionConverter()
{
  num_geo_keys = 0;
  geo_keys = 0;
  geo_ascii_params = 0;
  geo_double_params = 0;

  ellipsoid_id = 0;
  ellipsoid_name = 0;
  equatorial_radius = 0;
  eccentricity_squared = 0;
  eccentricity = 0;

  projection_name[0] = '\0';

  utm_zone_number = 0;
  utm_zone_letter = 0;
  utm_northern_hemisphere = false;

  lcc_false_easting_meter = 0;
  lcc_false_northing_meter = 0;
  lcc_lat_origin_degree = 0;
  lcc_long_meridian_degree = 0;
  lcc_first_std_parallel_degree = 0;
  lcc_second_std_parallel_degree = 0;
  lcc_lat_origin_radian = 0;
  lcc_long_meridian_radian = 0;
  lcc_first_std_parallel_radian = 0;
  lcc_second_std_parallel_radian = 0;
  lcc_n = 0;
  lcc_aF = 0;
  lcc_rho0 = 0;

  // parameters for convenience
  coordinates2meter = 1.0;
  elevation2meter = 1.0;
  elevation_offset_in_meter = 0.0f;
}

GeoProjectionConverter::~GeoProjectionConverter()
{
}

void GeoProjectionConverter::set_coordinates_in_survey_feet()
{
  coordinates2meter = 0.3048006096012;
}

void GeoProjectionConverter::set_coordinates_in_feet()
{
  coordinates2meter = 0.3048;
}

void GeoProjectionConverter::set_coordinates_in_meter()
{
  coordinates2meter = 1.0;
}

const char* GeoProjectionConverter::get_coordinate_unit_description_string(bool abrev)
{
  return (coordinates2meter == 1.0 ? (abrev ? "m" : "meter") : (coordinates2meter == 0.3048 ? (abrev ? "ft" : "feet") : (abrev ? "sft" : "surveyfeet")));
}

void GeoProjectionConverter::set_elevation_in_feet()
{
  elevation2meter = 0.3048;
}

void GeoProjectionConverter::set_elevation_in_meter()
{
  elevation2meter = 1.0;
}

void GeoProjectionConverter::set_elevation_offset_in_meter(float elevation_offset_in_meter)
{
  this->elevation_offset_in_meter = elevation_offset_in_meter;
}

const char* GeoProjectionConverter::get_elevation_unit_description_string(bool abrev)
{
  return (elevation2meter == 1.0 ? (abrev ? "m" : "meter") : (abrev ? "ft" : "feet"));
}

void GeoProjectionConverter::to_kml_style_lat_long_elevation(const double* point, double& latDeg,  double& longDeg, float& elevation)
{
  if (projection_name[0] == 'U')
    UTMtoLL(point[0], point[1], latDeg, longDeg);
  else if (projection_name[0] == 'L')
    LCCtoLL(coordinates2meter*point[0], coordinates2meter*point[1], latDeg, longDeg);
  else
    TMtoLL(coordinates2meter*point[0], coordinates2meter*point[1], latDeg, longDeg);
  elevation = (float)(elevation2meter*point[2] + elevation_offset_in_meter);
}

void GeoProjectionConverter::to_kml_style_lat_long_elevation(const float* point, double& latDeg,  double& longDeg, float& elevation)
{
  if (projection_name[0] == 'U')
    UTMtoLL(point[0], point[1], latDeg, longDeg);
  else if (projection_name[0] == 'L')
    LCCtoLL(coordinates2meter*point[0], coordinates2meter*point[1], latDeg, longDeg);
  else
    TMtoLL(coordinates2meter*point[0], coordinates2meter*point[1], latDeg, longDeg);
  elevation = (float)(elevation2meter*point[2] + elevation_offset_in_meter);
}

void GeoProjectionConverter::to_kml_style_lat_long_elevation(const int* point, double& latDeg,  double& longDeg, float& elevation)
{
  if (projection_name[0] == 'U')
    UTMtoLL(point[0], point[1], latDeg, longDeg);
  else if (projection_name[0] == 'L')
    LCCtoLL(coordinates2meter*point[0], coordinates2meter*point[1], latDeg, longDeg);
  else
    TMtoLL(coordinates2meter*point[0], coordinates2meter*point[1], latDeg, longDeg);
  elevation = (float)(elevation2meter*point[2] + elevation_offset_in_meter);
}
