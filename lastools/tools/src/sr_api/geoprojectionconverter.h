/*
===============================================================================

  FILE:  geoprojectionconverter.h
  
  CONTENTS:
  
    all functions expect input/output to be in meters / decimal degrees. if
    your lcc cordinates are in survey foot you need to convert them to meters
    first (1 unit [survey foot] * 0.3048006096012 == 1 unit [meter]). 

    converting between UTM coodinates and latitude / longitude coodinates
    adapted from code written by Chuck Gantz (chuck.gantz@globalstar.com)
  
    converting between Lambert Conformal Conic and latitude / longitude
    adapted from code written by Garrett Potts (gpotts@imagelinks.com)

  PROGRAMMERS:
  
    martin isenburg@cs.unc.edu
    chuck.gantz@globalstar.com
    gpotts@imagelinks.com
  
  COPYRIGHT:
  
    copyright (C) 2007  martin isenburg@cs.unc.edu
    
    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
    This is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation.
  
  CHANGE HISTORY:
  
    08 February 2007 -- created after interviews with purdue and google
  
===============================================================================
*/
#ifndef GEOPROJECTIONCONVERTER_H
#define GEOPROJECTIONCONVERTER_H

class GeoProjectionConverterGeoKeys
{
public:
  unsigned short key_id;
  unsigned short tiff_tag_location;
  unsigned short count;
  unsigned short value_offset;
};

class GeoProjectionConverter
{
public:
  // implementation
  bool set_projection_from_geo_keys(int num_geo_keys, GeoProjectionConverterGeoKeys* geo_keys, char* geo_ascii_params, double* geo_double_params, char* description = 0);
  
  short get_GTModelTypeGeoKey();
  short get_GTRasterTypeGeoKey();

  short get_GeographicTypeGeoKey();
  short get_GeogGeodeticDatumGeoKey();
  short get_GeogPrimeMeridianGeoKey();
  short get_GeogLinearUnitsGeoKey();
  double get_GeogLinearUnitSizeGeoKey();
  short get_GeogAngularUnitsGeoKey();
  double get_GeogAngularUnitSizeGeoKey();
  short get_GeogEllipsoidGeoKey();
  double get_GeogSemiMajorAxisGeoKey();
  double get_GeogSemiMinorAxisGeoKey();
  double get_GeogInvFlatteningGeoKey();
  short get_GeogAzimuthUnitsGeoKey();
  double get_GeogPrimeMeridianLongGeoKey();

  short get_ProjectedCSTypeGeoKey();

  bool set_reference_ellipsoid(int id, char* description = 0);
  const char* get_ellipsoid_name();
  bool set_utm_projection(char* zone, char* description = 0);
  bool set_utm_projection(int zone, bool northern, char* description = 0);
  bool set_lambert_conformal_conic_projection(double falseEastingMeter, double falseNorthingMeter, double latOriginDegree, double longMeridianDegree, double firstStdParallelDegree, double secondStdParallelDegree, char* description = 0);
  bool set_transverse_mercator_projection(double falseEastingMeter, double falseNorthingMeter, double latOriginDegree, double longMeridianDegree, double scaleFactor, char* description);

  bool set_state_plane_nad27_lcc(const char* zone, char* description);
  void print_all_state_plane_nad27_lcc();
  bool set_state_plane_nad83_lcc(const char* zone, char* description);
  void print_all_state_plane_nad83_lcc();

  bool set_state_plane_nad27_tm(const char* zone, char* description);
  void print_all_state_plane_nad27_tm();
  bool set_state_plane_nad83_tm(const char* zone, char* description);
  void print_all_state_plane_nad83_tm();

  bool has_projection();
  const char* get_projection_name();

  bool UTMtoLL(const double UTMEastingMeter, const double UTMNorthingMeter, double& LatDegree,  double& LongDegree);
  bool LLtoUTM(const double LatDegree, const double LongDegree, double &UTMEastingMeter, double &UTMNorthingMeter, char* UTMZone);
  char UTMLetterDesignator(double Lat);

  bool LCCtoLL(const double LCCEastingMeter, const double LCCNorthingMeter, double& LatDegree,  double& LongDegree) const;
  bool LLtoLCC(const double LatDegree, const double LongDegree, double &LCCEastingMeter, double &LCCNorthingMeter) const;

  bool TMtoLL(const double TMEastingMeter, const double TMNorthingMeter, double& LatDegree,  double& LongDegree) const;
  bool LLtoTM(const double LatDegree, const double LongDegree, double &TMEastingMeter, double &TMNorthingMeter) const;

  GeoProjectionConverter();
  ~GeoProjectionConverter();

  // convenience
  void set_coordinates_in_survey_feet();
  void set_coordinates_in_feet();
  void set_coordinates_in_meter();
  const char* get_coordinate_unit_description_string(bool abrev=true);
  void set_elevation_in_feet();
  void set_elevation_in_meter();
  void set_elevation_offset_in_meter(float elevation_offset);
  const char* get_elevation_unit_description_string(bool abrev=true);
  void to_kml_style_lat_long_elevation(const double* point, double& latDeg,  double& longDeg, float& elevation);
  void to_kml_style_lat_long_elevation(const float* point, double& latDeg,  double& longDeg, float& elevation);
  void to_kml_style_lat_long_elevation(const int* point, double& latDeg,  double& longDeg, float& elevation);

private:
  // parameters for gtiff
  int num_geo_keys;
  GeoProjectionConverterGeoKeys* geo_keys;
  char* geo_ascii_params;
  double* geo_double_params;

  // parameters for the reference ellipsoid
  int ellipsoid_id;
  char* ellipsoid_name;
  double equatorial_radius;
  double polar_radius;
  double eccentricity_squared;
  double inverse_flattening;
  double eccentricity_prime_squared;
  double eccentricity;
  double eccentricity_e1;

  char projection_name[256];

  // parameters for the utm projection
  int utm_zone_number;
  char utm_zone_letter;
  bool utm_northern_hemisphere;
  int utm_long_origin;

  // parameters for lambert conic conformal projection
  double lcc_false_easting_meter;
  double lcc_false_northing_meter;
  double lcc_lat_origin_degree;
  double lcc_long_meridian_degree;
  double lcc_first_std_parallel_degree;
  double lcc_second_std_parallel_degree;
  double lcc_lat_origin_radian;
  double lcc_long_meridian_radian;
  double lcc_first_std_parallel_radian;
  double lcc_second_std_parallel_radian;
  void compute_lcc_parameters();
  double lcc_n;
  double lcc_aF;
  double lcc_rho0;

  // parameters for the transverse mercator projection
  double tm_false_easting_meter;
  double tm_false_northing_meter;
  double tm_lat_origin_degree;
  double tm_long_meridian_degree;
  double tm_scale_factor;
  double tm_lat_origin_radian;
  double tm_long_meridian_radian;
  void compute_tm_parameters();
  double tm_ap;
  double tm_bp;
  double tm_cp;
  double tm_dp;
  double tm_ep;

  // parameters for convenience
  double coordinates2meter;
  double elevation2meter;
  float elevation_offset_in_meter;
};

#endif
