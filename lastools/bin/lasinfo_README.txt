****************************************************************

  LASinfo : simply prints the header contents and a short
            summary of the points. when there are differences
            between header info and point content they are
            reported as a warning.

            can also be used to modify entries in the header
            as described below

****************************************************************

example usage:

>> lasinfo lidar.las

reports all information 

>> lasinfo -o lidar_info.txt -i lidar.las

reports all information to a text file called lidar_info.txt

>> lasinfo -i lidar.las -no_variable

avoids reporting the contents of the variable length records

>> lasinfo -i lidar.las -nocheck

omits reading over all the points. only reports header information

>> lasinfo -i lidar.las -repair

if there are missing or wrong entries in the header they are corrected

>> lasinfo -i lidar.las -auto_date

sets the file creation day/year in the header based on the creation date of the file

>> lasinfo -i lidar.las -repair_boundingbox

reads all points, computes their bounding box, and updates it in the header

>> lasinfo -i lidar.las -file_creation 8 2007

sets the file creation day/year in the header to 8/2007

>> lasinfo -i lidar.las -system_identifier "hello world!"

copies the first 31 characters of the string into the system_identifier field of the header 

>> lasinfo -i lidar.las -version 1.1 -quiet

changes the version of the lidar file to be 1.1 while suppressing all control output

>> lasinfo -i lidar.las -generating_software "this is a test (-:"

copies the first 31 characters of the string into the generating_software field of the header 

for more info:

C:\lastools\bin>lasinfo -h
usage:
lasinfo lidar.las
lasinfo -no_variable lidar.las
lasinfo -no_variable -nocheck lidar.las
lasinfo -i lidar.las -o lidar_info.txt
lasinfo -i lidar.las -repair
lasinfo -i lidar.las -repair_bounding_box -file_creation 8 2007
lasinfo -i lidar.las -system_identifier "hello world!" -generating_software "this is a test (-:"

----

try these sources for sample lidar data in LAS format:

http://www.geoict.net/LidarData/Data/OptechSampleLAS.zip
http://www.qcoherent.com/data/LP360_Sample_Data.rar
http://www.appliedimagery.com/Serpent%20Mound%20Model%20LAS%20Data.las

if you find bugs let me (isenburg@cs.unc.edu) know.
