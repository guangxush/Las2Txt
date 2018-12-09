****************************************************************

  las2las:

  reads and writes LIDAR data in the LAS format while modifing
  its contents. Examples are clipping of points to those
  that lie within a certain region specified by a bounding box
  (-clip) or points that are between a certain height (-clip_z
  or clip_elev), or eliminating points that are the second return
  (-elim_return 2), that have a scan angle above some threshold
  (-elim_scan_angle_above 5), or below some intensity
  (-elim_intensity_below 15)
  Another typical use may be to extract only first (-first_only)
  returns or only last returns (-last_only). Extracting the first
  return is actually the same as eliminating all others (e.g.
  -elim_return 2 -elim_return 3, ...).
  Finally one can also only keep certain classifications. The
  option -keep_class 2 -keep_class 3 will eliminate all points
  except those that are of classification 2 or 3.
 
****************************************************************

example usage:

>> las2las -i in.las -verbose

lists the value ranges of all the points (similar to lasinfo.exe)

>> las2las -i in.las -o out.las -clip 630250 4834500 630500 4834750

clips points of in.las whose double precision coordinates fall outside
the box (630250,4834500) to (630500,4834750) and stores surviving points
to out.las. note that '-clip' expects the box to be specified in the
scaled and offset double-precision coordinates (and not the point.x
and point.y values that the LAS points are stored with internally).

>> las2las -i in.las -o out.las -clip_z 10 100

clips points of in.las whose double precision elevations falls outside
the range 10 to 100 and stores surviving points to out.las. note that
'-clip_z' expects the box to be specified in the scaled and offset
double-precision elevation (and not the point.z value that the LAS
points are stored with internally).

>> las2las -i in.las -o out.las -eliminate_return 1

eliminates all points of in.las that are designated first returns
by the value in their return_number field and stores surviving
points to out.las.

>> las2las -i in.las -o out.las -eliminate_scan_angle_above 15

eliminates all points of in.las whose scan angle is above 15 or
below -15 and stores surviving points to out.las.

>> las2las -i in.las -o out.las -eliminate_intensity_below 1000 -remove_extra_header

eliminates all points of in.las whose intensity is below 1000 and
stores surviving points to out.las. in addition all variable headers
and any additional user headers are stripped from the file.

>> las2las -i in.las -o out.las -last_only

extracts all last return points from in.las and stores them to
out.las.

>> las2las -i in.las -o out.las -scale_rgb_up

multiplies all rgb values in the file with 256. this is used to scale
the rgb values from standard unsigned char range (0 ... 255) to the
unsigned short range (0 ... 65535) used in the LAS format.

>> las2las -i in.las -o out.las -scale_rgb_down

does the opposite

>> las2las -i in.las -o out.las -keep_class 2 -keep_class 3

extracts all points classfied as 2 or 3 from in.las and stores
them to out.las.

>> las2las -i in.las -o out.las -clip_int 63025000 483450000 63050000 483475000

similar to '-clip' but uses the integer values point.x and point.y
that the points are stored with for clipping (and not the double
precision floating point coordinates they represent). clips all the
points of in.lasthat have point.x<63025000 or point.y<483450000 or
point.x>63050000 or point.y>483475000 and stores surviving points to
out.las. (use lasinfo.exe to see the range of point.x and point.y).

>> las2las -i in.las -o out.las -clip_int_z 1000 4000

similar to '-clip_z' but uses the integer values point.z that the
points are stored with for clipping (and not the double-precision
floating point coordinates they represent). clips all the points
of in.las that have point.z<1000 or point.z>4000 and stores all
surviving points to out.las. (use lasinfo.exe to see the range of
point.z).

for more info:

C:\lastools\bin>las2las -h
usage:
las2las -remove_extra_header in.las out.las
las2las -i in.las -clip 63000000 483450000 63050000 483500000 -o out.las
las2las -i in.las -eliminate_return 2 -o out.las
las2las -i in.las -eliminate_scan_angle_above 15 -o out.las
las2las -i in.las -eliminate_intensity_below 1000 -olas > out.las
las2las -i in.las -first_only -clip 63000000 483450000 63050000 483500000 -o out.las
las2las -i in.las -last_only -eliminate_intensity_below 2000 -olas > out.las
las2las -i in.las -keep_class 2 -keep_class 3 -keep_class 4 -olas > out.las
las2las -h

---------------

if you find bugs let me (isenburg@cs.unc.edu) know.
