****************************************************************

  las2tin.exe:

  reads LIDAR data in the LAS format and triangulates it. It is
  possible to triangulate only certain points such as only first
  returns (-first_only) or only last returns (-last_only). One
  can also only triangulate only points that have a particular
  classification. The option (-keep_class 2 -keep_class 3) will
  triangulate only the points of classification 2 or 3.
 
****************************************************************

example usage:

>> las2tin -i lidar.las -o tin.shp

triangulates all points and stores the resulting TIN in ESRI's Shapefile format

>> las2tin -i lidar.las -o tin.obj
>> las2tin -i lidar.las -oobj > tin.obj

triangulates all points and stores the resulting TIN in OBJ format

>> las2tin -i lidar.las -o triangles.txt -last_only

triangulates all last return points from lidar.las and stores them
as a list of triangles in ASCII format in triangles.txt.

>> las2tin -i lidar.las -o tin.shp -keep_class 2 -keep_class 3

triangulates all points classfied as 2 or 3 from lidar.las and stores
the resulting TIN in ESRI's Shapefile format to file tin.shp

for more info:

C:\lastools\bin>las2tin -h
usage:
las2tin -i lidar.las -o tin.shp
las2tin -i lidar.las -o triangles.txt
las2tin -i lidar.las -first_only -o mesh.obj
las2tin -last_only lidar.las mesh.obj
las2tin -i lidar.las -keep_class 2 -keep_class 3 -keep_class 9 -otxt > triangles.txt
las2tin -i lidar.las -keep_class 8 -oobj
las2tin -h


---------------

if you find bugs let me (isenburg@cs.unc.edu) know.
