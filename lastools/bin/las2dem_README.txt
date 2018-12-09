****************************************************************

  las2dem.exe:

  This tool reads LIDAR points from the LAS format, triangulates
  them temporarily into a TIN, and then rasters the TIN onto a
  DEM. The output is either in PNG, JPG, TIF, or BIL format. Optionally,
  a KML file is generated that puts the DEM in geospatial context
  to be displayed in Google Earth. If the LAS file contains
  projection information (i.e. geo keys as variable length records)
  then this metadata used to create the georeferencing in the KML
  file. But it is also possible to explicitely provide georeferencing
  information for the KML file creation in the command-line.
 
****************************************************************

example usage:

>> las2dem -i lidar.las -o dem.png

rasters all points and stores the resulting DEM in PNG format using
hillside shading

>> las2dem -i lidar.las -o dem.jpg -last_only

rasters all last returns and stores the resulting DEM in JPG format
using hillside shading

>> las2dem -i lidar.las -o dem.tif -keep_class 2 -keep_class 3 -elevation_color

rasters all points classfied as 2 or 3 from lidar.las and stores
stores the resulting DEM in TIF format using elevation coloring

try the following commands for generating some interesting georeferenced
DEMs that you can look at in Google Earth by double clicking the automatically
generated KML file

>> las2dem -i ..\data\test.las -o test.png
>> las2dem -i ..\data\TO_core_last_zoom.las -o toronto.png -utm 17S -ellipsoid 23
>> las2dem -i ..\data\SerpentMound.las -o SerpentMound.png

other commandline arguments are

-step 2              : raster with stepsize 2 (the default is 1)
-stepx 3             : raster horizontally with stepsize 3 (the default is 1)
-stepy 4             : raster vertically with stepsize 4 (the default is 1)
-nrows 512           : raster at most 512 rows
-ncols 512           : raster at most 512 columns
-llx 300000          : start rastering at this lower left x coordinate
-lly 600000          : start rastering at this lower left y coordinate
-ll 300000 600000    : start rastering at these lower left x and y coordinates
-nodata 9999         : use 9999 as the nodata value in the BIL format
-elevation_color     : color the image based on elevation with grey (default is hillshade)
-false_color         : color the image based on elevation with false colours
-elevation           : output the actual elevation value (used with BIL format)
-elevation_scale 2.0 : multiply all elevations by 2.0 before rastering
-nbits 32            : use 32 bits to represent the elevation (used with BIL format)
-light 1 1 2         : change the direction of the light vector for hillside shading
-utm 12T             : use UTM zone 12T to spatially georeference the image
-ellipsoid 23        : use the WGS-84 ellipsoid (do -ellipsoid -1 for a list)
-sp83 CO_S           : use the NAD83 Colorado South state plane for georeferencing
-sp27 SC_N           : use the NAD27 South Carolina North state plane
-survey_feet         : use survey feet
-feet                : use feet
-meter               : use meter
-elevation_feet      : use feet for elevation
-elevation_meter     : use meter for elevation
-tiling_ns crater 500 : create a tiling of DEMs named crater with tiles of size 500 
-traverse_mercator 609601.22 0.0 meter 33.75 -79 0.99996
-traverse_mercator 1804461.942257 0.0 feet 0.8203047 -2.1089395 0.99996
-lambert_conic_conformal 609601.22 0.0 meter 33.75 -79 34.33333 36.16666
-lambert_conic_conformal 1640416.666667 0.0 surveyfeet 47.000000 -120.833333 47.50 48.733333

for more info:

C:\lastools\bin>las2dem -h
usage:
las2dem -i lidar.las -o lidar.png
las2dem -i lidar.las -step 0.5 -o lidar.tif -false_coloring
las2dem -i lidar.las -ncols 400 -nrows 400 -o lidar.jpg -false_coloring
las2dem -i lidar.las -first_only -o lidar.jpg -utm 11S -ellipsoid 23
las2dem -last_only lidar.las lidar.png -sp83 TX_N -ellipsoid 11 -elevation_coloring
las2dem -i lidar.las -keep_class 2 -keep_class 3 -keep_class 9 -o lines.png -sp27 PA_N -ellipsoid 5
las2dem -i lidar.las -keep_class 8 -o lines.bil -nbits 32 -elevation
las2dem -h

---------------

if you find bugs let me (isenburg@cs.unc.edu) know.

---------------

please note that this software does not work in streaming mode and is therefore
not suited for extremely large LAS files. to work efficiently out-of-core for
turning large amounts of LAS data into DEM tilings, please have a look at our
streaming pipeline consisting of spfinalize, spdelaunay2d, and tin2dem.exe.
