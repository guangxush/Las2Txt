****************************************************************

  LASview : a simple OpenGL-based viewer for LAS files that
		  can also compute and display a TIN

****************************************************************

example usage:

>> lasview lidar.las
>> lasview -i lidar.las

reads around 1000000 subsampled lidar points and displays in 50 steps

>> lasview -i lidar.las -win 1600 1200

same as above but with a larger display window

>> lasview -i lidar.las -steps 10 -points 200000

reads around 200000 subsampled lidar points and displays in 11 steps

interactive options:

<t>     compute a TIN from the displayed returns
<h>     change shading mode for TIN (hill-shade, elevation, wire-frame)

<a>     display all returns
<l>     display last returns only
<f>     display first returns only
<g>     display returns classified as ground
<b>     display returns classified as building
<v>     display returns classified as vegetation
<o>     display returns classified as overlap
<w>     display returns classified as water
<u>     display returns that are unclassified

<space> switch between rotate/translate/zoom
<-/=>   render points smaller/bigger
<[/]>   exaggerate/mellow elevation differences
<c>     change color mode
<B>     hide/show bounding box
<s/S>   step forward/backward
<z/Z>   tiny step forward/backward
<r>     out-of-core full resolution rendering

----

try these sources for sample lidar data in LAS format:

http://www.geoict.net/LidarData/Data/OptechSampleLAS.zip
http://www.qcoherent.com/data/LP360_Sample_Data.rar
http://www.appliedimagery.com/Serpent%20Mound%20Model%20LAS%20Data.las

if you find bugs let me (isenburg@cs.unc.edu) know.
