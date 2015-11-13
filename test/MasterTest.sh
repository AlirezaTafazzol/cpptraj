# This should be sourced at the top of CPPTRAJ test run scripts.

# MasterTest.sh command line options
CLEAN=0             # If 1, only file cleaning needs to be performed.
SUMMARY=0           # If 1, only summary of results needs to be performed.
STANDALONE=0        # If 0, part of AmberTools. If 1, stand-alone (e.g. from GitHub).
PROFILE=0           # If 1, end of test profiling with gprof performed
FORCE_AMBERTOOLS=0  # FIXME: currently needed to get extended tests to work
CPPTRAJ=""          # CPPTRAJ binary
SFX=""              # CPPTRAJ binary suffix
AMBPDB=""           # ambpdb binary
TIME=""             # Set to the 'time' command if timing requested.
VALGRIND=""         # Set to 'valgrind' command if memory check requested.
DIFFCMD=""          # Command used to check for test differences
REMOVE="/bin/rm -f" # Remove command
NCDUMP=""           # ncdump command; needed for NcTest()
OUTPUT="test.out"   # File to direct test STDOUT to.
ERROR="/dev/stderr" # File to direct test STDERR to.
TEST_RESULTS=""     # For standalone, file to record test results to.
TEST_ERROR=""       # For standalone, file to record test errors to.
DEBUG=""            # Can be set to pass global debug flag to CPPTRAJ.
NUMTEST=0           # Total number of times DoTest has been called this test.
ERRCOUNT=0          # Total number of errors detected by DoTest this test.

# Options used in tests
TOP=""   # CPPTRAJ topology file/command line arg
INPUT="" # CPPTRAJ input file/command line arg

# Variables that describe how CPPTRAJ was compiled
ZLIB=""
BZLIB=""
NETCDFLIB=""
MPILIB=""
NOMATHLIB=""
OPENMP=""
PNETCDFLIB=""

# ------------------------------------------------------------------------------
# DoTest(): Compare File1 to File2, print an error if they differ.
#           Args 3 and 4 can be used to pass an option to diff
DoTest() {
  if [[ $STANDALONE -eq 0 ]] ; then
    # AmberTools - will use dacdif.
    $DIFFCMD $1 $2
  else
    # Standalone - will use diff.
    ((NUMTEST++))
    if [[ ! -f "$1" ]] ; then
      echo "  $1 not found." >> $TEST_RESULTS
      echo "  $1 not found." >> $TEST_ERROR
      ((ERRCOUNT++))
    elif [[ ! -f "$2" ]] ; then
      echo "  $2 not found." >> $TEST_RESULTS
      echo "  $2 not found." >> $TEST_ERROR
      ((ERRCOUNT++))
    else
      $DIFFCMD $1 $2 $3 $4 > temp.diff 2>&1
      if [[ -s temp.diff ]] ; then
        echo "  $1 $2 are different." >> $TEST_RESULTS
        echo "  $1 $2 are different." >> $TEST_ERROR
        cat temp.diff >> $TEST_ERROR
        ((ERRCOUNT++))
      else
        echo "  $2 OK." >> $TEST_RESULTS
      fi
      $REMOVE temp.diff
    fi
  fi
}

# ------------------------------------------------------------------------------
# NcTest(): Compare NetCDF files <1> and <2>. Use NCDUMP to convert to ASCII
# first, removing ==> line and :programVersion attribute.
NcTest() {
  if [[ -z $1 || -z $2 ]] ; then
    echo "Error: NcTest(): One or both files not specified." > /dev/stderr
    exit 1
  fi
  if [[ -z $NCDUMP || ! -e $NCDUMP ]] ; then
    echo "ncdump missing." > /dev/stderr
    exit 1
  fi
  # Prepare files.
  if [[ ! -e $1 ]] ; then
    echo "Error: $1 missing." >> $TEST_ERROR
  elif [[ ! -e $2 ]] ; then
    echo "Error: $2 missing." >> $TEST_ERROR
  else
    $NCDUMP -n nctest $1 | grep -v "==>\|:programVersion" > nc0
    $NCDUMP -n nctest $2 | grep -v "==>\|:programVersion" > nc1
    DoTest nc0 nc1
    $REMOVE nc0 nc1
  fi
}

# ------------------------------------------------------------------------------
# CheckTest(): Report if the error counter is greater than 0. TODO Remove
CheckTest() {
  # Only use when STANDALONE 
  if [[ $STANDALONE -eq 1 ]] ; then
    if [[ $ERR -gt 0 ]] ; then
      echo "  $ERR comparisons failed so far."
    fi
  fi
}

# ------------------------------------------------------------------------------
# RunCpptraj(): Run cpptraj with the given options.
RunCpptraj() {
  # If only cleaning requested no run needed, exit now
  if [[ $CLEAN -eq 1 ]] ; then
    exit 0
  fi
  echo ""
  echo "  CPPTRAJ: $1"
  if [[ $STANDALONE -eq 1 ]] ; then
    echo "  CPPTRAJ: $1" >> $TEST_RESULTS
  fi
  if [[ ! -z $DEBUG ]] ; then
    echo "$TIME $DO_PARALLEL $VALGRIND $CPPTRAJ $DEBUG $TOP $INPUT >> $OUTPUT 2>>$ERROR"
  fi
  $TIME $DO_PARALLEL $VALGRIND $CPPTRAJ $DEBUG $TOP $INPUT >> $OUTPUT 2>>$ERROR
}

# ------------------------------------------------------------------------------
# EndTest(): Called at the end of every test script if no errors found.
EndTest() {
  # Report only when standalone
  if [[ $STANDALONE -eq 1 ]] ; then
    if [[ $ERRCOUNT -gt 0 ]] ; then
      echo "  $ERRCOUNT out of $NUMTEST comparisons failed."
      echo "  $ERRCOUNT out of $NUMTEST comparisons failed." >> $TEST_RESULTS
      echo "  $ERRCOUNT out of $NUMTEST comparisons failed." >> $TEST_ERROR
    else 
      echo "All $NUMTEST comparisons passed." 
      echo "All $NUMTEST comparisons passed." >> $TEST_RESULTS 
    fi
    echo ""
    if [[ ! -z $VALGRIND ]] ; then
      echo "Valgrind summary:"
      grep ERROR $ERROR
      grep heap $ERROR
      grep LEAK $ERROR
      echo ""
      echo "Valgrind summary:" >> $TEST_RESULTS
      grep ERROR $ERROR >> $TEST_RESULTS
      grep heap $ERROR >> $TEST_RESULTS
      grep LEAK $ERROR >> $TEST_RESULTS

    fi
    if [[ $PROFILE -eq 1 ]] ; then
      if [[ -e gmon.out ]] ; then
        gprof $CPPTRAJ > profiledata.txt
      fi
    fi
  fi
}

# ------------------------------------------------------------------------------
# CleanFiles(): For every arg passed to the function, check for the file and rm it
CleanFiles() {
  while [[ ! -z $1 ]] ; do
    if [[ -e $1 ]] ; then
      $REMOVE $1
    fi
    shift
  done
  # If only cleaning requested no run needed, exit now
  if [[ $CLEAN -eq 1 ]] ; then
    exit 0
  fi
}

# ------------------------------------------------------------------------------
# Library Checks - Tests that depend on certain libraries like Zlib can run
# these to make sure cpptraj was compiled with that library - exit gracefully
# if not.
# Should not be called if CLEAN==1, CleanFiles should always be called first.
CheckZlib() {
  if [[ -z $ZLIB ]] ; then
    echo "This test requires zlib. Cpptraj was compiled without zlib support."
    echo "Skipping test."
    exit 0
  fi
}

CheckBzlib() {
  if [[ -z $BZLIB ]] ; then
    echo "This test requires bzlib. Cpptraj was compiled without bzlib support."
    echo "Skipping test."
    exit 0
  fi
}

CheckNetcdf() {
  if [[ -z $NETCDFLIB ]] ; then
    echo "This test requires Netcdf. Cpptraj was compiled without Netcdf support."
    echo "Skipping test."
    exit 0
  fi
}

CheckPtrajAnalyze() {
  if [[ ! -z $NOMATHLIB ]] ; then
    echo "This test requires LAPACK/ARPACK/BLAS routines from AmberTools."
    echo "Cpptraj was compiled with -DNO_MATHLIB. Skipping test."
    exit 0
  fi
}

#-------------------------------------------------------------------------------
# Summary(): Print a summary of the tests.
Summary() {
  RESULTFILES=""
  if [[ ! -z $TEST_RESULTS ]] ; then
    RESULTFILES=`ls */$TEST_RESULTS 2> /dev/null`
  else
    exit 0
  fi
  echo "===================== TEST SUMMARY ======================"
  if [[ ! -z $RESULTFILES ]] ; then
    cat $RESULTFILES > $TEST_RESULTS
    # DoTest - Number of comparisons OK
    OK=`cat $TEST_RESULTS | grep OK | wc -l`
    # DoTest - Number of comparisons different
    ERR=`cat $TEST_RESULTS | grep different | wc -l`
    NOTFOUND=`cat $TEST_RESULTS | grep "not found" | wc -l`
    ((ERR = $ERR + $NOTFOUND))
    # Number of tests run
    NTESTS=`cat $TEST_RESULTS | grep "TEST:" | wc -l`
    # Number of tests successfully finished
    PASSED=`cat $TEST_RESULTS | grep "comparisons passed" | wc -l`
    ((NCOMPS = $OK + $ERR))
    echo "  $OK out of $NCOMPS comparisons OK ($ERR failed)."
    echo "  $PASSED out of $NTESTS tests completed with no issues."
    RESULTFILES=`ls */$TEST_ERROR 2> /dev/null`
    if [[ ! -z $RESULTFILES ]] ; then
      cat $RESULTFILES > $TEST_ERROR
    fi 
  else
    echo "No Test Results files (./*/$TEST_RESULTS) found."
  fi

  if [[ $ERR -gt 0 ]]; then
    echo "Obtained the following errors:"
    echo "---------------------------------------------------------"
    cat $TEST_ERROR
    echo "---------------------------------------------------------"
  fi

  if [[ ! -z $VALGRIND ]] ; then
    RESULTFILES=`ls */$ERROR 2> /dev/null`
    if [[ ! -z $RESULTFILES ]] ; then
      echo "---------------------------------------------------------"
      echo "Valgrind summary:"
      NUMVGERR=`cat $RESULTFILES | grep ERROR | awk 'BEGIN{sum=0;}{sum+=$4;}END{print sum;}'`
      echo "    $NUMVGERR errors."
      NUMVGOK=`cat $RESULTFILES | grep "All heap" | wc -l`
      echo "    $NUMVGOK memory leak checks OK."
      NUMVGLEAK=`cat $RESULTFILES | grep LEAK | wc -l`
      echo "    $NUMVGLEAK memory leak reports."
    else
      echo "No valgrind test results found."
      exit $ERR
    fi
  fi
  echo "========================================================="
  exit $ERR
}

#-------------------------------------------------------------------------------
# Help(): Print help
Help() {
  echo "Command line flags:"
  echo "  summary    : Print summary of test results only."
  echo "  stdout     : Print CPPTRAJ test output to STDOUT."
  echo "  mpi        : Use MPI version of CPPTRAJ."
  echo "  openmp     : Use OpenMP version of CPPTRAJ."
  echo "  vg         : Run test with valgrind memcheck."
  echo "  vgh        : Run test with valgrind helgrind."
  echo "  time       : Time the test."
  echo "  -at        : Force AmberTools tests."
  echo "  -d         : Run CPPTRAJ with global debug level 4."
  echo "  -debug <#> : Run CPPTRAJ with global debug level #."
  echo "  -cpptraj <file> : Use CPPTRAJ binary <file>."
  echo "  -ambpdb <file>  : Use AMBPDB binary <file>."
  echo "  -profile        : Profile results with 'gprof' (requires special compile)."
}

#-------------------------------------------------------------------------------
# CmdLineOpts(): Process test script command line options
CmdLineOpts() {
  VGMODE=0 # Valgrind mode: 0 none, 1 memcheck, 2 helgrind
  while [[ ! -z $1 ]] ; do
    case "$1" in
      "summary"  ) SUMMARY=1 ;;
      "stdout"   ) OUTPUT="/dev/stdout" ;;
      "mpi"      ) SFX=".MPI" ;;
      "openmp"   ) SFX=".OMP" ;;
      "vg"       ) VGMODE=1 ;;
      "vgh"      ) VGMODE=2 ;;
      "time"     ) TIME=`which time` ;;
      "-at"      ) FORCE_AMBERTOOLS=1 ;;
      "-d"       ) DEBUG="-debug 4" ;;
      "-debug"   ) shift ; DEBUG="-debug $1" ;;
      "-cpptraj" ) shift ; CPPTRAJ=$1 ; echo "Using cpptraj: $CPPTRAJ" ;;
      "-ambpdb"  ) shift ; AMBPDB=$1  ; echo "Using ambpdb: $AMBPDB" ;;
      "-profile" ) PROFILE=1 ; echo "Performing gnu profiling during EndTest." ;;
      "-h" | "--help" ) Help ; exit 0 ;;
      *          ) echo "Error: Unknown opt: $1" > /dev/stderr ; exit 1 ;;
    esac
    shift
  done
  # Set up valgrind if necessary
  if [[ $VGMODE -ne 0 ]] ; then
    VG=`which valgrind`
    if [[ -z $VG ]] ; then
      echo "Error: Valgrind not found." > /dev/stderr
      echo "Error:    Make sure valgrind is installed and in your PATH" > /dev/stderr
      exit 1
    fi
    echo "  Using Valgrind."
    ERROR="valgrind.out"
    if [[ $VGMODE -eq 1 ]] ; then
      VALGRIND="valgrind --tool=memcheck --leak-check=yes --show-reachable=yes"
    elif [[ $VGMODE -eq 2 ]] ; then
      VALGRIND="valgrind --tool=helgrind"
    fi
  fi
  # If DO_PARALLEL has been set force MPI
  if [[ ! -z $DO_PARALLEL ]] ; then
    SFX=".MPI"
    MPI=1
  fi
  # Set default command locations
  DIFFCMD=`which diff`
  NCDUMP=`which ncdump`
  # Figure out if we are a part of AmberTools
  if [[ -z $CPPTRAJ ]] ; then
    if [[ ! -z `pwd | grep AmberTools` || $FORCE_AMBERTOOLS -eq 1 ]] ; then
      STANDALONE=0
    else
      STANDALONE=1
    fi
  else
    # CPPTRAJ was specified. Assume standalone.
    STANDALONE=1
  fi
}

#-------------------------------------------------------------------------------
# SetBinaries(): Set and check CPPTRAJ etc binaries
SetBinaries() {
  # Set default command locations
  DIFFCMD=`which diff`
  NCDUMP=`which ncdump`
  # Set CPPTRAJ binary location if not already set.
  if [[ -z $CPPTRAJ ]] ; then
    if [[ $STANDALONE -eq 0 ]] ; then
      # AmberTools - use dacdif for comparison
      if [[ -z $AMBERHOME ]] ; then
        echo "Warning: AMBERHOME is not set."
        # Assume we are running in $AMBERHOME/AmberTools/src/test/Test_X
        DIRPREFIX=../../../../
      else
        DIRPREFIX=$AMBERHOME
      fi
      DIFFCMD=$DIRPREFIX/test/dacdif
      NCDUMP=$DIRPREFIX/bin/ncdump
      CPPTRAJ=$DIRPREFIX/bin/cpptraj$SFX
      AMBPDB=$DIRPREFIX/bin/ambpdb
    else
      # Standalone: GitHub etc
      if [[ ! -z $CPPTRAJHOME ]] ; then
        CPPTRAJ=$CPPTRAJHOME/bin/cpptraj$SFX
        AMBPDB=$CPPTRAJHOME/bin/ambpdb
      else
        CPPTRAJ=../../bin/cpptraj$SFX
        AMBPDB=../../bin/ambpdb
      fi
    fi
  fi
  # Print DEBUG info
  if [[ ! -z $DEBUG ]] ; then
    if [[ $STANDALONE -eq 1 ]] ; then
      echo "DEBUG: Standalone mode."
    else
      echo "DEBUG: AmberTools mode."
    fi
    echo "DEBUG: CPPTRAJ: $CPPTRAJ"
    echo "DEBUG: AMBPDB:  $AMBPDB"
    echo "DEBUG: NCDUMP:  $NCDUMP"
    echo "DEBUG: DIFFCMD: $DIFFCMD"
  fi
  # Check binaries
  if [[ ! -f "$NCDUMP" ]] ; then
    echo "Warning: 'ncdump' not found; NetCDF file comparisons cannot be performed."
  fi
  if [[ ! -f "$DIFFCMD" ]] ; then
    echo "Error: diff command '$DIFFCMD' not found." > /dev/stderr
    exit 1
  fi
  if [[ ! -f "$CPPTRAJ" ]] ; then
    echo "Error: cpptraj binary '$CPPTRAJ' not found." > /dev/stderr
    exit 1
  fi
  if [[ ! -z $DEBUG || $STANDALONE -eq 1 ]] ; then
    ls -l $CPPTRAJ
  fi
  if [[ ! -f "$AMBPDB" ]] ; then
    # Try to locate it based on the location of CPPTRAJ
    DIRPREFIX=`dirname $CPPTRAJ`
    AMBPDB=$DIRPREFIX/ambpdb
    if [[ ! -f "$AMBPDB" ]] ; then
      echo "Warning: ambpdb binary '$AMBPDB' not found."
    fi
  fi
}

#-------------------------------------------------------------------------------
# CheckDefines(): Check how CPPTRAJ was compiled.
CheckDefines() {
  DEFINES=`$CPPTRAJ --defines | grep Compiled`
  ZLIB=`echo $DEFINES | grep DHASGZ`
  BZLIB=`echo $DEFINES | grep DHASBZ2`
  NETCDFLIB=`echo $DEFINES | grep DBINTRAJ`
  MPILIB=`echo $DEFINES | grep DMPI`
  NOMATHLIB=`echo $DEFINES | grep DNO_MATHLIB`
  OPENMP=`echo $DEFINES | grep D_OPENMP`
  PNETCDFLIB=`echo $DEFINES | grep DHAS_PNETCDF`
}

#===============================================================================
# If the first argument is "clean" then no set-up is required. Script will
# exit when either CleanFiles or RunCpptraj is called from sourcing script.
if [[ $1 = "clean" ]] ; then
  CLEAN=1
else
  CmdLineOpts $*
  # Set results files for STANDALONE
  if [[ $STANDALONE -eq 1 ]] ; then
    TEST_RESULTS=Test_Results.dat
    TEST_ERROR=Test_Error.dat
    if [[ -f "$TEST_RESULTS" ]] ; then
      $REMOVE $TEST_RESULTS
    fi
    if [[ -f "$TEST_ERROR" ]] ; then
      $REMOVE $TEST_ERROR
    fi
  fi
  # Only a summary of previous results has been requested.
  if [[ $SUMMARY -eq 1 ]] ; then
    Summary
  fi
  # Set binary locations
  SetBinaries
  # Check how CPPTRAJ was compiled
  CheckDefines
  # Start test results file
  echo "**************************************************************"
  echo "TEST: `pwd`"
  if [[ $STANDALONE -eq 1 ]] ; then
    echo "**************************************************************" > $TEST_RESULTS
    echo "TEST: `pwd`" >> $TEST_RESULTS
  fi
fi
# Always clean up OUTPUT and ERROR
if [[ $OUTPUT != "/dev/stdout" && -f "$OUTPUT" ]] ; then
  $REMOVE $OUTPUT
fi
if [[ $ERROR != "/dev/stderr" && -f "$ERROR" ]] ; then
  $REMOVE $ERROR
fi
