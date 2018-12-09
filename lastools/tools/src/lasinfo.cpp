/*
===============================================================================

  FILE:  lasinfo.cpp

  CONTENTS:

    This tool reads a LIDAR file in LAS or LAZ format and prints out the info
    from the standard header. It also prints out info from any variable length
    headers and gives detailed info on any geokeys that may be present (this
    can be disabled with the '-no_variable' flag). The real bounding box of the
    points is computed and compared with the bounding box specified in the header
    (this can be disables with the '-no_check' flag). It is also possible to
    change or repair some aspects of the header 

  PROGRAMMERS:

    martin isenburg@cs.unc.edu

  COPYRIGHT:

    copyright (C) 2007  martin isenburg@cs.unc.edu

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    6 August 2009 -- added 'skip_all_headers' to deal with LAS files with bad headers
    10 July 2009 -- '-auto_date' sets the day/year from the file creation date
    12 March 2009 -- updated to ask for input if started without arguments 
    9 March 2009 -- added output for size of user-defined header data
    17 September 2008 -- updated to deal with LAS format version 1.2
    13 July 2007 -- added the option to "repair" the header and change items
    11 June 2007 -- fixed number of return counts after Vinton found another bug
    6 June 2007 -- added lidardouble2string() after Vinton Valentine's bug report
    25 March 2007 -- sitting at the Pacific Coffee after doing the escalators

===============================================================================
*/

#include "lasreader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

void usage(bool wait=false)
{
  fprintf(stderr,"usage:\n");
  fprintf(stderr,"lasinfo lidar.las\n");
  fprintf(stderr,"lasinfo -no_variable lidar.las\n");
  fprintf(stderr,"lasinfo -no_variable -no_check lidar.las\n");
  fprintf(stderr,"lasinfo -i lidar.las -o lidar_info.txt\n");
  fprintf(stderr,"lasinfo -i lidar.las -repair\n");
  fprintf(stderr,"lasinfo -i lidar.las -repair_bounding_box -file_creation 8 2007\n");
  fprintf(stderr,"lasinfo -i lidar.las -set_version 1.2\n");
  fprintf(stderr,"lasinfo -i lidar.las -system_identifier \"hello world!\" -generating_software \"this is a test (-:\"\n");
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

#ifdef _WIN32
extern "C" FILE* fopenGzipped(const char* filename, const char* mode);
#endif

static inline void VecUpdateMinMax3dv(double min[3], double max[3], const double v[3])
{
  if (v[0]<min[0]) min[0]=v[0]; else if (v[0]>max[0]) max[0]=v[0];
  if (v[1]<min[1]) min[1]=v[1]; else if (v[1]>max[1]) max[1]=v[1];
  if (v[2]<min[2]) min[2]=v[2]; else if (v[2]>max[2]) max[2]=v[2];
}

static inline void VecCopy3dv(double v[3], const double a[3])
{
  v[0] = a[0];
  v[1] = a[1];
  v[2] = a[2];
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

int main(int argc, char *argv[])
{
  int i;
  bool parse_variable_header = true;
  bool check_points = true;
  bool repair_header = false;
  bool repair_bounding_box = false;
  bool change_header = false;
  bool skip_all_headers = false;
	char* system_identifier = 0;
	char* generating_software = 0;
	unsigned short file_creation_day = 0;
	unsigned short file_creation_year = 0;
  bool auto_date_creation = false;
  char set_version_major = -1;
  char set_version_minor = -1;
  char* file_name = 0;
  char* file_name_out = 0;
  bool ilas = false;
  FILE* file_out = stderr;
  bool say_peep = false;

  if (argc == 1)
  {
    fprintf(stderr,"lasinfo.exe is better run in the command line\n");
    file_name = new char[256];
    fprintf(stderr,"enter input file: "); fgets(file_name, 256, stdin);
    file_name[strlen(file_name)-1] = '\0';
  }

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i],"-h") == 0)
    {
      usage();
    }
    else if (strcmp(argv[i],"-i") == 0)
    {
      i++;
      file_name = argv[i];
    }
    else if (strcmp(argv[i],"-o") == 0)
    {
      i++;
      file_name_out = argv[i];
    }
    else if (strcmp(argv[i],"-ilas") == 0)
    {
      ilas = true;
    }
    else if (strcmp(argv[i],"-var") == 0 || strcmp(argv[i],"-variable") == 0)
    {
      parse_variable_header = true;
    }
    else if (strcmp(argv[i],"-no_var") == 0 || strcmp(argv[i],"-novar") == 0 || strcmp(argv[i],"-no_variable") == 0 || strcmp(argv[i],"-novariable") == 0)
    {
      parse_variable_header = false;
    }
    else if (strcmp(argv[i],"-points") == 0 || strcmp(argv[i],"-check") == 0 || strcmp(argv[i],"-check_points") == 0)
    {
      check_points = true;
    }
    else if (strcmp(argv[i],"-nocheck") == 0 || strcmp(argv[i],"-no_check") == 0)
    {
      check_points = false;
    }
    else if (strcmp(argv[i],"-quiet") == 0 || strcmp(argv[i],"-be_quiet") == 0)
    {
      file_out = NULL;
    }
    else if (strcmp(argv[i],"-skip") == 0 || strcmp(argv[i],"-skip_headers") == 0)
    {
      skip_all_headers = true;
    }
    else if (strcmp(argv[i],"-peep") == 0)
    {
      say_peep = true;
    }
    else if (strcmp(argv[i],"-repair") == 0 || strcmp(argv[i],"-repair_header") == 0)
    {
      repair_header = true;
    }
    else if (strcmp(argv[i],"-repair_bb") == 0 || strcmp(argv[i],"-repair_boundingbox") == 0 || strcmp(argv[i],"-repair_bounding_box") == 0)
    {
      repair_bounding_box = true;
    }
    else if (strcmp(argv[i],"-auto_date") == 0 || strcmp(argv[i],"-auto_creation_date") == 0 || strcmp(argv[i],"-auto_creation") == 0)
    {
      auto_date_creation = true;
    }
    else if (strcmp(argv[i],"-system_identifier") == 0 || strcmp(argv[i],"-sys_id") == 0)
    {
			i++;
			system_identifier = new char[32];
      strncpy(system_identifier, argv[i], 31);
      system_identifier[31] = '\0';
      change_header = true;
		}
    else if (strcmp(argv[i],"-generating_software") == 0 || strcmp(argv[i],"-gen_soft") == 0)
    {
			i++;
			generating_software = new char[32];
      strncpy(generating_software, argv[i], 31);
      generating_software[31] = '\0';
      change_header = true;
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
      change_header = true;
    }
    else if (strcmp(argv[i],"-file_creation") == 0)
    {
			i++;
      file_creation_day = (unsigned short)atoi(argv[i]);
			i++;
      file_creation_year = (unsigned short)atoi(argv[i]);
      change_header = true;
		}
    else if (i == argc - 1 && file_name == 0)
    {
      file_name = argv[i];
    }
    else
    {
      fprintf(stderr,"cannot understand argument '%s'\n", argv[i]);
      usage();
    }
  }

  FILE* file;
  if (file_name)
  {
    if (strstr(file_name, ".gz"))
    {
#ifdef _WIN32
      file = fopenGzipped(file_name, "rb");
#else
      fprintf(stderr, "ERROR: no support for gzipped input\n");
      byebye(argc==1);
#endif
    }
    else
    {
      file = fopen(file_name, "rb");
    }
    if (file == 0)
    {
      fprintf (stderr, "ERROR: could not open file '%s'\n", file_name);
      byebye(argc==1);
    }
  }
  else if (ilas)
  {
    file = stdin;
  }
  else
  {
    fprintf (stderr, "ERROR: no input specified\n");
    usage();
  }

  if (auto_date_creation && file_name)
  {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attr;
	  SYSTEMTIME creation;
    GetFileAttributesEx(file_name, GetFileExInfoStandard, &attr);
	  FileTimeToSystemTime(&attr.ftCreationTime, &creation);
    int startday[13] = {-1, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    file_creation_day = startday[creation.wMonth] + creation.wDay;
    file_creation_year = creation.wYear;
    change_header = true;
#endif
  }

  if (file_name_out)
  {
    file_out = fopen(file_name_out, "w");
    if (file_out == 0)
    {
      fprintf (stderr, "ERROR: could not open file '%s'\n", file_name_out);
      file_out = stderr;
    }
  }

  LASreader* lasreader = new LASreader();
  if (lasreader->open(file, skip_all_headers) == false)
  {
    fprintf(stderr, "ERROR: lasreader open failed for '%s'\n", file_name);
  }

  LASheader* header = &(lasreader->header);

  // print header info

  char printstring[256];

  if (file_out) fprintf(file_out, "reporting all LAS header entries:\n");
  if (file_out) fprintf(file_out, "  file signature:            '%.4s'\n", header->file_signature);
  if (file_out) fprintf(file_out, "  file source ID:            %d\n", header->file_source_id);
  if (file_out) fprintf(file_out, "  reserved (global_encoding):%d\n", header->global_encoding);
  if (file_out) fprintf(file_out, "  project ID GUID data 1-4:  %d %d %d '%s'\n", header->project_ID_GUID_data_1, header->project_ID_GUID_data_2, header->project_ID_GUID_data_3, header->project_ID_GUID_data_4);
  if (file_out) fprintf(file_out, "  version major.minor:       %d.%d\n", header->version_major, header->version_minor);
  if (file_out) fprintf(file_out, "  system_identifier:         '%s'\n", header->system_identifier);
  if (file_out) fprintf(file_out, "  generating_software:       '%s'\n", header->generating_software);
  if (file_out) fprintf(file_out, "  file creation day/year:    %d/%d\n", header->file_creation_day, header->file_creation_year);
  if (file_out) fprintf(file_out, "  header size                %d\n", header->header_size);
  if (file_out) fprintf(file_out, "  offset to point data       %d\n", header->offset_to_point_data);
  if (file_out) fprintf(file_out, "  number var. length records %d\n", header->number_of_variable_length_records);
  if (file_out) fprintf(file_out, "  point data format          %d\n", header->point_data_format);
  if (file_out) fprintf(file_out, "  point data record length   %d\n", header->point_data_record_length);
  if (file_out) fprintf(file_out, "  number of point records    %d\n", header->number_of_point_records);
  if (file_out) fprintf(file_out, "  number of points by return %d %d %d %d %d\n", header->number_of_points_by_return[0], header->number_of_points_by_return[1], header->number_of_points_by_return[2], header->number_of_points_by_return[3], header->number_of_points_by_return[4]);
  if (file_out) fprintf(file_out, "  scale factor x y z         "); lidardouble2string(printstring, header->x_scale_factor, header->y_scale_factor, header->z_scale_factor, true); if (file_out) fprintf(file_out, printstring);
  if (file_out) fprintf(file_out, "  offset x y z               "); lidardouble2string(printstring, header->x_offset, header->y_offset, header->z_offset, true); if (file_out) fprintf(file_out, printstring);
  if (file_out) fprintf(file_out, "  min x y z                  "); lidardouble2string(printstring, header->min_x, header->min_y, header->min_z, true); if (file_out) fprintf(file_out, printstring);
  if (file_out) fprintf(file_out, "  max x y z                  "); lidardouble2string(printstring, header->max_x, header->max_y, header->max_z, true); if (file_out) fprintf(file_out, printstring);
  if (file_out) if (header->user_data_in_header_size) fprintf(file_out, "the header contains %d user-defined bytes\n", header->user_data_in_header_size);


  // special for batch processing

  if (say_peep)
  {
    fprintf(stderr, "%s '%s' with %d points\n", (repair_header ? "repairing" : "reading"), (file_name ? file_name : "stdin"), header->number_of_point_records);
  }

  // maybe print variable header

  if (parse_variable_header  && !skip_all_headers)
  {
    for (int i = 0; i < (int)header->number_of_variable_length_records; i++)
    {
      if (file_out) fprintf(file_out, "variable length header record %d of %d:\n", i+1, (int)header->number_of_variable_length_records);
      if (file_out) fprintf(file_out, "  reserved             %d\n", lasreader->header.vlrs[i].reserved);
      if (file_out) fprintf(file_out, "  user ID              '%s'\n", lasreader->header.vlrs[i].user_id);
      if (file_out) fprintf(file_out, "  record ID            %d\n", lasreader->header.vlrs[i].record_id);
      if (file_out) fprintf(file_out, "  length after header  %d\n", lasreader->header.vlrs[i].record_length_after_header);
      if (file_out) fprintf(file_out, "  description          '%s'\n", lasreader->header.vlrs[i].description);

      // special handling for known variable header tags

      if (strcmp(header->vlrs[i].user_id, "LASF_Projection") == 0)
      {
        if (header->vlrs[i].record_id == 34735) // GeoKeyDirectoryTag
        {
          if (file_out) fprintf(file_out, "    GeoKeyDirectoryTag version %d.%d.%d number of keys %d\n", header->vlr_geo_keys->key_directory_version, header->vlr_geo_keys->key_revision, header->vlr_geo_keys->minor_revision, header->vlr_geo_keys->number_of_keys);
          for (int j = 0; j < header->vlr_geo_keys->number_of_keys; j++)
          {
            if (file_out)
            {
              fprintf(file_out, "      key %d tiff_tag_location %d count %d value_offset %d - ", header->vlr_geo_key_entries[j].key_id, header->vlr_geo_key_entries[j].tiff_tag_location, header->vlr_geo_key_entries[j].count, header->vlr_geo_key_entries[j].value_offset);
              switch (lasreader->header.vlr_geo_key_entries[j].key_id)
              {
              case 1024: // GTModelTypeGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 1: // ModelTypeProjected   
                  fprintf(file_out, "GTModelTypeGeoKey: ModelTypeProjected\n");
                  break;
                case 2:
                  fprintf(file_out, "GTModelTypeGeoKey: ModelTypeGeographic\n");
                  break;
                case 3:
                  fprintf(file_out, "GTModelTypeGeoKey: ModelTypeGeocentric\n");
                  break;
                default:
                  fprintf(file_out, "GTModelTypeGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
               }
                break;
              case 1025: // GTRasterTypeGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 1: // RasterPixelIsArea   
                  fprintf(file_out, "GTRasterTypeGeoKey: RasterPixelIsArea\n");
                  break;
                case 2: // RasterPixelIsPoint
                  fprintf(file_out, "GTRasterTypeGeoKey: RasterPixelIsPoint\n");
                  break;
               default:
                  fprintf(file_out, "GTRasterTypeGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 1026: // GTCitationGeoKey
                if (lasreader->header.vlr_geo_ascii_params)
                {
                  char dummy[256];
                  strncpy(dummy, &(lasreader->header.vlr_geo_ascii_params[lasreader->header.vlr_geo_key_entries[j].value_offset]), lasreader->header.vlr_geo_key_entries[j].count);
                  dummy[lasreader->header.vlr_geo_key_entries[j].count-1] = '\0';
                  fprintf(file_out, "GTCitationGeoKey: %s\n",dummy);
                }
                break;
              case 2048: // GeographicTypeGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 4001: // GCSE_Airy1830
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Airy1830\n");
                  break;
                case 4002: // GCSE_AiryModified1849 
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_AiryModified1849\n");
                  break;
                case 4003: // GCSE_AustralianNationalSpheroid
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_AustralianNationalSpheroid\n");
                  break;
                case 4004: // GCSE_Bessel1841
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Bessel1841\n");
                  break;
                case 4005: // GCSE_Bessel1841Modified
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Bessel1841Modified\n");
                  break;
                case 4006: // GCSE_BesselNamibia
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_BesselNamibia\n");
                  break;
                case 4008: // GCSE_Clarke1866
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1866\n");
                  break;
                case 4009: // GCSE_Clarke1866Michigan
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1866Michigan\n");
                  break;
                case 4010: // GCSE_Clarke1880_Benoit
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1880_Benoit\n");
                  break;
                case 4011: // GCSE_Clarke1880_IGN
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1880_IGN\n");
                  break;
                case 4012: // GCSE_Clarke1880_RGS
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1880_RGS\n");
                  break;
                case 4013: // GCSE_Clarke1880_Arc
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1880_Arc\n");
                  break;
                case 4014: // GCSE_Clarke1880_SGA1922
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1880_SGA1922\n");
                  break;
                case 4015: // GCSE_Everest1830_1937Adjustment
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Everest1830_1937Adjustment\n");
                  break;
                case 4016: // GCSE_Everest1830_1967Definition
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Everest1830_1967Definition\n");
                  break;
                case 4017: // GCSE_Everest1830_1975Definition
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Everest1830_1975Definition\n");
                  break;
                case 4018: // GCSE_Everest1830Modified
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Everest1830Modified\n");
                  break;
                case 4019: // GCSE_GRS1980
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_GRS1980\n");
                  break;
                case 4020: // GCSE_Helmert1906
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Helmert1906\n");
                  break;
                case 4022: // GCSE_International1924
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_International1924\n");
                  break;
                case 4023: // GCSE_International1967
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_International1967\n");
                  break;
                case 4024: // GCSE_Krassowsky1940
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Krassowsky1940\n");
                  break;
                case 4030: // GCSE_WGS84
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_WGS84\n");
                  break;
                case 4034: // GCSE_Clarke1880
                  fprintf(file_out, "GeographicTypeGeoKey: GCSE_Clarke1880\n");
                  break;
                case 4267: // GCS_NAD27
                  fprintf(file_out, "GeographicTypeGeoKey: GCS_NAD27\n");
                  break;
                case 4269: // GCS_NAD83
                  fprintf(file_out, "GeographicTypeGeoKey: GCS_NAD83\n");
                  break;
                case 4322: // GCS_WGS_72
                  fprintf(file_out, "GeographicTypeGeoKey: GCS_WGS_72\n");
                  break;
                case 4326: // GCS_WGS_84
                  fprintf(file_out, "GeographicTypeGeoKey: GCS_WGS_84\n");
                  break;
                default:
                  fprintf(file_out, "GeographicTypeGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 2049: // GeogCitationGeoKey
                if (lasreader->header.vlr_geo_ascii_params)
                {
                  char dummy[256];
                  strncpy(dummy, &(lasreader->header.vlr_geo_ascii_params[lasreader->header.vlr_geo_key_entries[j].value_offset]), lasreader->header.vlr_geo_key_entries[j].count);
                  dummy[lasreader->header.vlr_geo_key_entries[j].count-1] = '\0';
                  fprintf(file_out, "GeogCitationGeoKey: %s\n",dummy);
                }
                break;
              case 2050: // GeogGeodeticDatumGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 6202: // Datum_Australian_Geodetic_Datum_1966
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: Datum_Australian_Geodetic_Datum_1966\n");
                  break;
                case 6203: // Datum_Australian_Geodetic_Datum_1984
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: Datum_Australian_Geodetic_Datum_1984\n");
                  break;
                case 6267: // Datum_North_American_Datum_1927
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: Datum_North_American_Datum_1927\n");
                  break;
                case 6269: // Datum_North_American_Datum_1983
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: Datum_North_American_Datum_1983\n");
                  break;
                case 6322: // Datum_WGS72
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: Datum_WGS72\n");
                  break;
                case 6326: // Datum_WGS84
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: Datum_WGS84\n");
                  break;
                case 6001: // DatumE_Airy1830
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Airy1830\n");
                  break;
                case 6002: // DatumE_AiryModified1849
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_AiryModified1849\n");
                  break;
                case 6003: // DatumE_AustralianNationalSpheroid
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_AustralianNationalSpheroid\n");
                  break;
                case 6004: // DatumE_Bessel1841
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Bessel1841\n");
                  break;
                case 6005: // DatumE_BesselModified
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_BesselModified\n");
                  break;
                case 6006: // DatumE_BesselNamibia
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_BesselNamibia\n");
                  break;
                case 6008: // DatumE_Clarke1866
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1866\n");
                  break;
                case 6009: // DatumE_Clarke1866Michigan
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1866Michigan\n");
                  break;
                case 6010: // DatumE_Clarke1880_Benoit
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1880_Benoit\n");
                  break;
                case 6011: // DatumE_Clarke1880_IGN
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1880_IGN\n");
                  break;
                case 6012: // DatumE_Clarke1880_RGS
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1880_RGS\n");
                  break;
                case 6013: // DatumE_Clarke1880_Arc
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1880_Arc\n");
                  break;
                case 6014: // DatumE_Clarke1880_SGA1922
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1880_SGA1922\n");
                  break;
                case 6015: // DatumE_Everest1830_1937Adjustment
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Everest1830_1937Adjustment\n");
                  break;
                case 6016: // DatumE_Everest1830_1967Definition
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Everest1830_1967Definition\n");
                  break;
                case 6017: // DatumE_Everest1830_1975Definition
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Everest1830_1975Definition\n");
                  break;
                case 6018: // DatumE_Everest1830Modified
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Everest1830Modified\n");
                  break;
                case 6019: // DatumE_GRS1980
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_GRS1980\n");
                  break;
                case 6020: // DatumE_Helmert1906
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Helmert1906\n");
                  break;
                case 6022: // DatumE_International1924
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_International1924\n");
                  break;
                case 6023: // DatumE_International1967
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_International1967\n");
                  break;
                case 6024: // DatumE_Krassowsky1940
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Krassowsky1940\n");
                  break;
                case 6030: // DatumE_WGS84
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_WGS84\n");
                  break;
                case 6034: // DatumE_Clarke1880
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: DatumE_Clarke1880\n");
                  break;
                default:
                  fprintf(file_out, "GeogGeodeticDatumGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 2051: // GeogPrimeMeridianGeoKey
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 8901: // PM_Greenwich
                  fprintf(file_out, "GeogPrimeMeridianGeoKey: PM_Greenwich\n");
                  break;
                case 8902: // PM_Lisbon
                  fprintf(file_out, "GeogPrimeMeridianGeoKey: PM_Lisbon\n");
                  break;
                default:
                  fprintf(file_out, "GeogPrimeMeridianGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 2052: // GeogLinearUnitsGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 9001: // Linear_Meter
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Meter\n");
                  break;
                case 9002: // Linear_Foot
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Foot\n");
                  break;
                case 9003: // Linear_Foot_US_Survey
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Foot_US_Survey\n");
                  break;
                case 9004: // Linear_Foot_Modified_American
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Foot_Modified_American\n");
                  break;
                case 9005: // Linear_Foot_Clarke
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Foot_Clarke\n");
                  break;
                case 9006: // Linear_Foot_Indian
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Foot_Indian\n");
                  break;
                case 9007: // Linear_Link
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Link\n");
                  break;
                case 9008: // Linear_Link_Benoit
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Link_Benoit\n");
                  break;
                case 9009: // Linear_Link_Sears
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Link_Sears\n");
                  break;
                case 9010: // Linear_Chain_Benoit
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Chain_Benoit\n");
                  break;
                case 9011: // Linear_Chain_Sears
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Chain_Sears\n");
                  break;
                case 9012: // Linear_Yard_Sears
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Yard_Sears\n");
                  break;
                case 9013: // Linear_Yard_Indian
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Yard_Indian\n");
                  break;
                case 9014: // Linear_Fathom
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Fathom\n");
                  break;
                case 9015: // Linear_Mile_International_Nautical
                  fprintf(file_out, "GeogLinearUnitsGeoKey: Linear_Mile_International_Nautical\n");
                  break;
                default:
                  fprintf(file_out, "GeogLinearUnitsGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 2053: // GeogLinearUnitSizeGeoKey  
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "GeogLinearUnitSizeGeoKey: %g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 2054: // GeogAngularUnitsGeoKey
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 9101: // Angular_Radian
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_Radian\n");
                  break;
                case 9102: // Angular_Degree
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_Degree\n");
                  break;
                case 9103: // Angular_Arc_Minute
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_Arc_Minute\n");
                  break;
                case 9104: // Angular_Arc_Second
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_Arc_Second\n");
                  break;
                case 9105: // Angular_Grad
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_Grad\n");
                  break;
                case 9106: // Angular_Gon
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_Gon\n");
                  break;
                case 9107: // Angular_DMS
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_DMS\n");
                  break;
                case 9108: // Angular_DMS_Hemisphere
                  fprintf(file_out, "GeogAngularUnitsGeoKey: Angular_DMS_Hemisphere\n");
                  break;
                default:
                  fprintf(file_out, "GeogAngularUnitsGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 2055: // GeogAngularUnitSizeGeoKey 
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "GeogAngularUnitSizeGeoKey: %g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 2056: // GeogEllipsoidGeoKey
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 7001: // Ellipse_Airy_1830
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Airy_1830\n");
                  break;
                case 7002: // Ellipse_Airy_Modified_1849
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Airy_Modified_1849\n");
                  break;
                case 7003: // Ellipse_Australian_National_Spheroid
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Australian_National_Spheroid\n");
                  break;
                case 7004: // Ellipse_Bessel_1841
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Bessel_1841\n");
                  break;
                case 7005: // Ellipse_Bessel_Modified
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Bessel_Modified\n");
                  break;
                case 7006: // Ellipse_Bessel_Namibia
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Bessel_Namibia\n");
                  break;
                case 7008: // Ellipse_Clarke_1866
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke_1866\n");
                  break;
                case 7009: // Ellipse_Clarke_1866_Michigan
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke_1866_Michigan\n");
                  break;
                case 7010: // Ellipse_Clarke1880_Benoit
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke1880_Benoit\n");
                  break;
                case 7011: // Ellipse_Clarke1880_IGN
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke1880_IGN\n");
                  break;
                case 7012: // Ellipse_Clarke1880_RGS
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke1880_RGS\n");
                  break;
                case 7013: // Ellipse_Clarke1880_Arc
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke1880_Arc\n");
                  break;
                case 7014: // Ellipse_Clarke1880_SGA1922
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke1880_SGA1922\n");
                  break;
                case 7015: // Ellipse_Everest1830_1937Adjustment
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Everest1830_1937Adjustment\n");
                  break;
                case 7016: // Ellipse_Everest1830_1967Definition
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Everest1830_1967Definition\n");
                  break;
                case 7017: // Ellipse_Everest1830_1975Definition
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Everest1830_1975Definition\n");
                  break;
                case 7018: // Ellipse_Everest1830Modified
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Everest1830Modified\n");
                  break;
                case 7019: // Ellipse_GRS_1980
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_GRS_1980\n");
                  break;
                case 7020: // Ellipse_Helmert1906
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Helmert1906\n");
                  break;
                case 7022: // Ellipse_International1924
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_International1924\n");
                  break;
                case 7023: // Ellipse_International1967
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_International1967\n");
                  break;
                case 7024: // Ellipse_Krassowsky1940
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Krassowsky1940\n");
                  break;
                case 7030: // Ellipse_WGS_84
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_WGS_84\n");
                  break;
                case 7034: // Ellipse_Clarke_1880
                  fprintf(file_out, "GeogEllipsoidGeoKey: Ellipse_Clarke_1880\n");
                  break;
                default:
                  fprintf(file_out, "GeogEllipsoidGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 2057: // GeogSemiMajorAxisGeoKey 
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "GeogSemiMajorAxisGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 2058: // GeogSemiMinorAxisGeoKey 
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "GeogSemiMinorAxisGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 2059: // GeogInvFlatteningGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "GeogInvFlatteningGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 2060: // GeogAzimuthUnitsGeoKey
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 9101: // Angular_Radian
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_Radian\n");
                  break;
                case 9102: // Angular_Degree
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_Degree\n");
                  break;
                case 9103: // Angular_Arc_Minute
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_Arc_Minute\n");
                  break;
                case 9104: // Angular_Arc_Second
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_Arc_Second\n");
                  break;
                case 9105: // Angular_Grad
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_Grad\n");
                  break;
                case 9106: // Angular_Gon
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_Gon\n");
                  break;
                case 9107: // Angular_DMS
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_DMS\n");
                  break;
                case 9108: // Angular_DMS_Hemisphere
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: Angular_DMS_Hemisphere\n");
                  break;
                default:
                  fprintf(file_out, "GeogAzimuthUnitsGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 2061: // GeogPrimeMeridianLongGeoKey  
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "GeogPrimeMeridianLongGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3072: // ProjectedCSTypeGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 20137: // PCS_Adindan_UTM_zone_37N
                case 20138: // PCS_Adindan_UTM_zone_38N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Adindan_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-20100);
                  break;
                case 20248: // PCS_AGD66_AMG_zone_48
                case 20249:
                case 20250:
                case 20251:
                case 20252:
                case 20253:
                case 20254:
                case 20255:
                case 20256:
                case 20257:
                case 20258: // PCS_AGD66_AMG_zone_58
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_AGD66_AMG_zone_%d\n", lasreader->header.vlr_geo_key_entries[j].value_offset-20200);
                  break;
                case 20348: // PCS_AGD84_AMG_zone_48
                case 20349:
                case 20350:
                case 20351:
                case 20352:
                case 20353:
                case 20354:
                case 20355:
                case 20356:
                case 20357:
                case 20358: // PCS_AGD84_AMG_zone_58
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_AGD84_AMG_zone_%d\n", lasreader->header.vlr_geo_key_entries[j].value_offset-20300);
                  break;
                case 20437: // PCS_Ain_el_Abd_UTM_zone_37N
                case 20438: // PCS_Ain_el_Abd_UTM_zone_38N
                case 20439: // PCS_Ain_el_Abd_UTM_zone_39N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Ain_el_Abd_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-20400);
                  break;                
                case 20538: // PCS_Afgooye_UTM_zone_38N
                case 20539: // PCS_Afgooye_UTM_zone_39N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Afgooye_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-20500);
                  break;
                case 20822: // PCS_Aratu_UTM_zone_22S
                case 20823: // PCS_Aratu_UTM_zone_23S
                case 20824: // PCS_Aratu_UTM_zone_24S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Aratu_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-20800);
                  break;
                case 21148: // PCS_Batavia_UTM_zone_48S
                case 21149: // PCS_Batavia_UTM_zone_49S
                case 21150: // PCS_Batavia_UTM_zone_50S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Batavia_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-21100);
                  break;
                case 21817: // PCS_Bogota_UTM_zone_17N
                case 21818: // PCS_Bogota_UTM_zone_18N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Bogota_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-21800);
                  break;
                case 22032: // PCS_Camacupa_UTM_32S
                case 22033: // PCS_Camacupa_UTM_33S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Camacupa_UTM_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-22000);
                  break; 
                case 22332: // PCS_Carthage_UTM_zone_32N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Carthage_UTM_zone_32N\n");
                  break; 
                case 22523: // PCS_Corrego_Alegre_UTM_23S
                case 22524: // PCS_Corrego_Alegre_UTM_24S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Corrego_Alegre_UTM_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-22500);
                  break;
                case 22832: // PCS_Douala_UTM_zone_32N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Douala_UTM_zone_32N\n");
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_ED50_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-23000);
                  break;
                case 23239: // PCS_Fahud_UTM_zone_39N
                case 23240: // PCS_Fahud_UTM_zone_40N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Fahud_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-23200);
                  break;
                case 23433: // PCS_Garoua_UTM_zone_33N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Garoua_UTM_zone_33N\n");
                  break;
                case 23846: // PCS_ID74_UTM_zone_46N
                case 23847: // PCS_ID74_UTM_zone_47N
                case 23848:
                case 23849:
                case 23850:
                case 23851:
                case 23852:
                case 23853: // PCS_ID74_UTM_zone_53N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_ID74_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-23800);
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_ID74_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-23840);
                  break;
                case 23947: // PCS_Indian_1954_UTM_47N
                case 23948: // PCS_Indian_1954_UTM_48N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Indian_1954_UTM_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-23900);
                  break;
                case 24047: // PCS_Indian_1975_UTM_47N
                case 24048: // PCS_Indian_1975_UTM_48N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Indian_1975_UTM_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-24000);
                  break;
                case 24547: // PCS_Kertau_UTM_zone_47N
                case 24548: // PCS_Kertau_UTM_zone_48N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Kertau_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-24500);
                  break;
                case 24720: // PCS_La_Canoa_UTM_zone_20N
                case 24721: // PCS_La_Canoa_UTM_zone_21N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_La_Canoa_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-24700);
                  break;
                case 24818: // PCS_PSAD56_UTM_zone_18N
                case 24819: // PCS_PSAD56_UTM_zone_19N
                case 24820: // PCS_PSAD56_UTM_zone_20N
                case 24821: // PCS_PSAD56_UTM_zone_21N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_PSAD56_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-24800);
                  break;
                case 24877: // PCS_PSAD56_UTM_zone_17S
                case 24878: // PCS_PSAD56_UTM_zone_18S
                case 24879: // PCS_PSAD56_UTM_zone_19S
                case 24880: // PCS_PSAD56_UTM_zone_20S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_PSAD56_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-24860);
                  break;
                case 25231: // PCS_Lome_UTM_zone_31N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Lome_UTM_zone_31N\n");
                  break;
                case 25932: // PCS_Malongo_1987_UTM_32S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Malongo_1987_UTM_32S\n");
                  break;
                case 26237: // PCS_Massawa_UTM_zone_37N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Massawa_UTM_zone_37N\n");
                  break;
                case 26331: // PCS_Minna_UTM_zone_31N
                case 26332: // PCS_Minna_UTM_zone_32N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Minna_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-26300);
                  break;
                case 26432: // PCS_Mhast_UTM_zone_32S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Mhast_UTM_zone_32S\n");
                  break;
                case 26632: // PCS_M_poraloko_UTM_32N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_M_poraloko_UTM_32N\n");
                  break;
                case 26692: // PCS_Minna_UTM_zone_32S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Minna_UTM_zone_32S\n");
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-26700);
                  break;
                case 26729: // PCS_NAD27_Alabama_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alabama_East\n");
                  break;
                case 26730: // PCS_NAD27_Alabama_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alabama_West\n");
                  break;
                case 26731: // PCS_NAD27_Alaska_zone_1
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_1\n");
                  break;
                case 26732: // PCS_NAD27_Alaska_zone_2
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_2\n");
                  break;
                case 26733: // PCS_NAD27_Alaska_zone_3
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_3\n");
                  break;
                case 26734: // PCS_NAD27_Alaska_zone_4
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_4\n");
                  break;
                case 26735: // PCS_NAD27_Alaska_zone_5
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_5\n");
                  break;
                case 26736: // PCS_NAD27_Alaska_zone_6
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_6\n");
                  break;
                case 26737: // PCS_NAD27_Alaska_zone_7
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_7\n");
                  break;
                case 26738: // PCS_NAD27_Alaska_zone_8
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_8\n");
                  break;
                case 26739: // PCS_NAD27_Alaska_zone_9
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_9\n");
                  break;
                case 26740: // PCS_NAD27_Alaska_zone_10
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Alaska_zone_10\n");
                  break;
                case 26741: // PCS_NAD27_California_I
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_California_I\n");
                  break;
                case 26742: // PCS_NAD27_California_II
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_California_II\n");
                  break;
                case 26743: // PCS_NAD27_California_III
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_California_III\n");
                  break;
                case 26744: // PCS_NAD27_California_IV
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_California_IV\n");
                  break;
                case 26745: // PCS_NAD27_California_V
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_California_V\n");
                  break;
                case 26746: // PCS_NAD27_California_VI
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_California_VI\n");
                  break;
                case 26747: // PCS_NAD27_California_VII
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_California_VII\n");
                  break;
                case 26748: // PCS_NAD27_Arizona_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Arizona_East\n");
                  break;
                case 26749: // PCS_NAD27_Arizona_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Arizona_Central\n");
                  break;
                case 26750: // PCS_NAD27_Arizona_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Arizona_West\n");
                  break;
                case 26751: // PCS_NAD27_Arkansas_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Arkansas_North\n");
                  break;
                case 26752: // PCS_NAD27_Arkansas_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Arkansas_South\n");
                  break;
                case 26753: // PCS_NAD27_Colorado_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Colorado_North\n");
                  break;
                case 26754: // PCS_NAD27_Colorado_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Colorado_Central\n");
                  break;
                case 26755: // PCS_NAD27_Colorado_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Colorado_South\n");
                  break;
                case 26756: // PCS_NAD27_Connecticut
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Connecticut\n");
                  break;
                case 26757: // PCS_NAD27_Delaware
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Delaware\n");
                  break;
                case 26758: // PCS_NAD27_Florida_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Florida_East\n");
                  break;
                case 26759: // PCS_NAD27_Florida_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Florida_West\n");
                  break;
                case 26760: // PCS_NAD27_Florida_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Florida_North\n");
                  break;
                case 26761: // PCS_NAD27_Hawaii_zone_1
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Hawaii_zone_1\n");
                  break;
                case 26762: // PCS_NAD27_Hawaii_zone_2
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Hawaii_zone_2\n");
                  break;
                case 26763: // PCS_NAD27_Hawaii_zone_3
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Hawaii_zone_3\n");
                  break;
                case 26764: // PCS_NAD27_Hawaii_zone_4
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Hawaii_zone_4\n");
                  break;
                case 26765: // PCS_NAD27_Hawaii_zone_5
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Hawaii_zone_5\n");
                  break;
                case 26766: // PCS_NAD27_Georgia_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Georgia_East\n");
                  break;
                case 26767: // PCS_NAD27_Georgia_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Georgia_West\n");
                  break;
                case 26768: // PCS_NAD27_Idaho_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Idaho_East\n");
                  break;
                case 26769: // PCS_NAD27_Idaho_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Idaho_Central\n");
                  break;
                case 26770: // PCS_NAD27_Idaho_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Idaho_West\n");
                  break;
                case 26771: // PCS_NAD27_Illinois_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Illinois_East\n");
                  break;
                case 26772: // PCS_NAD27_Illinois_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Illinois_West\n");
                  break;
                case 26773: // PCS_NAD27_Indiana_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Indiana_East\n");
                  break;
                case 26774: // PCS_NAD27_Indiana_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Indiana_West\n");
                  break;
                case 26775: // PCS_NAD27_Iowa_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Iowa_North\n");
                  break;
                case 26776: // PCS_NAD27_Iowa_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Iowa_South\n");
                  break;
                case 26777: // PCS_NAD27_Kansas_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Kansas_North\n");
                  break;
                case 26778: // PCS_NAD27_Kansas_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Kansas_South\n");
                  break;
                case 26779: // PCS_NAD27_Kentucky_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Kentucky_North\n");
                  break;
                case 26780: // PCS_NAD27_Kentucky_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Kentucky_South\n");
                  break;
                case 26781: // PCS_NAD27_Louisiana_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Louisiana_North\n");
                  break;
                case 26782: // PCS_NAD27_Louisiana_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Louisiana_South\n");
                  break;
                case 26783: // PCS_NAD27_Maine_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Maine_East\n");
                  break;
                case 26784: // PCS_NAD27_Maine_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Maine_West\n");
                  break;
                case 26785: // PCS_NAD27_Maryland
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Maryland\n");
                  break;
                case 26786: // PCS_NAD27_Massachusetts
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Massachusetts\n");
                  break;
                case 26787: // PCS_NAD27_Massachusetts_Is
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Massachusetts_Is\n");
                  break;
                case 26788: // PCS_NAD27_Michigan_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Michigan_North\n");
                  break;
                case 26789: // PCS_NAD27_Michigan_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Michigan_Central\n");
                  break;
                case 26790: // PCS_NAD27_Michigan_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Michigan_South\n");
                  break;
                case 26791: // PCS_NAD27_Minnesota_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Minnesota_North\n");
                  break;
                case 26792: // PCS_NAD27_Minnesota_Cent
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Minnesota_Cent\n");
                  break;
                case 26793: // PCS_NAD27_Minnesota_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Minnesota_South\n");
                  break;
                case 26794: // PCS_NAD27_Mississippi_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Mississippi_East\n");
                  break;
                case 26795: // PCS_NAD27_Mississippi_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Mississippi_West\n");
                  break;
                case 26796: // PCS_NAD27_Missouri_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Missouri_East\n");
                  break;
                case 26797: // PCS_NAD27_Missouri_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Missouri_Central\n");
                  break;
                case 26798: // PCS_NAD27_Missouri_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Missouri_West\n");
                  break;
                case 26801: // PCS_NAD_Michigan_Michigan_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD_Michigan_Michigan_East\n");
                  break;
                case 26802: // PCS_NAD_Michigan_Michigan_Old_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD_Michigan_Michigan_Old_Central\n");
                  break;
                case 26803: // PCS_NAD_Michigan_Michigan_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD_Michigan_Michigan_West\n");
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-26900);
                  break;
                case 26929: // PCS_NAD83_Alabama_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alabama_East\n");
                  break;
                case 26930: // PCS_NAD83_Alabama_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alabama_West\n");
                  break;
                case 26931: // PCS_NAD83_Alaska_zone_1
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_1\n");
                  break;
                case 26932: // PCS_NAD83_Alaska_zone_2
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_2\n");
                  break;
                case 26933: // PCS_NAD83_Alaska_zone_3
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_3\n");
                  break;
                case 26934: // PCS_NAD83_Alaska_zone_4
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_4\n");
                  break;
                case 26935: // PCS_NAD83_Alaska_zone_5
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_5\n");
                  break;
                case 26936: // PCS_NAD83_Alaska_zone_6
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_6\n");
                  break;
                case 26937: // PCS_NAD83_Alaska_zone_7
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_7\n");
                  break;
                case 26938: // PCS_NAD83_Alaska_zone_8
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_8\n");
                  break;
                case 26939: // PCS_NAD83_Alaska_zone_9
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_9\n");
                  break;
                case 26940: // PCS_NAD83_Alaska_zone_10
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Alaska_zone_10\n");
                  break;
                case 26941: // PCS_NAD83_California_I
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_California_I\n");
                  break;
                case 26942: // PCS_NAD83_California_II
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_California_II\n");
                  break;
                case 26943: // PCS_NAD83_California_III
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_California_III\n");
                  break;
                case 26944: // PCS_NAD83_California_IV
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_California_IV\n");
                  break;
                case 26945: // PCS_NAD83_California_V
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_California_V\n");
                  break;
                case 26946: // PCS_NAD83_California_VI
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_California_VI\n");
                  break;
                case 26947: // PCS_NAD83_California_VII
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_California_VII\n");
                  break;
                case 26948: // PCS_NAD83_Arizona_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Arizona_East\n");
                  break;
                case 26949: // PCS_NAD83_Arizona_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Arizona_Central\n");
                  break;
                case 26950: // PCS_NAD83_Arizona_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Arizona_West\n");
                  break;
                case 26951: // PCS_NAD83_Arkansas_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Arkansas_North\n");
                  break;
                case 26952: // PCS_NAD83_Arkansas_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Arkansas_South\n");
                  break;
                case 26953: // PCS_NAD83_Colorado_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Colorado_North\n");
                  break;
                case 26954: // PCS_NAD83_Colorado_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Colorado_Central\n");
                  break;
                case 26955: // PCS_NAD83_Colorado_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Colorado_South\n");
                  break;
                case 26956: // PCS_NAD83_Connecticut
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Connecticut\n");
                  break;
                case 26957: // PCS_NAD83_Delaware
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Delaware\n");
                  break;
                case 26958: // PCS_NAD83_Florida_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Florida_East\n");
                  break;
                case 26959: // PCS_NAD83_Florida_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Florida_West\n");
                  break;
                case 26960: // PCS_NAD83_Florida_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Florida_North\n");
                  break;
                case 26961: // PCS_NAD83_Hawaii_zone_1
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Hawaii_zone_1\n");
                  break;
                case 26962: // PCS_NAD83_Hawaii_zone_2
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Hawaii_zone_2\n");
                  break;
                case 26963: // PCS_NAD83_Hawaii_zone_3
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Hawaii_zone_3\n");
                  break;
                case 26964: // PCS_NAD83_Hawaii_zone_4
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Hawaii_zone_4\n");
                  break;
                case 26965: // PCS_NAD83_Hawaii_zone_5
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Hawaii_zone_5\n");
                  break;
                case 26966: // PCS_NAD83_Georgia_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Georgia_East\n");
                  break;
                case 26967: // PCS_NAD83_Georgia_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Georgia_West\n");
                  break;
                case 26968: // PCS_NAD83_Idaho_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Idaho_East\n");
                  break;
                case 26969: // PCS_NAD83_Idaho_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Idaho_Central\n");
                  break;
                case 26970: // PCS_NAD83_Idaho_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Idaho_West\n");
                  break;
                case 26971: // PCS_NAD83_Illinois_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Illinois_East\n");
                  break;
                case 26972: // PCS_NAD83_Illinois_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Illinois_West\n");
                  break;
                case 26973: // PCS_NAD83_Indiana_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Indiana_East\n");
                  break;
                case 26974: // PCS_NAD83_Indiana_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Indiana_West\n");
                  break;
                case 26975: // PCS_NAD83_Iowa_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Iowa_North\n");
                  break;
                case 26976: // PCS_NAD83_Iowa_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Iowa_South\n");
                  break;
                case 26977: // PCS_NAD83_Kansas_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Kansas_North\n");
                  break;
                case 26978: // PCS_NAD83_Kansas_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Kansas_South\n");
                  break;
                case 26979: // PCS_NAD83_Kentucky_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Kentucky_North\n");
                  break;
                case 26980: // PCS_NAD83_Kentucky_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Kentucky_South\n");
                  break;
                case 26981: // PCS_NAD83_Louisiana_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Louisiana_North\n");
                  break;
                case 26982: // PCS_NAD83_Louisiana_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Louisiana_South\n");
                  break;
                case 26983: // PCS_NAD83_Maine_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Maine_East\n");
                  break;
                case 26984: // PCS_NAD83_Maine_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Maine_West\n");
                  break;
                case 26985: // PCS_NAD83_Maryland
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Maryland\n");
                  break;
                case 26986: // PCS_NAD83_Massachusetts
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Massachusetts\n");
                  break;
                case 26987: // PCS_NAD83_Massachusetts_Is
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Massachusetts_Is\n");
                  break;
                case 26988: // PCS_NAD83_Michigan_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Michigan_North\n");
                  break;
                case 26989: // PCS_NAD83_Michigan_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Michigan_Central\n");
                  break;
                case 26990: // PCS_NAD83_Michigan_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Michigan_South\n");
                  break;
                case 26991: // PCS_NAD83_Minnesota_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Minnesota_North\n");
                  break;
                case 26992: // PCS_NAD83_Minnesota_Cent
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Minnesota_Cent\n");
                  break;
                case 26993: // PCS_NAD83_Minnesota_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Minnesota_South\n");
                  break;
                case 26994: // PCS_NAD83_Mississippi_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Mississippi_East\n");
                  break;
                case 26995: // PCS_NAD83_Mississippi_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Mississippi_West\n");
                  break;
                case 26996: // PCS_NAD83_Missouri_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Missouri_East\n");
                  break;
                case 26997: // PCS_NAD83_Missouri_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Missouri_Central\n");
                  break;
                case 26998: // PCS_NAD83_Missouri_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Missouri_West\n");
                  break;
                case 29118: // PCS_SAD69_UTM_zone_18N
                case 29119: // PCS_SAD69_UTM_zone_19N
                case 29120: // PCS_SAD69_UTM_zone_20N
                case 29121: // PCS_SAD69_UTM_zone_21N
                case 29122: // PCS_SAD69_UTM_zone_22N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_SAD69_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-29100);
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_SAD69_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-29160);
                  break;
                case 29220: // PCS_Sapper_Hill_UTM_20S
                case 29221: // PCS_Sapper_Hill_UTM_21S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Sapper_Hill_UTM_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-29200);
                  break;
                case 29333: // PCS_Schwarzeck_UTM_33S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Schwarzeck_UTM_33S\n");
                  break;
                case 29635: // PCS_Sudan_UTM_zone_35N
                case 29636: // PCS_Sudan_UTM_zone_35N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Sudan_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-29600);
                  break;
                case 29738: // PCS_Tananarive_UTM_38S
                case 29739: // PCS_Tananarive_UTM_39S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Tananarive_UTM_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-29700);
                  break;
                case 29849: // PCS_Timbalai_1948_UTM_49N
                case 29850: // PCS_Timbalai_1948_UTM_50N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Timbalai_1948_UTM_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-29800);
                  break;
                case 30339: // PCS_TC_1948_UTM_zone_39N
                case 30340: // PCS_TC_1948_UTM_zone_40N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_TC_1948_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-30300);
                  break;
                case 30729: // PCS_Nord_Sahara_UTM_29N
                case 30730: // PCS_Nord_Sahara_UTM_30N
                case 30731: // PCS_Nord_Sahara_UTM_31N
                case 30732: // PCS_Nord_Sahara_UTM_32N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Nord_Sahara_UTM_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-30700);
                  break;
                case 31028: // PCS_Yoff_UTM_zone_28N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Yoff_UTM_zone_28N\n");
                  break;
                case 31121: // PCS_Zanderij_UTM_zone_21N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_Zanderij_UTM_zone_21N\n");
                  break;
                case 32001: // PCS_NAD27_Montana_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Montana_North\n");
                  break;
                case 32002: // PCS_NAD27_Montana_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Montana_Central\n");
                  break;
                case 32003: // PCS_NAD27_Montana_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Montana_South\n");
                  break;
                case 32005: // PCS_NAD27_Nebraska_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Nebraska_North\n");
                  break;
                case 32006: // PCS_NAD27_Nebraska_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Nebraska_South\n");
                  break;
                case 32007: // PCS_NAD27_Nevada_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Nevada_East\n");
                  break;
                case 32008: // PCS_NAD27_Nevada_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Nevada_Central\n");
                  break;
                case 32009: // PCS_NAD27_Nevada_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Nevada_West\n");
                  break;
                case 32010: // PCS_NAD27_New_Hampshire
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_Hampshire\n");
                  break;
                case 32011: // PCS_NAD27_New_Jersey
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_Jersey\n");
                  break;
                case 32012: // PCS_NAD27_New_Mexico_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_Mexico_East\n");
                  break;
                case 32013: // PCS_NAD27_New_Mexico_Cent
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_Mexico_Cent\n");
                  break;
                case 32014: // PCS_NAD27_New_Mexico_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_Mexico_West\n");
                  break;
                case 32015: // PCS_NAD27_New_York_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_York_East\n");
                  break;
                case 32016: // PCS_NAD27_New_York_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_York_Central\n");
                  break;
                case 32017: // PCS_NAD27_New_York_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_York_West\n");
                  break;
                case 32018: // PCS_NAD27_New_York_Long_Is
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_New_York_Long_Is\n");
                  break;
                case 32019: // PCS_NAD27_North_Carolina
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_North_Carolina\n");
                  break;
                case 32020: // PCS_NAD27_North_Dakota_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_North_Dakota_N\n");
                  break;
                case 32021: // PCS_NAD27_North_Dakota_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_North_Dakota_S\n");
                  break;
                case 32022: // PCS_NAD27_Ohio_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Ohio_North\n");
                  break;
                case 32023: // PCS_NAD27_Ohio_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Ohio_South\n");
                  break;
                case 32024: // PCS_NAD27_Oklahoma_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Oklahoma_North\n");
                  break;
                case 32025: // PCS_NAD27_Oklahoma_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Oklahoma_South\n");
                  break;
                case 32026: // PCS_NAD27_Oregon_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Oregon_North\n");
                  break;
                case 32027: // PCS_NAD27_Oregon_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Oregon_South\n");
                  break;
                case 32028: // PCS_NAD27_Pennsylvania_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Pennsylvania_N\n");
                  break;
                case 32029: // PCS_NAD27_Pennsylvania_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Pennsylvania_S\n");
                  break;
                case 32030: // PCS_NAD27_Rhode_Island
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Rhode_Island\n");
                  break;
                case 32031: // PCS_NAD27_South_Carolina_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_South_Carolina_N\n");
                  break;
                case 32033: // PCS_NAD27_South_Carolina_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_South_Carolina_S\n");
                  break;
                case 32034: // PCS_NAD27_South_Dakota_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_South_Dakota_N\n");
                  break;
                case 32035: // PCS_NAD27_South_Dakota_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_South_Dakota_S\n");
                  break;
                case 32036: // PCS_NAD27_Tennessee
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Tennessee\n");
                  break;
                case 32037: // PCS_NAD27_Texas_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Texas_North\n");
                  break;
                case 32038: // PCS_NAD27_Texas_North_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Texas_North_Cen\n");
                  break;
                case 32039: // PCS_NAD27_Texas_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Texas_Central\n");
                  break;
                case 32040: // PCS_NAD27_Texas_South_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Texas_South_Cen\n");
                  break;
                case 32041: // PCS_NAD27_Texas_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Texas_South\n");
                  break;
                case 32042: // PCS_NAD27_Utah_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Utah_North\n");
                  break;
                case 32043: // PCS_NAD27_Utah_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Utah_Central\n");
                  break;
                case 32044: // PCS_NAD27_Utah_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Utah_South\n");
                  break;
                case 32045: // PCS_NAD27_Vermont
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Vermont\n");
                  break;
                case 32046: // PCS_NAD27_Virginia_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Virginia_North\n");
                  break;
                case 32047: // PCS_NAD27_Virginia_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Virginia_South\n");
                  break;
                case 32048: // PCS_NAD27_Washington_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Washington_North\n");
                  break;
                case 32049: // PCS_NAD27_Washington_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Washington_South\n");
                  break;
                case 32050: // PCS_NAD27_West_Virginia_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_West_Virginia_N\n");
                  break;
                case 32051: // PCS_NAD27_West_Virginia_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_West_Virginia_S\n");
                  break;
                case 32052: // PCS_NAD27_Wisconsin_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Wisconsin_North\n");
                  break;
                case 32053: // PCS_NAD27_Wisconsin_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Wisconsin_Cen\n");
                  break;
                case 32054: // PCS_NAD27_Wisconsin_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Wisconsin_South\n");
                  break;
                case 32055: // PCS_NAD27_Wyoming_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Wyoming_East\n");
                  break;
                case 32056: // PCS_NAD27_Wyoming_E_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Wyoming_E_Cen\n");
                  break;
                case 32057: // PCS_NAD27_Wyoming_W_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Wyoming_W_Cen\n");
                  break;
                case 32058: // PCS_NAD27_Wyoming_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Wyoming_West\n");
                  break;
                case 32059: // PCS_NAD27_Puerto_Rico
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_Puerto_Rico\n");
                  break;
                case 32060: // PCS_NAD27_St_Croix
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD27_St_Croix\n");
                  break;
                case 32100: // PCS_NAD83_Montana
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Montana\n");
                  break;
                case 32104: // PCS_NAD83_Nebraska
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Nebraska\n");
                  break;
                case 32107: // PCS_NAD83_Nevada_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Nevada_East\n");
                  break;
                case 32108: // PCS_NAD83_Nevada_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Nevada_Central\n");
                  break;
                case 32109: // PCS_NAD83_Nevada_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Nevada_West\n");
                  break;
                case 32110: // PCS_NAD83_New_Hampshire
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_Hampshire\n");
                  break;
                case 32111: // PCS_NAD83_New_Jersey
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_Jersey\n");
                  break;
                case 32112: // PCS_NAD83_New_Mexico_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_Mexico_East\n");
                  break;
                case 32113: // PCS_NAD83_New_Mexico_Cent
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_Mexico_Cent\n");
                  break;
                case 32114: // PCS_NAD83_New_Mexico_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_Mexico_West\n");
                  break;
                case 32115: // PCS_NAD83_New_York_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_York_East\n");
                  break;
                case 32116: // PCS_NAD83_New_York_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_York_Central\n");
                  break;
                case 32117: // PCS_NAD83_New_York_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_York_West\n");
                  break;
                case 32118: // PCS_NAD83_New_York_Long_Is
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_New_York_Long_Is\n");
                  break;
                case 32119: // PCS_NAD83_North_Carolina
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_North_Carolina\n");
                  break;
                case 32120: // PCS_NAD83_North_Dakota_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_North_Dakota_N\n");
                  break;
                case 32121: // PCS_NAD83_North_Dakota_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_North_Dakota_S\n");
                  break;
                case 32122: // PCS_NAD83_Ohio_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Ohio_North\n");
                  break;
                case 32123: // PCS_NAD83_Ohio_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Ohio_South\n");
                  break;
                case 32124: // PCS_NAD83_Oklahoma_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Oklahoma_North\n");
                  break;
                case 32125: // PCS_NAD83_Oklahoma_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Oklahoma_South\n");
                  break;
                case 32126: // PCS_NAD83_Oregon_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Oregon_North\n");
                  break;
                case 32127: // PCS_NAD83_Oregon_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Oregon_South\n");
                  break;
                case 32128: // PCS_NAD83_Pennsylvania_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Pennsylvania_N\n");
                  break;
                case 32129: // PCS_NAD83_Pennsylvania_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Pennsylvania_S\n");
                  break;
                case 32130: // PCS_NAD83_Rhode_Island
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Rhode_Island\n");
                  break;
                case 32133: // PCS_NAD83_South_Carolina
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_South_Carolina\n");
                  break;
                case 32134: // PCS_NAD83_South_Dakota_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_South_Dakota_N\n");
                  break;
                case 32135: // PCS_NAD83_South_Dakota_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_South_Dakota_S\n");
                  break;
                case 32136: // PCS_NAD83_Tennessee
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Tennessee\n");
                  break;
                case 32137: // PCS_NAD83_Texas_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Texas_North\n");
                  break;
                case 32138: // PCS_NAD83_Texas_North_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Texas_North_Cen\n");
                  break;
                case 32139: // PCS_NAD83_Texas_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Texas_Central\n");
                  break;
                case 32140: // PCS_NAD83_Texas_South_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Texas_South_Cen\n");
                  break;
                case 32141: // PCS_NAD83_Texas_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Texas_South\n");
                  break;
                case 32142: // PCS_NAD83_Utah_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Utah_North\n");
                  break;
                case 32143: // PCS_NAD83_Utah_Central
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Utah_Central\n");
                  break;
                case 32144: // PCS_NAD83_Utah_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Utah_South\n");
                  break;
                case 32145: // PCS_NAD83_Vermont
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Vermont\n");
                  break;
                case 32146: // PCS_NAD83_Virginia_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Virginia_North\n");
                  break;
                case 32147: // PCS_NAD83_Virginia_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Virginia_South\n");
                  break;
                case 32148: // PCS_NAD83_Washington_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Washington_North\n");
                  break;
                case 32149: // PCS_NAD83_Washington_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Washington_South\n");
                  break;
                case 32150: // PCS_NAD83_West_Virginia_N
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_West_Virginia_N\n");
                  break;
                case 32151: // PCS_NAD83_West_Virginia_S
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_West_Virginia_S\n");
                  break;
                case 32152: // PCS_NAD83_Wisconsin_North
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Wisconsin_North\n");
                  break;
                case 32153: // PCS_NAD83_Wisconsin_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Wisconsin_Cen\n");
                  break;
                case 32154: // PCS_NAD83_Wisconsin_South
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Wisconsin_South\n");
                  break;
                case 32155: // PCS_NAD83_Wyoming_East
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Wyoming_East\n");
                  break;
                case 32156: // PCS_NAD83_Wyoming_E_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Wyoming_E_Cen\n");
                  break;
                case 32157: // PCS_NAD83_Wyoming_W_Cen
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Wyoming_W_Cen\n");
                  break;
                case 32158: // PCS_NAD83_Wyoming_West
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Wyoming_West\n");
                  break;
                case 32161: // PCS_NAD83_Puerto_Rico_Virgin_Is
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_NAD83_Puerto_Rico_Virgin_Is\n");
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_WGS72_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-32200);
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_WGS72_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-32300);
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_WGS72BE_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-32400);
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_WGS72BE_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-32500);
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_WGS84_UTM_zone_%dN\n", lasreader->header.vlr_geo_key_entries[j].value_offset-32600);
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
                  fprintf(file_out, "ProjectedCSTypeGeoKey: PCS_WGS84_UTM_zone_%dS\n", lasreader->header.vlr_geo_key_entries[j].value_offset-32700);
                  break;
                default:
                  fprintf(file_out, "ProjectedCSTypeGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 3073: // PCSCitationGeoKey
                if (lasreader->header.vlr_geo_ascii_params)
                {
                  char dummy[256];
                  strncpy(dummy, &(lasreader->header.vlr_geo_ascii_params[lasreader->header.vlr_geo_key_entries[j].value_offset]), lasreader->header.vlr_geo_key_entries[j].count);
                  dummy[lasreader->header.vlr_geo_key_entries[j].count-1] = '\0';
                  fprintf(file_out, "PCSCitationGeoKey: %s\n",dummy);
                }
                break;
              case 3074: // ProjectionGeoKey
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 10101: // Proj_Alabama_CS27_East
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alabama_CS27_East\n");
                  break;
                case 10102: // Proj_Alabama_CS27_West
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alabama_CS27_West\n");
                  break;
                case 10131: // Proj_Alabama_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alabama_CS83_East\n");
                  break;
                case 10132: // Proj_Alabama_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alabama_CS83_West\n");
                  break;
                case 10201: // Proj_Arizona_Coordinate_System_east			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arizona_Coordinate_System_east\n");
                  break;
                case 10202: // Proj_Arizona_Coordinate_System_Central		
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arizona_Coordinate_System_Central\n");
                  break;
                case 10203: // Proj_Arizona_Coordinate_System_west			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arizona_Coordinate_System_west\n");
                  break;
                case 10231: // Proj_Arizona_CS83_east				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arizona_CS83_east\n");
                  break;
                case 10232: // Proj_Arizona_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arizona_CS83_Central\n");
                  break;
                case 10233: // Proj_Arizona_CS83_west				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arizona_CS83_west\n");
                  break;
                case 10301: // Proj_Arkansas_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arkansas_CS27_North\n");
                  break;
                case 10302: // Proj_Arkansas_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arkansas_CS27_South\n");
                  break;
                case 10331: // Proj_Arkansas_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arkansas_CS83_North\n");
                  break;
                case 10332: // Proj_Arkansas_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Arkansas_CS83_South\n");
                  break;
                case 10401: // Proj_California_CS27_I				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS27_I\n");
                  break;
                case 10402: // Proj_California_CS27_II				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS27_II\n");
                  break;
                case 10403: // Proj_California_CS27_III				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS27_III\n");
                  break;
                case 10404: // Proj_California_CS27_IV				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS27_IV\n");
                  break;
                case 10405: // Proj_California_CS27_V				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS27_V\n");
                  break;
                case 10406: // Proj_California_CS27_VI				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS27_VI\n");
                  break;
                case 10407: // Proj_California_CS27_VII				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS27_VII\n");
                  break;
                case 10431: // Proj_California_CS83_1				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS83_1\n");
                  break;
                case 10432: // Proj_California_CS83_2				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS83_2\n");
                  break;
                case 10433: // Proj_California_CS83_3				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS83_3\n");
                  break;
                case 10434: // Proj_California_CS83_4				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS83_4\n");
                  break;
                case 10435: // Proj_California_CS83_5				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS83_5\n");
                  break;
                case 10436: // Proj_California_CS83_6				
                  fprintf(file_out, "ProjectionGeoKey: Proj_California_CS83_6\n");
                  break;
                case 10501: // Proj_Colorado_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colorado_CS27_North\n");
                  break;
                case 10502: // Proj_Colorado_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colorado_CS27_Central\n");
                  break;
                case 10503: // Proj_Colorado_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colorado_CS27_South\n");
                  break;
                case 10531: // Proj_Colorado_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colorado_CS83_North\n");
                  break;
                case 10532: // Proj_Colorado_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colorado_CS83_Central\n");
                  break;
                case 10533: // Proj_Colorado_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colorado_CS83_South\n");
                  break;
                case 10600: // Proj_Connecticut_CS27				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Connecticut_CS27\n");
                  break;
                case 10630: // Proj_Connecticut_CS83				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Connecticut_CS83\n");
                  break;
                case 10700: // Proj_Delaware_CS27					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Delaware_CS27\n");
                  break;
                case 10730: // Proj_Delaware_CS83					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Delaware_CS83\n");
                  break;
                case 10901: // Proj_Florida_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Florida_CS27_East\n");
                  break;
                case 10902: // Proj_Florida_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Florida_CS27_West\n");
                  break;
                case 10903: // Proj_Florida_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Florida_CS27_North\n");
                  break;
                case 10931: // Proj_Florida_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Florida_CS83_East\n");
                  break;
                case 10932: // Proj_Florida_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Florida_CS83_West\n");
                  break;
                case 10933: // Proj_Florida_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Florida_CS83_North\n");
                  break;
                case 11001: // Proj_Georgia_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Georgia_CS27_East\n");
                  break;
                case 11002: // Proj_Georgia_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Georgia_CS27_West\n");
                  break;
                case 11031: // Proj_Georgia_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Georgia_CS83_East\n");
                  break;
                case 11032: // Proj_Georgia_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Georgia_CS83_West\n");
                  break;
                case 11101: // Proj_Idaho_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Idaho_CS27_East\n");
                  break;
                case 11102: // Proj_Idaho_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Idaho_CS27_Central\n");
                  break;
                case 11103: // Proj_Idaho_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Idaho_CS27_West\n");
                  break;
                case 11131: // Proj_Idaho_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Idaho_CS83_East\n");
                  break;
                case 11132: // Proj_Idaho_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Idaho_CS83_Central\n");
                  break;
                case 11133: // Proj_Idaho_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Idaho_CS83_West\n");
                  break;
                case 11201: // Proj_Illinois_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Illinois_CS27_East\n");
                  break;
                case 11202: // Proj_Illinois_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Illinois_CS27_West\n");
                  break;
                case 11231: // Proj_Illinois_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Illinois_CS83_East\n");
                  break;
                case 11232: // Proj_Illinois_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Illinois_CS83_West\n");
                  break;
                case 11301: // Proj_Indiana_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Indiana_CS27_East\n");
                  break;
                case 11302: // Proj_Indiana_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Indiana_CS27_West\n");
                  break;
                case 11331: // Proj_Indiana_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Indiana_CS83_East\n");
                  break;
                case 11332: // Proj_Indiana_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Indiana_CS83_West\n");
                  break;
                case 11401: // Proj_Iowa_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Iowa_CS27_North\n");
                  break;
                case 11402: // Proj_Iowa_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Iowa_CS27_South\n");
                  break;
                case 11431: // Proj_Iowa_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Iowa_CS83_North\n");
                  break;
                case 11432: // Proj_Iowa_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Iowa_CS83_South\n");
                  break;
                case 11501: // Proj_Kansas_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kansas_CS27_North\n");
                  break;
                case 11502: // Proj_Kansas_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kansas_CS27_South\n");
                  break;
                case 11531: // Proj_Kansas_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kansas_CS83_North\n");
                  break;
                case 11532: // Proj_Kansas_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kansas_CS83_South\n");
                  break;
                case 11601: // Proj_Kentucky_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kentucky_CS27_North\n");
                  break;
                case 11602: // Proj_Kentucky_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kentucky_CS27_South\n");
                  break;
                case 11631: // Proj_Kentucky_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kentucky_CS83_North\n");
                  break;
                case 11632: // Proj_Kentucky_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Kentucky_CS83_South\n");
                  break;
                case 11701: // Proj_Louisiana_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Louisiana_CS27_North\n");
                  break;
                case 11702: // Proj_Louisiana_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Louisiana_CS27_South\n");
                  break;
                case 11731: // Proj_Louisiana_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Louisiana_CS83_North\n");
                  break;
                case 11732: // Proj_Louisiana_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Louisiana_CS83_South\n");
                  break;
                case 11801: // Proj_Maine_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Maine_CS27_East\n");
                  break;
                case 11802: // Proj_Maine_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Maine_CS27_West\n");
                  break;
                case 11831: // Proj_Maine_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Maine_CS83_East\n");
                  break;
                case 11832: // Proj_Maine_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Maine_CS83_West\n");
                  break;
                case 11900: // Proj_Maryland_CS27					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Maryland_CS27\n");
                  break;
                case 11930: // Proj_Maryland_CS83					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Maryland_CS83\n");
                  break;
                case 12001: // Proj_Massachusetts_CS27_Mainland			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Massachusetts_CS27_Mainland\n");
                  break;
                case 12002: // Proj_Massachusetts_CS27_Island			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Massachusetts_CS27_Island\n");
                  break;
                case 12031: // Proj_Massachusetts_CS83_Mainland			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Massachusetts_CS83_Mainland\n");
                  break;
                case 12032: // Proj_Massachusetts_CS83_Island			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Massachusetts_CS83_Island\n");
                  break;
                case 12101: // Proj_Michigan_State_Plane_East			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_State_Plane_East\n");
                  break;
                case 12102: // Proj_Michigan_State_Plane_Old_Central		
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_State_Plane_Old_Central\n");
                  break;
                case 12103: // Proj_Michigan_State_Plane_West			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_State_Plane_West\n");
                  break;
                case 12111: // Proj_Michigan_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_CS27_North\n");
                  break;
                case 12112: // Proj_Michigan_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_CS27_Central\n");
                  break;
                case 12113: // Proj_Michigan_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_CS27_South\n");
                  break;
                case 12141: // Proj_Michigan_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_CS83_North\n");
                  break;
                case 12142: // Proj_Michigan_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_CS83_Central\n");
                  break;
                case 12143: // Proj_Michigan_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Michigan_CS83_South\n");
                  break;
                case 12201: // Proj_Minnesota_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Minnesota_CS27_North\n");
                  break;
                case 12202: // Proj_Minnesota_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Minnesota_CS27_Central\n");
                  break;
                case 12203: // Proj_Minnesota_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Minnesota_CS27_South\n");
                  break;
                case 12231: // Proj_Minnesota_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Minnesota_CS83_North\n");
                  break;
                case 12232: // Proj_Minnesota_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Minnesota_CS83_Central\n");
                  break;
                case 12233: // Proj_Minnesota_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Minnesota_CS83_South\n");
                  break;
                case 12301: // Proj_Mississippi_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Mississippi_CS27_East\n");
                  break;
                case 12302: // Proj_Mississippi_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Mississippi_CS27_West\n");
                  break;
                case 12331: // Proj_Mississippi_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Mississippi_CS83_East\n");
                  break;
                case 12332: // Proj_Mississippi_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Mississippi_CS83_West\n");
                  break;
                case 12401: // Proj_Missouri_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Missouri_CS27_East\n");
                  break;
                case 12402: // Proj_Missouri_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Missouri_CS27_Central\n");
                  break;
                case 12403: // Proj_Missouri_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Missouri_CS27_West\n");
                  break;
                case 12431: // Proj_Missouri_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Missouri_CS83_East\n");
                  break;
                case 12432: // Proj_Missouri_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Missouri_CS83_Central\n");
                  break;
                case 12433: // Proj_Missouri_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Missouri_CS83_West\n");
                  break;
                case 12501: // Proj_Montana_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Montana_CS27_North\n");
                  break;
                case 12502: // Proj_Montana_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Montana_CS27_Central\n");
                  break;
                case 12503: // Proj_Montana_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Montana_CS27_South\n");
                  break;
                case 12530: // Proj_Montana_CS83					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Montana_CS83\n");
                  break;
                case 12601: // Proj_Nebraska_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nebraska_CS27_North\n");
                  break;
                case 12602: // Proj_Nebraska_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nebraska_CS27_South\n");
                  break;
                case 12630: // Proj_Nebraska_CS83					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nebraska_CS83\n");
                  break;
                case 12701: // Proj_Nevada_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nevada_CS27_East\n");
                  break;
                case 12702: // Proj_Nevada_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nevada_CS27_Central\n");
                  break;
                case 12703: // Proj_Nevada_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nevada_CS27_West\n");
                  break;
                case 12731: // Proj_Nevada_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nevada_CS83_East\n");
                  break;
                case 12732: // Proj_Nevada_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nevada_CS83_Central\n");
                  break;
                case 12733: // Proj_Nevada_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Nevada_CS83_West\n");
                  break;
                case 12800: // Proj_New_Hampshire_CS27				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Hampshire_CS27\n");
                  break;
                case 12830: // Proj_New_Hampshire_CS83				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Hampshire_CS83\n");
                  break;
                case 12900: // Proj_New_Jersey_CS27				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Jersey_CS27\n");
                  break;
                case 12930: // Proj_New_Jersey_CS83				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Jersey_CS83\n");
                  break;
                case 13001: // Proj_New_Mexico_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Mexico_CS27_East\n");
                  break;
                case 13002: // Proj_New_Mexico_CS27_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Mexico_CS27_Central\n");
                  break;
                case 13003: // Proj_New_Mexico_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Mexico_CS27_West\n");
                  break;
                case 13031: // Proj_New_Mexico_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Mexico_CS83_East\n");
                  break;
                case 13032: // Proj_New_Mexico_CS83_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Mexico_CS83_Central\n");
                  break;
                case 13033: // Proj_New_Mexico_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Mexico_CS83_West\n");
                  break;
                case 13101: // Proj_New_York_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS27_East\n");
                  break;
                case 13102: // Proj_New_York_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS27_Central\n");
                  break;
                case 13103: // Proj_New_York_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS27_West\n");
                  break;
                case 13104: // Proj_New_York_CS27_Long_Island			
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS27_Long_Island\n");
                  break;
                case 13131: // Proj_New_York_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS83_East\n");
                  break;
                case 13132: // Proj_New_York_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS83_Central\n");
                  break;
                case 13133: // Proj_New_York_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS83_West\n");
                  break;
                case 13134: // Proj_New_York_CS83_Long_Island			
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_York_CS83_Long_Island\n");
                  break;
                case 13200: // Proj_North_Carolina_CS27				
                  fprintf(file_out, "ProjectionGeoKey: Proj_North_Carolina_CS27\n");
                  break;
                case 13230: // Proj_North_Carolina_CS83				
                  fprintf(file_out, "ProjectionGeoKey: Proj_North_Carolina_CS83\n");
                  break;
                case 13301: // Proj_North_Dakota_CS27_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_North_Dakota_CS27_North\n");
                  break;
                case 13302: // Proj_North_Dakota_CS27_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_North_Dakota_CS27_South\n");
                  break;
                case 13331: // Proj_North_Dakota_CS83_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_North_Dakota_CS83_North\n");
                  break;
                case 13332: // Proj_North_Dakota_CS83_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_North_Dakota_CS83_South\n");
                  break;
                case 13401: // Proj_Ohio_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Ohio_CS27_North\n");
                  break;
                case 13402: // Proj_Ohio_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Ohio_CS27_South\n");
                  break;
                case 13431: // Proj_Ohio_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Ohio_CS83_North\n");
                  break;
                case 13432: // Proj_Ohio_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Ohio_CS83_South\n");
                  break;
                case 13501: // Proj_Oklahoma_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oklahoma_CS27_North\n");
                  break;
                case 13502: // Proj_Oklahoma_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oklahoma_CS27_South\n");
                  break;
                case 13531: // Proj_Oklahoma_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oklahoma_CS83_North\n");
                  break;
                case 13532: // Proj_Oklahoma_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oklahoma_CS83_South\n");
                  break;
                case 13601: // Proj_Oregon_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oregon_CS27_North\n");
                  break;
                case 13602: // Proj_Oregon_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oregon_CS27_South\n");
                  break;
                case 13631: // Proj_Oregon_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oregon_CS83_North\n");
                  break;
                case 13632: // Proj_Oregon_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Oregon_CS83_South\n");
                  break;
                case 13701: // Proj_Pennsylvania_CS27_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Pennsylvania_CS27_North\n");
                  break;
                case 13702: // Proj_Pennsylvania_CS27_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Pennsylvania_CS27_South\n");
                  break;
                case 13731: // Proj_Pennsylvania_CS83_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Pennsylvania_CS83_North\n");
                  break;
                case 13732: // Proj_Pennsylvania_CS83_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Pennsylvania_CS83_South\n");
                  break;
                case 13800: // Proj_Rhode_Island_CS27				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Rhode_Island_CS27\n");
                  break;
                case 13830: // Proj_Rhode_Island_CS83				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Rhode_Island_CS83\n");
                  break;
                case 13901: // Proj_South_Carolina_CS27_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_South_Carolina_CS27_North\n");
                  break;
                case 13902: // Proj_South_Carolina_CS27_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_South_Carolina_CS27_South\n");
                  break;
                case 13930: // Proj_South_Carolina_CS83				
                  fprintf(file_out, "ProjectionGeoKey: Proj_South_Carolina_CS83\n");
                  break;
                case 14001: // Proj_South_Dakota_CS27_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_South_Dakota_CS27_North\n");
                  break;
                case 14002: // Proj_South_Dakota_CS27_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_South_Dakota_CS27_South\n");
                  break;
                case 14031: // Proj_South_Dakota_CS83_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_South_Dakota_CS83_North\n");
                  break;
                case 14032: // Proj_South_Dakota_CS83_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_South_Dakota_CS83_South\n");
                  break;
                case 14100: // Proj_Tennessee_CS27					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Tennessee_CS27\n");
                  break;
                case 14130: // Proj_Tennessee_CS83					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Tennessee_CS83\n");
                  break;
                case 14201: // Proj_Texas_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS27_North\n");
                  break;
                case 14202: // Proj_Texas_CS27_North_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS27_North_Central\n");
                  break;
                case 14203: // Proj_Texas_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS27_Central\n");
                  break;
                case 14204: // Proj_Texas_CS27_South_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS27_South_Central\n");
                  break;
                case 14205: // Proj_Texas_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS27_South\n");
                  break;
                case 14231: // Proj_Texas_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS83_North\n");
                  break;
                case 14232: // Proj_Texas_CS83_North_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS83_North_Central\n");
                  break;
                case 14233: // Proj_Texas_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS83_Central\n");
                  break;
                case 14234: // Proj_Texas_CS83_South_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS83_South_Central\n");
                  break;
                case 14235: // Proj_Texas_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Texas_CS83_South\n");
                  break;
                case 14301: // Proj_Utah_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Utah_CS27_North\n");
                  break;
                case 14302: // Proj_Utah_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Utah_CS27_Central\n");
                  break;
                case 14303: // Proj_Utah_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Utah_CS27_South\n");
                  break;
                case 14331: // Proj_Utah_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Utah_CS83_North\n");
                  break;
                case 14332: // Proj_Utah_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Utah_CS83_Central\n");
                  break;
                case 14333: // Proj_Utah_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Utah_CS83_South\n");
                  break;
                case 14400: // Proj_Vermont_CS27					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Vermont_CS27\n");
                  break;
                case 14430: // Proj_Vermont_CS83					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Vermont_CS83\n");
                  break;
                case 14501: // Proj_Virginia_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Virginia_CS27_North\n");
                  break;
                case 14502: // Proj_Virginia_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Virginia_CS27_South\n");
                  break;
                case 14531: // Proj_Virginia_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Virginia_CS83_North\n");
                  break;
                case 14532: // Proj_Virginia_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Virginia_CS83_South\n");
                  break;
                case 14601: // Proj_Washington_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Washington_CS27_North\n");
                  break;
                case 14602: // Proj_Washington_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Washington_CS27_South\n");
                  break;
                case 14631: // Proj_Washington_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Washington_CS83_North\n");
                  break;
                case 14632: // Proj_Washington_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Washington_CS83_South\n");
                  break;
                case 14701: // Proj_West_Virginia_CS27_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_West_Virginia_CS27_North\n");
                  break;
                case 14702: // Proj_West_Virginia_CS27_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_West_Virginia_CS27_South\n");
                  break;
                case 14731: // Proj_West_Virginia_CS83_North			
                  fprintf(file_out, "ProjectionGeoKey: Proj_West_Virginia_CS83_North\n");
                  break;
                case 14732: // Proj_West_Virginia_CS83_South			
                  fprintf(file_out, "ProjectionGeoKey: Proj_West_Virginia_CS83_South\n");
                  break;
                case 14801: // Proj_Wisconsin_CS27_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wisconsin_CS27_North\n");
                  break;
                case 14802: // Proj_Wisconsin_CS27_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wisconsin_CS27_Central\n");
                  break;
                case 14803: // Proj_Wisconsin_CS27_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wisconsin_CS27_South\n");
                  break;
                case 14831: // Proj_Wisconsin_CS83_North				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wisconsin_CS83_North\n");
                  break;
                case 14832: // Proj_Wisconsin_CS83_Central				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wisconsin_CS83_Central\n");
                  break;
                case 14833: // Proj_Wisconsin_CS83_South				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wisconsin_CS83_South\n");
                  break;
                case 14901: // Proj_Wyoming_CS27_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS27_East\n");
                  break;
                case 14902: // Proj_Wyoming_CS27_East_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS27_East_Central\n");
                  break;
                case 14903: // Proj_Wyoming_CS27_West_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS27_West_Central\n");
                  break;
                case 14904: // Proj_Wyoming_CS27_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS27_West\n");
                  break;
                case 14931: // Proj_Wyoming_CS83_East				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS83_East\n");
                  break;
                case 14932: // Proj_Wyoming_CS83_East_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS83_East_Central\n");
                  break;
                case 14933: // Proj_Wyoming_CS83_West_Central			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS83_West_Central\n");
                  break;
                case 14934: // Proj_Wyoming_CS83_West				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Wyoming_CS83_West\n");
                  break;
                case 15001: // Proj_Alaska_CS27_1					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_1\n");
                  break;
                case 15002: // Proj_Alaska_CS27_2					
                  fprintf(file_out, "ProjectionGeoKey: ProjectionGeoKey\n");
                  break;
                case 15003: // Proj_Alaska_CS27_3					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_3\n");
                  break;
                case 15004: // Proj_Alaska_CS27_4					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_4\n");
                  break;
                case 15005: // Proj_Alaska_CS27_5					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_5\n");
                  break;
                case 15006: // Proj_Alaska_CS27_6					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_6\n");
                  break;
                case 15007: // Proj_Alaska_CS27_7					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_7\n");
                  break;
                case 15008: // Proj_Alaska_CS27_8					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_8\n");
                  break;
                case 15009: // Proj_Alaska_CS27_9					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_9\n");
                  break;
                case 15010: // Proj_Alaska_CS27_10					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS27_10\n");
                  break;
                case 15031: // Proj_Alaska_CS83_1					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_1\n");
                  break;
                case 15032: // Proj_Alaska_CS83_2					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_2\n");
                  break;
                case 15033: // Proj_Alaska_CS83_3					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_3\n");
                  break;
                case 15034: // Proj_Alaska_CS83_4					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_4\n");
                  break;
                case 15035: // Proj_Alaska_CS83_5					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_5\n");
                  break;
                case 15036: // Proj_Alaska_CS83_6					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_6\n");
                  break;
                case 15037: // Proj_Alaska_CS83_7					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_7\n");
                  break;
                case 15038: // Proj_Alaska_CS83_8					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_8\n");
                  break;
                case 15039: // Proj_Alaska_CS83_9					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_9\n");
                  break;
                case 15040: // Proj_Alaska_CS83_10					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Alaska_CS83_10\n");
                  break;
                case 15101: // Proj_Hawaii_CS27_1					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS27_1\n");
                  break;
                case 15102: // Proj_Hawaii_CS27_2					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS27_2\n");
                  break;
                case 15103: // Proj_Hawaii_CS27_3					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS27_3\n");
                  break;
                case 15104: // Proj_Hawaii_CS27_4					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS27_4\n");
                  break;
                case 15105: // Proj_Hawaii_CS27_5					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS27_5\n");
                  break;
                case 15131: // Proj_Hawaii_CS83_1					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS83_1\n");
                  break;
                case 15132: // Proj_Hawaii_CS83_2					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS83_2\n");
                  break;
                case 15133: // Proj_Hawaii_CS83_3					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS83_3\n");
                  break;
                case 15134: // Proj_Hawaii_CS83_4					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS83_4\n");
                  break;
                case 15135: // Proj_Hawaii_CS83_5					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Hawaii_CS83_5\n");
                  break;
                case 15201: // Proj_Puerto_Rico_CS27				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Puerto_Rico_CS27\n");
                  break;
                case 15202: // Proj_St_Croix					
                  fprintf(file_out, "ProjectionGeoKey: Proj_St_Croix\n");
                  break;
                case 15230: // Proj_Puerto_Rico_Virgin_Is				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Puerto_Rico_Virgin_Is\n");
                  break;
                case 15914: // Proj_BLM_14N_feet					
                  fprintf(file_out, "ProjectionGeoKey: Proj_BLM_14N_feet\n");
                  break;
                case 15915: // Proj_BLM_15N_feet					
                  fprintf(file_out, "ProjectionGeoKey: Proj_BLM_15N_feet\n");
                  break;
                case 15916: // Proj_BLM_16N_feet					
                  fprintf(file_out, "ProjectionGeoKey: Proj_BLM_16N_feet\n");
                  break;
                case 15917: // Proj_BLM_17N_feet					
                  fprintf(file_out, "ProjectionGeoKey: Proj_BLM_17N_feet\n");
                  break;
                case 17348: // Proj_Map_Grid_of_Australia_48			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_48\n");
                  break;
                case 17349: // Proj_Map_Grid_of_Australia_49			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_49\n");
                  break;
                case 17350: // Proj_Map_Grid_of_Australia_50			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_50\n");
                  break;
                case 17351: // Proj_Map_Grid_of_Australia_51			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_51\n");
                  break;
                case 17352: // Proj_Map_Grid_of_Australia_52			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_52\n");
                  break;
                case 17353: // Proj_Map_Grid_of_Australia_53			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_53\n");
                  break;
                case 17354: // Proj_Map_Grid_of_Australia_54			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_54\n");
                  break;
                case 17355: // Proj_Map_Grid_of_Australia_55			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_55\n");
                  break;
                case 17356: // Proj_Map_Grid_of_Australia_56			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_56\n");
                  break;
                case 17357: // Proj_Map_Grid_of_Australia_57			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_57\n");
                  break;
                case 17358: // Proj_Map_Grid_of_Australia_58			
                  fprintf(file_out, "ProjectionGeoKey: Proj_Map_Grid_of_Australia_58\n");
                  break;
                case 17448: // Proj_Australian_Map_Grid_48				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_48\n");
                  break;
                case 17449: // Proj_Australian_Map_Grid_49				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_49\n");
                  break;
                case 17450: // Proj_Australian_Map_Grid_50				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_50\n");
                  break;
                case 17451: // Proj_Australian_Map_Grid_51				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_51\n");
                  break;
                case 17452: // Proj_Australian_Map_Grid_52				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_52\n");
                  break;
                case 17453: // Proj_Australian_Map_Grid_53				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_53\n");
                  break;
                case 17454: // Proj_Australian_Map_Grid_54				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_54\n");
                  break;
                case 17455: // Proj_Australian_Map_Grid_55				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_55\n");
                  break;
                case 17456: // Proj_Australian_Map_Grid_56				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_56\n");
                  break;
                case 17457: // Proj_Australian_Map_Grid_57				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_57\n");
                  break;
                case 17458: // Proj_Australian_Map_Grid_58				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Australian_Map_Grid_58\n");
                  break;
                case 18031: // Proj_Argentina_1					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Argentina_1\n");
                  break;
                case 18032: // Proj_Argentina_2					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Argentina_2\n");
                  break;
                case 18033: // Proj_Argentina_3					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Argentina_3\n");
                  break;
                case 18034: // Proj_Argentina_4					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Argentina_4\n");
                  break;
                case 18035: // Proj_Argentina_5					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Argentina_5\n");
                  break;
                case 18036: // Proj_Argentina_6					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Argentina_6\n");
                  break;
                case 18037: // Proj_Argentina_7					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Argentina_7\n");
                  break;
                case 18051: // Proj_Colombia_3W					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colombia_3W\n");
                  break;
                case 18052: // Proj_Colombia_Bogota				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colombia_Bogota\n");
                  break;
                case 18053: // Proj_Colombia_3E					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colombia_3E\n");
                  break;
                case 18054: // Proj_Colombia_6E					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Colombia_6E\n");
                  break;
                case 18072: // Proj_Egypt_Red_Belt					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Egypt_Red_Belt\n");
                  break;
                case 18073: // Proj_Egypt_Purple_Belt				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Egypt_Purple_Belt\n");
                  break;
                case 18074: // Proj_Extended_Purple_Belt				
                  fprintf(file_out, "ProjectionGeoKey: Proj_Extended_Purple_Belt\n");
                  break;
                case 18141: // Proj_New_Zealand_North_Island_Nat_Grid		
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Zealand_North_Island_Nat_Grid\n");
                  break;
                case 18142: // Proj_New_Zealand_South_Island_Nat_Grid		
                  fprintf(file_out, "ProjectionGeoKey: Proj_New_Zealand_South_Island_Nat_Grid\n");
                  break;
                case 19900: // Proj_Bahrain_Grid					
                  fprintf(file_out, "ProjectionGeoKey: Proj_Bahrain_Grid\n");
                  break;
                case 19905: // Proj_Netherlands_E_Indies_Equatorial		
                  fprintf(file_out, "ProjectionGeoKey: Proj_Netherlands_E_Indies_Equatorial\n");
                  break;
                case 19912: // Proj_RSO_Borneo
                  fprintf(file_out, "ProjectionGeoKey: Proj_RSO_Borneo\n");
                  break;
                default:
                  fprintf(file_out, "ProjectionGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 3075: // ProjCoordTransGeoKey
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 1: // CT_TransverseMercator
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_TransverseMercator\n");
                  break;
                case 2: // CT_TransvMercator_Modified_Alaska
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_TransvMercator_Modified_Alaska\n");
                  break;
                case 3: // CT_ObliqueMercator
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_ObliqueMercator\n");
                  break;
                case 4: // CT_ObliqueMercator_Laborde
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_ObliqueMercator_Laborde\n");
                  break;
                case 5: // CT_ObliqueMercator_Rosenmund
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_ObliqueMercator_Rosenmund\n");
                  break;
                case 6: // CT_ObliqueMercator_Spherical
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_ObliqueMercator_Spherical\n");
                  break;
                case 7: // CT_Mercator
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Mercator\n");
                  break;
                case 8: // CT_LambertConfConic_2SP
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_LambertConfConic_2SP\n");
                  break;
                case 9: // CT_LambertConfConic_Helmert
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_LambertConfConic_Helmert\n");
                  break;
                case 10: // CT_LambertAzimEqualArea
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_LambertAzimEqualArea\n");
                  break;
                case 11: // CT_AlbersEqualArea
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_AlbersEqualArea\n");
                  break;
                case 12: // CT_AzimuthalEquidistant
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_AzimuthalEquidistant\n");
                  break;
                case 13: // CT_EquidistantConic
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_EquidistantConic\n");
                  break;
                case 14: // CT_Stereographic
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Stereographic\n");
                  break;
                case 15: // CT_PolarStereographic
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_PolarStereographic\n");
                  break;
                case 16: // CT_ObliqueStereographic
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_ObliqueStereographic\n");
                  break;
                case 17: // CT_Equirectangular
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Equirectangular\n");
                  break;
                case 18: // CT_CassiniSoldner
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_CassiniSoldner\n");
                  break;
                case 19: // CT_Gnomonic
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Gnomonic\n");
                  break;
                case 20: // CT_MillerCylindrical
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_MillerCylindrical\n");
                  break;
                case 21: // CT_Orthographic
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Orthographic\n");
                  break;
                case 22: // CT_Polyconic
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Polyconic\n");
                  break;
                case 23: // CT_Robinson
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Robinson\n");
                  break;
                case 24: // CT_Sinusoidal
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_Sinusoidal\n");
                  break;
                case 25: // CT_VanDerGrinten
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_VanDerGrinten\n");
                  break;
                case 26: // CT_NewZealandMapGrid
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_NewZealandMapGrid\n");
                  break;
                case 27: // CT_TransvMercator_SouthOriented
                  fprintf(file_out, "ProjCoordTransGeoKey: CT_TransvMercator_SouthOriented\n");
                  break;
                default:
                  fprintf(file_out, "ProjCoordTransGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 3076: // ProjLinearUnitsGeoKey
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 9001: // Linear_Meter
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Meter\n");
                  break;
                case 9002: // Linear_Foot
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Foot\n");
                  break;
                case 9003: // Linear_Foot_US_Survey
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Foot_US_Survey\n");
                  break;
                case 9004: // Linear_Foot_Modified_American
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Foot_Modified_American\n");
                  break;
                case 9005: // Linear_Foot_Clarke
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Foot_Clarke\n");
                  break;
                case 9006: // Linear_Foot_Indian
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Foot_Indian\n");
                  break;
                case 9007: // Linear_Link
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Link\n");
                  break;
                case 9008: // Linear_Link_Benoit
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Link_Benoit\n");
                  break;
                case 9009: // Linear_Link_Sears
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Link_Sears\n");
                  break;
                case 9010: // Linear_Chain_Benoit
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Chain_Benoit\n");
                  break;
                case 9011: // Linear_Chain_Sears
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Chain_Sears\n");
                  break;
                case 9012: // Linear_Yard_Sears
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Yard_Sears\n");
                  break;
                case 9013: // Linear_Yard_Indian
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Yard_Indian\n");
                  break;
                case 9014: // Linear_Fathom
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Fathom\n");
                  break;
                case 9015: // Linear_Mile_International_Nautical
                  fprintf(file_out, "ProjLinearUnitsGeoKey: Linear_Mile_International_Nautical\n");
                  break;
                default:
                  fprintf(file_out, "ProjLinearUnitsGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 3077: // ProjLinearUnitSizeGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjLinearUnitSizeGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3078: // ProjStdParallel1GeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjStdParallel1GeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3079: // ProjStdParallel2GeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjStdParallel2GeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;        
              case 3080: // ProjNatOriginLongGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjNatOriginLongGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3081: // ProjNatOriginLatGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjNatOriginLatGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3082: // ProjFalseEastingGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjFalseEastingGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3083: // ProjFalseNorthingGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjFalseNorthingGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3084: // ProjFalseOriginLongGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjFalseOriginLongGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3085: // ProjFalseOriginLatGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjFalseOriginLatGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3086: // ProjFalseOriginEastingGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjFalseOriginEastingGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3087: // ProjFalseOriginNorthingGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjFalseOriginNorthingGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3088: // ProjCenterLongGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjCenterLongGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3089: // ProjCenterLatGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjCenterLatGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3090: // ProjCenterEastingGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjCenterEastingGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3091: // ProjCenterNorthingGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjCenterNorthingGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3092: // ProjScaleAtNatOriginGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjScaleAtNatOriginGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3093: // ProjScaleAtCenterGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjScaleAtCenterGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3094: // ProjAzimuthAngleGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjAzimuthAngleGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 3095: // ProjStraightVertPoleLongGeoKey
                if (lasreader->header.vlr_geo_double_params)
                {
                  fprintf(file_out, "ProjStraightVertPoleLongGeoKey: %.8g\n",lasreader->header.vlr_geo_double_params[lasreader->header.vlr_geo_key_entries[j].value_offset]);
                }
                break;
              case 4096: // VerticalCSTypeGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 5001: // VertCS_Airy_1830_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Airy_1830_ellipsoid\n");
                  break;
                case 5002: // VertCS_Airy_Modified_1849_ellipsoid 
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Airy_Modified_1849_ellipsoid\n");
                  break;
                case 5003: // VertCS_ANS_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_ANS_ellipsoid\n");
                  break;
                case 5004: // VertCS_Bessel_1841_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Bessel_1841_ellipsoid\n");
                  break;
                case 5005: // VertCS_Bessel_Modified_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Bessel_Modified_ellipsoid\n");
                  break;
                case 5006: // VertCS_Bessel_Namibia_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Bessel_Namibia_ellipsoid\n");
                  break;
                case 5007: // VertCS_Clarke_1858_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Clarke_1858_ellipsoid\n");
                  break;
                case 5008: // VertCS_Clarke_1866_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Clarke_1866_ellipsoid\n");
                  break;
                case 5010: // VertCS_Clarke_1880_Benoit_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Clarke_1880_Benoit_ellipsoid\n");
                  break;
                case 5011: // VertCS_Clarke_1880_IGN_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Clarke_1880_IGN_ellipsoid\n");
                  break;
                case 5012: // VertCS_Clarke_1880_RGS_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Clarke_1880_RGS_ellipsoid\n");
                  break;
                case 5013: // VertCS_Clarke_1880_Arc_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Clarke_1880_Arc_ellipsoid\n");
                  break;
                case 5014: // VertCS_Clarke_1880_SGA_1922_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Clarke_1880_SGA_1922_ellipsoid\n");
                  break;
                case 5015: // VertCS_Everest_1830_1937_Adjustment_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Everest_1830_1937_Adjustment_ellipsoid\n");
                  break;
                case 5016: // VertCS_Everest_1830_1967_Definition_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Everest_1830_1967_Definition_ellipsoid\n");
                  break;
                case 5017: // VertCS_Everest_1830_1975_Definition_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Everest_1830_1975_Definition_ellipsoid\n");
                  break;
                case 5018: // VertCS_Everest_1830_Modified_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Everest_1830_Modified_ellipsoid\n");
                  break;
                case 5019: // VertCS_GRS_1980_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_GRS_1980_ellipsoid\n");
                  break;
                case 5020: // VertCS_Helmert_1906_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Helmert_1906_ellipsoid\n");
                  break;
                case 5021: // VertCS_INS_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_INS_ellipsoid\n");
                  break;
                case 5022: // VertCS_International_1924_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_International_1924_ellipsoid\n");
                  break;
                case 5023: // VertCS_International_1967_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_International_1967_ellipsoid\n");
                  break;
                case 5024: // VertCS_Krassowsky_1940_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Krassowsky_1940_ellipsoid\n");
                  break;
                case 5025: // VertCS_NWL_9D_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_NWL_9D_ellipsoid\n");
                  break;
                case 5026: // VertCS_NWL_10D_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_NWL_10D_ellipsoid\n");
                  break;
                case 5027: // VertCS_Plessis_1817_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Plessis_1817_ellipsoid\n");
                  break;
                case 5028: // VertCS_Struve_1860_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Struve_1860_ellipsoid\n");
                  break;
                case 5029: // VertCS_War_Office_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_War_Office_ellipsoid\n");
                  break;
                case 5030: // VertCS_WGS_84_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_WGS_84_ellipsoid\n");
                  break;
                case 5031: // VertCS_GEM_10C_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_GEM_10C_ellipsoid\n");
                  break;
                case 5032: // VertCS_OSU86F_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_OSU86F_ellipsoid\n");
                  break;
                case 5033: // VertCS_OSU91A_ellipsoid
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_OSU91A_ellipsoid\n");
                  break;
                case 5101: // VertCS_Newlyn
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Newlyn\n");
                  break;
                case 5102: // VertCS_North_American_Vertical_Datum_1929
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_North_American_Vertical_Datum_1929\n");
                  break;
                case 5103: // VertCS_North_American_Vertical_Datum_1988
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_North_American_Vertical_Datum_1988\n");
                  break;
                case 5104: // VertCS_Yellow_Sea_1956
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Yellow_Sea_1956\n");
                  break;
                case 5105: // VertCS_Baltic_Sea
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Baltic_Sea\n");
                  break;
                case 5106: // VertCS_Caspian_Sea
                  fprintf(file_out, "VerticalCSTypeGeoKey: VertCS_Caspian_Sea\n");
                  break;
                default:
                  fprintf(file_out, "VerticalCSTypeGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              case 4097: // VerticalCitationGeoKey
                if (lasreader->header.vlr_geo_ascii_params)
                {
                  char dummy[256];
                  strncpy(dummy, &(lasreader->header.vlr_geo_ascii_params[lasreader->header.vlr_geo_key_entries[j].value_offset]), lasreader->header.vlr_geo_key_entries[j].count);
                  dummy[lasreader->header.vlr_geo_key_entries[j].count-1] = '\0';
                  fprintf(file_out, "VerticalCitationGeoKey: %s\n",dummy);
                }
                break;
              case 4098: // VerticalDatumGeoKey 
                fprintf(file_out, "VerticalDatumGeoKey: Vertical Datum Codes %d\n",lasreader->header.vlr_geo_key_entries[j].value_offset);
                break;
              case 4099: // VerticalUnitsGeoKey 
                switch (lasreader->header.vlr_geo_key_entries[j].value_offset)
                {
                case 9001: // Linear_Meter
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Meter\n");
                  break;
                case 9002: // Linear_Foot
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Foot\n");
                  break;
                case 9003: // Linear_Foot_US_Survey
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Foot_US_Survey\n");
                  break;
                case 9004: // Linear_Foot_Modified_American
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Foot_Modified_American\n");
                  break;
                case 9005: // Linear_Foot_Clarke
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Foot_Clarke\n");
                  break;
                case 9006: // Linear_Foot_Indian
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Foot_Indian\n");
                  break;
                case 9007: // Linear_Link
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Link\n");
                  break;
                case 9008: // Linear_Link_Benoit
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Link_Benoit\n");
                  break;
                case 9009: // Linear_Link_Sears
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Link_Sears\n");
                  break;
                case 9010: // Linear_Chain_Benoit
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Chain_Benoit\n");
                  break;
                case 9011: // Linear_Chain_Sears
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Chain_Sears\n");
                  break;
                case 9012: // Linear_Yard_Sears
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Yard_Sears\n");
                  break;
                case 9013: // Linear_Yard_Indian
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Yard_Indian\n");
                  break;
                case 9014: // Linear_Fathom
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Fathom\n");
                  break;
                case 9015: // Linear_Mile_International_Nautical
                  fprintf(file_out, "VerticalUnitsGeoKey: Linear_Mile_International_Nautical\n");
                  break;
                default:
                  fprintf(file_out, "VerticalUnitsGeoKey: look-up for %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].value_offset);
                }
                break;
              default:
                fprintf(stderr, "key ID %d not implemented\n", lasreader->header.vlr_geo_key_entries[j].key_id);
              }
            }
          }
        }
        else if (header->vlrs[i].record_id == 34736) // GeoDoubleParamsTag
        {
          if (file_out) fprintf(file_out, "    GeoDoubleParamsTag (number of doubles %d)\n", lasreader->header.vlrs[i].record_length_after_header/8);
          if (file_out) fprintf(file_out, "      ");
          for (int j = 0; j < lasreader->header.vlrs[i].record_length_after_header/8; j++)
          {
            if (file_out) fprintf(file_out, "%lg ", header->vlr_geo_double_params[j]);
          }
          if (file_out) fprintf(file_out, "\n");
        }
        else if (header->vlrs[i].record_id == 34737) // GeoAsciiParamsTag
        {
          if (file_out) fprintf(file_out, "    GeoAsciiParamsTag (number of characters %d)\n", lasreader->header.vlrs[i].record_length_after_header);
          if (file_out) fprintf(file_out, "      ");
          for (int j = 0; j < lasreader->header.vlrs[i].record_length_after_header; j++)
          {
            if (file_out) fprintf(file_out, "%c", header->vlr_geo_ascii_params[j]);
          }
          if (file_out) fprintf(file_out, "\n");
        }
      }
    }
  }
  if (file_out) if (header->user_data_after_header_size) fprintf(file_out, "the header is followed by %d user-defined bytes\n", header->user_data_after_header_size);

  // some additional histograms

  unsigned int number_of_point_records = 0;
  unsigned int number_of_points_by_return[8] = {0,0,0,0,0,0,0,0};
  int number_of_returns_of_given_pulse[8] = {0,0,0,0,0,0,0,0};
  int classification[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int classification_synthetic = 0;
  int classification_keypoint = 0;
  int classification_withheld = 0;
  double min[3];
  double max[3];

  // loop over the points

  if (check_points)
  {
    if (file_out) fprintf(file_out, "reporting minimum and maximum for all %d LAS point record entries ...\n",lasreader->npoints);
    LASpoint point_min;
    LASpoint point_max;
    double gps_time_min;
    double gps_time_max;
    short rgb_min[3];
    short rgb_max[3];
    double coordinates[3];
    lasreader->read_point(coordinates);
    point_min = lasreader->point;
    point_max = lasreader->point;
    if (lasreader->points_have_gps_time)
    {
      gps_time_min = lasreader->gps_time;
      gps_time_max = lasreader->gps_time;
    }
    if (lasreader->points_have_rgb)
    {
      rgb_min[0] = lasreader->rgb[0];
      rgb_min[1] = lasreader->rgb[1];
      rgb_min[2] = lasreader->rgb[2];
      rgb_max[0] = lasreader->rgb[0];
      rgb_max[1] = lasreader->rgb[1];
      rgb_max[2] = lasreader->rgb[2];
    }
    VecCopy3dv(min, coordinates);
    VecCopy3dv(max, coordinates);
    number_of_point_records = 1;
    number_of_points_by_return[lasreader->point.return_number]++;
    number_of_returns_of_given_pulse[lasreader->point.number_of_returns_of_given_pulse]++;
    classification[(lasreader->point.classification & 31)]++;
    if (lasreader->point.classification & 32) classification_synthetic++;
    if (lasreader->point.classification & 64) classification_keypoint++;
    if (lasreader->point.classification & 128) classification_withheld++;
    while (lasreader->read_point(coordinates))
    {
      if (lasreader->point.x < point_min.x) point_min.x = lasreader->point.x;
      else if (lasreader->point.x > point_max.x) point_max.x = lasreader->point.x;
      if (lasreader->point.y < point_min.y) point_min.y = lasreader->point.y;
      else if (lasreader->point.y > point_max.y) point_max.y = lasreader->point.y;
      if (lasreader->point.z < point_min.z) point_min.z = lasreader->point.z;
      else if (lasreader->point.z > point_max.z) point_max.z = lasreader->point.z;
      if (lasreader->point.intensity < point_min.intensity) point_min.intensity = lasreader->point.intensity;
      else if (lasreader->point.intensity > point_max.intensity) point_max.intensity = lasreader->point.intensity;
      if (lasreader->point.edge_of_flight_line < point_min.edge_of_flight_line) point_min.edge_of_flight_line = lasreader->point.edge_of_flight_line;
      else if (lasreader->point.edge_of_flight_line > point_max.edge_of_flight_line) point_max.edge_of_flight_line = lasreader->point.edge_of_flight_line;
      if (lasreader->point.scan_direction_flag < point_min.scan_direction_flag) point_min.scan_direction_flag = lasreader->point.scan_direction_flag;
      else if (lasreader->point.scan_direction_flag > point_max.scan_direction_flag) point_max.scan_direction_flag = lasreader->point.scan_direction_flag;
      if (lasreader->point.number_of_returns_of_given_pulse < point_min.number_of_returns_of_given_pulse) point_min.number_of_returns_of_given_pulse = lasreader->point.number_of_returns_of_given_pulse;
      else if (lasreader->point.number_of_returns_of_given_pulse > point_max.number_of_returns_of_given_pulse) point_max.number_of_returns_of_given_pulse = lasreader->point.number_of_returns_of_given_pulse;
      if (lasreader->point.return_number < point_min.return_number) point_min.return_number = lasreader->point.return_number;
      else if (lasreader->point.return_number > point_max.return_number) point_max.return_number = lasreader->point.return_number;
      if (lasreader->point.classification < point_min.classification) point_min.classification = lasreader->point.classification;
      else if (lasreader->point.classification > point_max.classification) point_max.classification = lasreader->point.classification;
      if (lasreader->point.scan_angle_rank < point_min.scan_angle_rank) point_min.scan_angle_rank = lasreader->point.scan_angle_rank;
      else if (lasreader->point.scan_angle_rank > point_max.scan_angle_rank) point_max.scan_angle_rank = lasreader->point.scan_angle_rank;
      if (lasreader->point.user_data < point_min.user_data) point_min.user_data = lasreader->point.user_data;
      else if (lasreader->point.user_data > point_max.user_data) point_max.user_data = lasreader->point.user_data;
      if (lasreader->point.point_source_ID < point_min.point_source_ID) point_min.point_source_ID = lasreader->point.point_source_ID;
      else if (lasreader->point.point_source_ID > point_max.point_source_ID) point_max.point_source_ID = lasreader->point.point_source_ID;
      if (lasreader->point.point_source_ID < point_min.point_source_ID) point_min.point_source_ID = lasreader->point.point_source_ID;
      else if (lasreader->point.point_source_ID > point_max.point_source_ID) point_max.point_source_ID = lasreader->point.point_source_ID;
      if (lasreader->points_have_gps_time)
      {
        if (lasreader->gps_time < gps_time_min) gps_time_min = lasreader->gps_time;
        else if (lasreader->gps_time > gps_time_max) gps_time_max = lasreader->gps_time;
      }
      if (lasreader->points_have_rgb)
      {
        if (lasreader->rgb[0] < rgb_min[0]) rgb_min[0] = lasreader->rgb[0];
        else if (lasreader->rgb[0] > rgb_max[0]) rgb_max[0] = lasreader->rgb[0];
        if (lasreader->rgb[1] < rgb_min[1]) rgb_min[1] = lasreader->rgb[1];
        else if (lasreader->rgb[1] > rgb_max[1]) rgb_max[1] = lasreader->rgb[1];
        if (lasreader->rgb[2] < rgb_min[2]) rgb_min[2] = lasreader->rgb[2];
        else if (lasreader->rgb[2] > rgb_max[2]) rgb_max[2] = lasreader->rgb[2];
      }
      VecUpdateMinMax3dv(min, max, coordinates);
      number_of_point_records++;
      number_of_points_by_return[lasreader->point.return_number]++;
      number_of_returns_of_given_pulse[lasreader->point.number_of_returns_of_given_pulse]++;
      classification[(lasreader->point.classification & 31)]++;
      if (lasreader->point.classification & 32) classification_synthetic++;
      if (lasreader->point.classification & 64) classification_keypoint++;
      if (lasreader->point.classification & 128) classification_withheld++;

      if (lasreader->p_count == 14765)
      {
        lasreader->p_count = 14765;
      }
    }

    if (file_out) fprintf(file_out, "  x %d %d\n",point_min.x, point_max.x);
    if (file_out) fprintf(file_out, "  y %d %d\n",point_min.y, point_max.y);
    if (file_out) fprintf(file_out, "  z %d %d\n",point_min.z, point_max.z);
    if (file_out) fprintf(file_out, "  intensity %d %d\n",point_min.intensity, point_max.intensity);
    if (file_out) fprintf(file_out, "  edge_of_flight_line %d %d\n",point_min.edge_of_flight_line, point_max.edge_of_flight_line);
    if (file_out) fprintf(file_out, "  scan_direction_flag %d %d\n",point_min.scan_direction_flag, point_max.scan_direction_flag);
    if (file_out) fprintf(file_out, "  number_of_returns_of_given_pulse %d %d\n",point_min.number_of_returns_of_given_pulse, point_max.number_of_returns_of_given_pulse);
    if (file_out) fprintf(file_out, "  return_number %d %d\n",point_min.return_number, point_max.return_number);
    if (file_out) fprintf(file_out, "  classification %d %d\n",point_min.classification, point_max.classification);
    if (file_out) fprintf(file_out, "  scan_angle_rank %d %d\n",point_min.scan_angle_rank, point_max.scan_angle_rank);
    if (file_out) fprintf(file_out, "  user_data %d %d\n",point_min.user_data, point_max.user_data);
    if (file_out) fprintf(file_out, "  point_source_ID %d %d\n",point_min.point_source_ID, point_max.point_source_ID);
    if (lasreader->points_have_gps_time)
    {
      if (file_out) fprintf(file_out, "  gps_time %f %f\n",gps_time_min, gps_time_max);
    }
    if (lasreader->points_have_rgb)
    {
      if (file_out) fprintf(file_out, "  R %d %d\n", rgb_min[0], rgb_max[0]);
      if (file_out) fprintf(file_out, "  G %d %d\n", rgb_min[1], rgb_max[1]);
      if (file_out) fprintf(file_out, "  B %d %d\n", rgb_min[2], rgb_max[2]);
    }
  }

  lasreader->close();
  fclose(file);

  if (repair_header || repair_bounding_box || change_header)
  {
    if (file_name)
    {
      if (strstr(file_name, ".gz"))
      {
        fprintf(stderr, "ERROR: cannot change header of gzipped input files\n");
        repair_header = repair_bounding_box = change_header = false;
      }
      else
      {
        file = fopen(file_name, "rb+");
        if (file == 0)
        {
          fprintf (stderr, "ERROR: could reopen file '%s' for changing header\n", file_name);
          repair_header = repair_bounding_box = change_header = false;
        }
      }
    }
    else if (ilas)
    {
      fprintf(stderr, "ERROR: cannot change header of piped input\n");
      repair_header = repair_bounding_box = change_header = false;
    }
    else
    {
      fprintf (stderr, "ERROR: no input specified\n");
      usage();
    }
  }

  if (change_header)
  {
    if (set_version_major != -1)
    {
      fseek(file, 24, SEEK_SET);
      fwrite(&set_version_major, sizeof(char), 1, file);
    }
    if (set_version_minor != -1)
    {
      fseek(file, 25, SEEK_SET);
      fwrite(&set_version_minor, sizeof(char), 1, file);
    }
    if (system_identifier)
    {
      fseek(file, 26, SEEK_SET);
      fwrite(system_identifier, sizeof(char), 32, file);
    }
    if (generating_software)
    {
      fseek(file, 58, SEEK_SET);
      fwrite(generating_software, sizeof(char), 32, file);
    }
    if (file_creation_day || file_creation_year)
    {
      fseek(file, 90, SEEK_SET);
      fwrite(&file_creation_day, sizeof(unsigned short), 1, file);
      fwrite(&file_creation_year, sizeof(unsigned short), 1, file);
    }
  }

  if (check_points)
  {
    if (number_of_point_records != header->number_of_point_records)
    {
      if (file_out) fprintf(file_out, "real number of points (%d) is different from header number of points (%d) %s\n", number_of_point_records, header->number_of_point_records, repair_header ? "(repaired)" : "");
      if (repair_header)
      {
        fseek(file, 107, SEEK_SET);
        fwrite(&number_of_point_records, sizeof(unsigned int), 1, file);
      }
    }

    bool report = false;
    bool was_set = false;
    for (i = 1; i < 6; i++) if (header->number_of_points_by_return[i-1] != number_of_points_by_return[i]) report = true;
    for (i = 1; i < 6; i++) if (header->number_of_points_by_return[i-1]) was_set = true;
    if (report)
    {
      if (file_out) fprintf(file_out, "number of points by return %s", was_set ? "is different than reported in header:" : "was not set in header:"); 
      for (i = 1; i < 6; i++) if (file_out) fprintf(file_out, " %d", number_of_points_by_return[i]); 
      if (file_out) fprintf(file_out, " %s\n", repair_header ? "(repaired)" : "");
      if (repair_header)
      {
        fseek(file, 111, SEEK_SET);
        fwrite(&(number_of_points_by_return[1]), sizeof(unsigned int), 5, file);
      }
    }

    if (number_of_points_by_return[0]) if (file_out) fprintf(file_out, "WARNING: there are %d points with return number 0\n", number_of_points_by_return[0]); 
    if (number_of_points_by_return[6]) if (file_out) fprintf(file_out, "WARNING: there are %d points with return number 6\n", number_of_points_by_return[6]); 
    if (number_of_points_by_return[7]) if (file_out) fprintf(file_out, "WARNING: there are %d points with return number 7\n", number_of_points_by_return[7]); 

    report = false;
    for (i = 1; i < 8; i++) if (number_of_returns_of_given_pulse[i]) report = true;
    if (report)
    {
      if (file_out) fprintf(file_out, "overview over number of returns of given pulse:"); 
      for (i = 1; i < 8; i++) if (file_out) fprintf(file_out, " %d", number_of_returns_of_given_pulse[i]);
      if (file_out) fprintf(file_out, "\n"); 
    }

    if (number_of_returns_of_given_pulse[0]) if (file_out) fprintf(file_out, "WARNING: there are %d points with a number of returns of given pulse of 0\n", number_of_returns_of_given_pulse[0]); 

    report = false;
    for (i = 0; i < 32; i++) if (classification[i]) report = true;
    if (classification_synthetic || classification_keypoint ||  classification_withheld) report = true;

    if (report)
    {
      if (file_out) fprintf(file_out, "histogram of classification of points:\n"); 
      for (i = 0; i < 32; i++) if (classification[i]) if (file_out) fprintf(file_out, " %8d %s (%d)\n", classification[i], LASpointClassification[i], i);
      if (classification_synthetic) if (file_out) fprintf(file_out, " +-> flagged as synthetic: %d\n", classification_synthetic);
      if (classification_keypoint) if (file_out) fprintf(file_out,  " +-> flagged as keypoints: %d\n", classification_keypoint);
      if (classification_withheld) if (file_out) fprintf(file_out,  " +-> flagged as withheld:  %d\n", classification_withheld);
    }

    if (repair_bounding_box)
    {
      if (file_out) fprintf(file_out, "repairing bounding box\n");
      fseek(file, 179, SEEK_SET);
      fwrite(&(max[0]), sizeof(double), 1, file);
      fwrite(&(min[0]), sizeof(double), 1, file);
      fwrite(&(max[1]), sizeof(double), 1, file);
      fwrite(&(min[1]), sizeof(double), 1, file);
      fwrite(&(max[2]), sizeof(double), 1, file);
      fwrite(&(min[2]), sizeof(double), 1, file);
    }
    else
    {
      if (max[0] > header->max_x)
      {
        if (file_out) fprintf(file_out, "real max x larger than header max x by %lf %s\n", max[0] - header->max_x, repair_header ? "(repaired)" : "");
        if (repair_header)
        {
          fseek(file, 179, SEEK_SET);
          fwrite(&(max[0]), sizeof(double), 1, file);
        }
      }
      if (min[0] < header->min_x)
      {
        if (file_out) fprintf(file_out, "real min x smaller than header min x by %lf %s\n", header->min_x - min[0], repair_header ? "(repaired)" : "");
        if (repair_header)
        {
          fseek(file, 187, SEEK_SET);
          fwrite(&(min[0]), sizeof(double), 1, file);
        }
      }
      if (max[1] > header->max_y)
      {
        if (file_out) fprintf(file_out, "real max y larger than header max y by %lf %s\n", max[1] - header->max_y, repair_header ? "(repaired)" : "");
        if (repair_header)
        {
          fseek(file, 195, SEEK_SET);
          fwrite(&(max[1]), sizeof(double), 1, file);
        }
      }
      if (min[1] < header->min_y)
      {
        if (file_out) fprintf(file_out, "real min y smaller than header min y by %lf %s\n", header->min_y - min[1], repair_header ? "(repaired)" : "");
        if (repair_header)
        {
          fseek(file, 203, SEEK_SET);
          fwrite(&(min[1]), sizeof(double), 1, file);
        }
      }
      if (max[2] > header->max_z)
      {
        if (file_out) fprintf(file_out, "real max z larger than header max z by %lf %s\n", max[2] - header->max_z, repair_header ? "(repaired)" : "");
        if (repair_header)
        {
          fseek(file, 211, SEEK_SET);
          fwrite(&(max[2]), sizeof(double), 1, file);
        }
      }
      if (min[2] < header->min_z)
      {
        if (file_out) fprintf(file_out, "real min z smaller than header min z by %lf %s\n", header->min_z - min[2], repair_header ? "(repaired)" : "");
        if (repair_header)
        {
          fseek(file, 219, SEEK_SET);
          fwrite(&(min[2]), sizeof(double), 1, file);
        }
      }
    }
  }

  if (repair_header || repair_bounding_box || change_header)
  {
    fclose(file);
  }

  byebye(argc==1);

  return 0;
}
