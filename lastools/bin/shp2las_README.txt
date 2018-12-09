****************************************************************

  shp2las: converts from ESRI's Shapefile format to binary LAS
           given the input contains Point or MultiPoint records.
 
****************************************************************

example usage:

>> shp2las -i lidar.shp -o lidar.las

converts the ESRI's Shapefile 'lidar.shp' to the LAS file 'lidar.las'

>> shp2las -i lidar.shp -o lidar.las -verbose

converts the ESRI's Shapefile 'lidar.shp' to the LAS file 'lidar.las'
and outputs some of the header information found in the SHP file

C:\lastools\bin>shp2las -h
usage:
shp2las -i lidar.shp -o lidar.las
shp2las -i lidar.shp -o lidar.las -verbose
shp2las -h

---------------

if you find bugs let me (isenburg@cs.unc.edu) know.
