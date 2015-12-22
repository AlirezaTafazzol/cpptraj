#!/bin/bash

. ../MasterTest.sh

# Clean
CleanFiles hbond.in nhb.dat avghb.dat solvhb.dat solvavg.dat \
           nbb.dat hbavg.dat solutehb.agr lifehb.gnu avg.lifehb.gnu max.lifehb.gnu \
           crv.lifehb.gnu

INPUT="-i hbond.in"
CheckNetcdf

# Solute-solute, series output, lifetime analysis
cat > hbond.in <<EOF
noprogress
parm ../DPDP.parm7
trajin ../DPDP.nc
hbond HB out nhb.dat avgout avghb.dat
hbond BB out nbb.dat @N,H,C,O series avgout hbavg.dat printatomnum
run
write solutehb.agr BB[solutehb]
runanalysis lifetime BB[solutehb] out lifehb.gnu window 10
EOF
RunCpptraj "Solute Hbond test."
DoTest nhb.dat.save nhb.dat 
DoTest avghb.dat.save avghb.dat
DoTest nbb.dat.save nbb.dat
DoTest hbavg.dat.save hbavg.dat
DoTest solutehb.agr.save solutehb.agr
DoTest lifehb.gnu.save lifehb.gnu
DoTest avg.lifehb.gnu.save avg.lifehb.gnu
DoTest max.lifehb.gnu.save max.lifehb.gnu
DoTest crv.lifehb.gnu.save crv.lifehb.gnu

# Solute-Solvent test
cat > hbond.in <<EOF
parm ../tz2.ortho.parm7
trajin ../tz2.ortho.nc
hbond hb out solvhb.dat avgout solvavg.dat :1-13 solventacceptor :WAT@O solventdonor :WAT \
      solvout solvavg.dat bridgeout solvavg.dat
EOF
RunCpptraj "Solvent Hbond test."
DoTest solvhb.dat.save solvhb.dat
DoTest solvavg.dat.save solvavg.dat

EndTest

exit 0
