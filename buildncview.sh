#!/bin/bash
./configure --x-includes=/scratch/PI/qying/lib/X11/usr/include --x-libraries=/scratch/PI/qying/lib/X11/usr/lib64  --with-netcdf_incdir=/scratch/PI/qying/lib/netcdf-4.7.4-intel/include --with-netcdf_libdir=/scratch/PI/qying/lib/netcdf-4.7.4-intel/lib
make
