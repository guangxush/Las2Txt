****************************************************************

  lasthin:

  Implements a simple LIDAR point thinning algorithm. It puts
  a uniform grid over the points and within each grid cell keeps
  only the point with the lowest Z coordinate. Optionally you
  can request to only consider last returns (-last_only) or a
  particular classification (-keep_classification 2) before doing
  the thinning. The grid spacing default is 1 unit and it is set
  with -grid_spacing 0.5
 
****************************************************************

example usage:

>> lasthin -i in.las -o out.las

does LAS thinning with the grid spacing default of 1 unit

>> lasthin -i in.las -grid_spacing 0.5 -o out.las

does point thinning with a grid spacing of 0.5 units

>> lasthin -i in.las -o out.las -last_only

does LIDAR thinning with a grid spacing of 1 unit but only
using points that are last returns

>> lasthin -i in.las -o out.las -keep_class 2 -keep_class 3

looks only at the points classfied as 2 or 3 from in.las and
thins them with a grid spacing of 1 unit 

for more info:

C:\lastools\bin>lasthin -h
usage:
lasthin in.las out.las
lasthin -i in.las -grid_spacing 0.5 -o out.las
lasthin -i in.las -last_only -grid_spacing 1.0 -o out.las
lasthin -i in.las -keep_class 2 -keep_class 3 -keep_class 4 -grid_spacing 0.5 -olas > out.las
lasthin -h

---------------

if you find bugs let me (isenburg@cs.unc.edu) know.
