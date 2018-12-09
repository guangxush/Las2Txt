****************************************************************

  las2shp: converts from binary LAS to ESRI's Shapefile format
           by grouping consecutive points into MultiPointZ
           records. The default record size is 1024. It can be
           changed with, for example, a '-record 2048' flag. 

           If you need the *.dbf file as well ... you can create
           this with the ShapeViewer.exe software (by Mohammed
           Hammoud). Just google for it.
 
****************************************************************

example usage:

>> las2shp -i lidar.las -o lidar.shp

converts the LAS file 'lidar.las' to ESRI's Shapefile format 'lidar.shp'
using MultiPointZ records containing 1024 points each.

>> las2shp -i lidar.las -o lidar.shp -record 2048

converts the LAS file 'lidar.las' to ESRI's Shapefile format 'lidar.shp'
using MultiPointZ records containing 2048 points each.

C:\lastools\bin>las2shp -h
usage:
las2shp -i lidar.las -o lidar.shp
las2shp -i lidar.las -o lidar.shp -record 2048
las2shp -h

---------------

if you find bugs let me (isenburg@cs.unc.edu) know.
