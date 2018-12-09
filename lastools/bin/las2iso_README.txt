****************************************************************

  las2iso.exe:

  reads LIDAR data in the LAS format and extracts isocontours by
  constructing and interpolating a temporary TIN. It is possible
  to triangulate only certain points such as only first returns
  (-first_only) or only last returns (-last_only). One can also
  only triangulate points that have a particular classification.
  The option (-keep_class 2 -keep_class 3) will triangulate only
  the points of classification 2 or 3.

  The resulting isocontours are stored either in ESRI's Shapefile
  format (-o contours.shp) or as a text file (-o contours.txt).
 
****************************************************************

example usage:

>> las2iso -i lidar.las -o contours.shp

extracts 10 evenly spaced contours and stores the result in ESRI's
Shapefile format

>> las2iso -i lidar.las -o contours.shp -every 5 -simplify 1 -clean 10
 
extracts evenly spaced contours every 10 units, simplifies away all
segments that are shorter than 1 unit, cleans out all contours whose
total length is shorter than 10 units, and then stores the result in
ESRI's Shapefile format

>> las2iso -i lidar.las -o contours.shp

extracts 10 evenly spaced contours and stores the result in ESRI's
Shapefile format

>> las2iso -i lidar.las -o contours.txt

extracts 10 evenly spaced contours and stores the result in ASCII
format

>> las2iso -i lidar.las -o contours.shp -number 50

extracts 50 evenly spaced contours and stores the result in ESRI's
Shapefile format

>> las2iso -i lidar.las -o contours.shp -every 10 -simplify 0.5

extracts evenly spaced contours every 10 units, simplifies away all
segments that are shorter than 0.5 units, and stores the result in
ESRI's Shapefile format

>> las2iso -i lidar.las -o contours.shp -range 400 800 5 

extracts contours evenly spaced every 5 units from 400 to 800 and
stores the result in ESRI's Shapefile format

>> las2iso -i lidar.las -o contours.shp -last_only -clean 100

extracts 10 contours using only the last returns from the LAS file, 
cleans out all contours whose total length is shorter than 100 units,
and stores the result in ESRI's Shapefile format

>> las2iso -i lidar.las -o tin.shp -every 5 -keep_class 2 -keep_class 3

extracts contours every 5 units using only LAS points that are classified
as 2 or 3 in the LAS file and and stores the result in ESRI's Shapefile
format

for more info:

C:\lastools\bin>las2iso -h
usage:
las2iso -i lidar.las -o contours.shp
las2iso -i lidar.las -o contours.shp -simplify 1 -clean 10
las2iso -i lidar.las -o contours.shp -every 10
las2iso -i lidar.las -o contours.shp -every 2 -simplify 0.5
las2iso -i lidar.las -first_only -o contours.txt
las2iso -last_only -i lidar.las -o contours.shp
las2iso -i lidar.las -keep_class 2 -keep_class 3 -keep_class 9 -otxt > lines.txt
las2iso -i lidar.las -keep_class 8 -o contours.shp
las2iso -h

---------------

if you find bugs let me (isenburg@cs.unc.edu) know.
