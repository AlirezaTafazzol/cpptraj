#include <cstdio> // stdin, fileno
#ifdef _WIN32
	#include <io.h>
	#define isatty _isatty
#else
	#include <unistd.h> // isatty
#endif
#include "Cpptraj.h"
#include "CpptrajStdio.h"
#include "Command.h"
#include "ReadLine.h"
#include "Version.h"
#include "ParmFile.h" // ProcessMask
#include "TopInfo.h" // ProcessMask
#include "Timer.h"
#include "StringRoutines.h" // TimeString
#ifdef CUDA
# include <cuda_runtime_api.h>
#endif

/// CONSTRUCTOR - initializes all commands
Cpptraj::Cpptraj() {
# ifdef _MSC_VER
  // To make sure output format on windows matches C specification, force
  // 2 digit exponents when the compiler version number is less than 1900
  // (Visual Studio 2015).
# if _MSC_VER < 1900
  _set_output_format(_TWO_DIGIT_EXPONENT);
# endif
# endif
  Command::Init();
}

/// DESTRUCTOR - free all commands
Cpptraj::~Cpptraj() { Command::Free(); }

void Cpptraj::Usage() {
  mprinterr("\n"
            "Usage: cpptraj [-p <Top0>] [-i <Input0>] [-y <trajin>] [-x <trajout>]\n"
            "               [-c <reference>] [-d <datain>] [-w <dataout>] [-o <output>]\n"
            "               [-h | --help] [-V | --version] [--defines] [-debug <#>]\n"
            "               [--interactive] [--log <logfile>] [-tl]\n"
            "               [-ms <mask>] [-mr <mask>] [--mask <mask>] [--resmask <mask>]\n"
            "       cpptraj <Top> <Input>\n"
            "\t-p <Top0>        : Load <Top0> as a topology file. May be specified more than once.\n"
            "\t-i <Input0>      : Read input from <Input0>. May be specified more than once.\n"
            "\t-y <trajin>      : Read from trajectory file <trajin>; same as input 'trajin <trajin>'.\n"
            "\t-x <trajout>     : Write trajectory file <trajout>; same as input 'trajout <trajout>'.\n"
            "\t-c <reference>   : Read <reference> as reference coordinates; same as input 'reference <reference>'.\n"
            "\t-d <datain>      : Read data in from file <datain> ('readdata <datain>').\n"
            "\t-w <dataout>     : Write data from <datain> as file <dataout> ('writedata <dataout>).\n"
            "\t-o <output>      : Write CPPTRAJ STDOUT output to file <output>.\n"
            "\t-h | --help      : Print command line help and exit.\n"
            "\t-V | --version   : Print version and exit.\n"
            "\t--defines        : Print compiler defines and exit.\n"
            "\t-debug <#>       : Set global debug level to <#>; same as input 'debug <#>'.\n"
            "\t--interactive    : Force interactive mode.\n"
            "\t--log <logfile>  : Record commands to <logfile> (interactive mode only). Default is 'cpptraj.log'.\n"
            "\t-tl              : Print length of trajectories specified with '-y' to STDOUT.\n"
            "\t-ms <mask>       : Print selected atom numbers to STDOUT.\n"
            "\t-mr <mask>       : Print selected residue numbers to STDOUT.\n"
            "\t--mask <mask>    : Print detailed atom selection to STDOUT.\n"
            "\t--resmask <mask> : Print detailed residue selection to STDOUT.\n\n");
}

void Cpptraj::Intro() {
  mprintf("\nCPPTRAJ: Trajectory Analysis. %s"
# ifdef MPI
          " MPI"
# endif
# ifdef _OPENMP
          " OpenMP"
# endif
# ifdef CUDA
          " CUDA"
# endif
          "\n    ___  ___  ___  ___\n     | \\/ | \\/ | \\/ | \n    _|_/\\_|_/\\_|_/\\_|_\n\n",
          CPPTRAJ_VERSION_STRING);
# ifdef MPI
  mprintf("| Running on %i threads\n", Parallel::World().Size());
# endif
  mprintf("| Date/time: %s\n", TimeString().c_str());
  std::string available_mem = AvailableMemoryStr();
  // If empty, available mem could not be calculated correctly.
  if (!available_mem.empty())
    mprintf(  "| Available memory: %s\n", available_mem.c_str());
# ifdef CUDA
  int device;
  if ( cudaGetDevice( &device ) == cudaSuccess ) {
    cudaDeviceProp deviceProp;
    if ( cudaGetDeviceProperties( &deviceProp, device ) == cudaSuccess ) {
      mprintf("| CUDA device: %s\n", deviceProp.name);
      mprintf("| Available GPU memory: %s\n",
              ByteString(deviceProp.totalGlobalMem, BYTE_DECIMAL).c_str());
    }
  }
# endif
  mprintf("\n");
}

void Cpptraj::Finalize() {
  mprintf("--------------------------------------------------------------------------------\n"
    "To cite CPPTRAJ use:\n"
    "Daniel R. Roe and Thomas E. Cheatham, III, \"PTRAJ and CPPTRAJ: Software for\n"
    "  Processing and Analysis of Molecular Dynamics Trajectory Data\". J. Chem.\n"
    "  Theory Comput., 2013, 9 (7), pp 3084-3095.\n");
}

/** Main routine for running cpptraj. */
int Cpptraj::RunCpptraj(int argc, char** argv) {
  int err = 0;
  Timer total_time;
  total_time.Start();
# ifdef CUDA
  int nGPUs = 0;
  cudaError_t cerr = cudaGetDeviceCount( &nGPUs );
  if ( cerr == cudaErrorNoDevice )
    mprinterr("Error: No CUDA-capable devices present.\n");
  else if ( cerr == cudaErrorInsufficientDriver )
    mprinterr("Error: NVIDIA driver version is insufficient for this version of CUDA.\n");
  if (nGPUs < 1 || cerr != cudaSuccess) {
    mprinterr("Error: No CUDA-capable devices found.\n");
    return 1;
  }
# endif
  Mode cmode = ProcessCmdLineArgs(argc, argv);
  if ( cmode == BATCH ) {
    // If State is not empty, run now.
    if (!State_.EmptyState())
      err = State_.Run();
  } else if ( cmode == INTERACTIVE ) {
#   ifdef MPI
    mprinterr("Error: MPI version of cpptraj cannot run in interactive mode.\n");
    err = 1;
#   else
    err = Interactive();
#   endif
  } else if ( cmode == ERROR ) {
    err = 1;
  }
  // Ensure all data has been written.
  if (State_.DFL().UnwrittenData())
    State_.DFL().WriteAllDF();
  total_time.Stop();
  if (cmode != INTERACTIVE)
    mprintf("TIME: Total execution time: %.4f seconds.\n", total_time.Total());
  if (err == 0)
    Cpptraj::Finalize();
  else
    mprinterr("Error: Error(s) occurred during execution.\n");
  mprintf("\n");
  FinalizeIO();
  return err;
}

/** \return string containing preprocessor defines used to compile cpptraj. */
std::string Cpptraj::Defines() {
    std::string defined_str ("");
#ifdef DEBUG
  defined_str.append(" -DDEBUG");
#endif
#ifdef HASBZ2
  defined_str.append(" -DHASBZ2");
#endif
#ifdef HASGZ
  defined_str.append(" -DHASGZ");
#endif
#ifdef BINTRAJ
  defined_str.append(" -DBINTRAJ");
#endif
#ifdef MPI
  defined_str.append(" -DMPI");
#endif
#ifdef _OPENMP
  defined_str.append(" -D_OPENMP");
#endif
#ifdef CUDA
  defined_str.append(" -DCUDA"); //TODO SHADER_MODEL?
#endif
#ifdef NO_MATHLIB
  defined_str.append(" -DNO_MATHLIB");
#endif
#ifdef NO_ARPACK
  defined_str.append(" -DNO_ARPACK");
#endif
#ifdef TIMER
  defined_str.append(" -DTIMER");
#endif
#ifdef ENABLE_SINGLE_ENSEMBLE
  defined_str.append(" -DENABLE_SINGLE_ENSEMBLE");
#endif
#ifdef HAS_PNETCDF
  defined_str.append(" -DHAS_PNETCDF");
#endif
#ifdef NO_XDRFILE
  defined_str.append(" -DNO_XDRFILE");
#endif
#if defined(USE_SANDERLIB) && !defined(LIBCPPTRAJ)
  defined_str.append(" -DUSE_SANDERLIB");
#endif
#ifdef LIBPME
  defined_str.append(" -DLIBPME");
#endif
  return defined_str;
}

/** Process a mask from the command line. */
int Cpptraj::ProcessMask( Sarray const& topFiles, Sarray const& refFiles,
                          std::string const& maskexpr,
                          bool verbose, bool residue ) const
{
  SetWorldSilent(true);
  if (topFiles.empty()) {
    mprinterr("Error: No topology file specified.\n");
    return 1;
  }
  ParmFile pfile;
  Topology parm;
  if (pfile.ReadTopology(parm, topFiles[0], State_.Debug())) return 1;
  if (!refFiles.empty()) {
    DataSet_Coords_REF refCoords;
    if (refCoords.LoadRefFromFile( refFiles[0], parm, State_.Debug())) return 1;
    parm.SetDistMaskRef( refCoords.RefFrame() );
  }
  if (!verbose) {
    AtomMask tempMask( maskexpr );
    if (parm.SetupIntegerMask( tempMask )) return 1;
    loudPrintf("Selected=");
    if (residue) {
      int res = -1;
      for (AtomMask::const_iterator atom = tempMask.begin();
                                    atom != tempMask.end(); ++atom)
      {
        if (parm[*atom].ResNum() > res) {
          loudPrintf(" %i", parm[*atom].ResNum()+1);
          res = parm[*atom].ResNum();
        }
      }
    } else
      for (AtomMask::const_iterator atom = tempMask.begin();
                                    atom != tempMask.end(); ++atom)
        loudPrintf(" %i", *atom + 1);
    loudPrintf("\n");
  } else {
    TopInfo info(&parm);
    if (residue)
      info.PrintResidueInfo( maskexpr );
    else
      info.PrintAtomInfo( maskexpr );
  }
  return 0;
}

/** Add current argument and all following arguments without leading '-' to
  * given string array.
  */
void Cpptraj::AddArgs(Sarray& Args, ArgList const& cmdLineArgs, int& idx)
{
  Args.push_back( cmdLineArgs[++idx] );
  while (idx+1 != cmdLineArgs.Nargs() && cmdLineArgs[idx+1][0] != '-')
    Args.push_back( cmdLineArgs[++idx] );
}

/** \return True if given argument matches key and is not the final argument.
  */
static inline bool NotFinalArg(std::string const& arg, const char* key, int pos, int nargs)
{
  return (arg == key && pos+1 != nargs);
}

/** Read command line args. */
Cpptraj::Mode Cpptraj::ProcessCmdLineArgs(int argc, char** argv) {
  // First convert argv to one continuous string.
  commandLine_.clear();
  for (int i = 1; i < argc; i++)
    commandLine_.append( " " + std::string(argv[i]) );
  // Use ArgList to split into arguments.
  ArgList cmdLineArgs( commandLine_ );
  mprintf("DEBUG: CmdLine: %s\n", cmdLineArgs.ArgLine() );
  // Process command line flags from ArgList
  bool hasInput = false;
  bool interactive = false;
  Sarray inputFiles;
  Sarray topFiles;
  Sarray trajinFiles;
  Sarray trajinArgs;
  Sarray trajoutFiles;
  Sarray refFiles;
  Sarray dataFiles;
  std::string dataOut;
  for (int iarg = 0; iarg < cmdLineArgs.Nargs(); iarg++)
  {
    std::string const& arg = cmdLineArgs[iarg];
    // ----- One-and-done flags ------------------
    if ( arg == "--help" || arg == "-h" ) {
      // --help, -help: Print usage and exit
      SetWorldSilent(true);
      Usage();
      return QUIT;
    }
    if ( arg == "-V" || arg == "--version" ) {
      // -V, --version: Print version number and exit
      SetWorldSilent( true );
      loudPrintf("CPPTRAJ: Version %s\n", CPPTRAJ_VERSION_STRING);
      return QUIT;
    }
    if ( arg == "--internal-version" ) {
      // --internal-version: Print internal version number and quit.
      SetWorldSilent( true );
      loudPrintf("CPPTRAJ: Internal version # %s\n", CPPTRAJ_INTERNAL_VERSION);
      return QUIT;
    }
    if ( arg == "--defines" ) {
      // --defines: Print information on compiler defines used and exit
      SetWorldSilent( true );
      loudPrintf("Compiled with:");
      loudPrintf("%s\n", Cpptraj::Defines().c_str());
      return QUIT;
    }
    if (arg == "-tl") {
      // -tl: Trajectory length
      if (topFiles.empty()) {
        mprinterr("Error: No topology file specified.\n");
        return ERROR;
      }
      SetWorldSilent( true );
      if (State_.TrajLength( topFiles[0], trajinFiles )) return ERROR;
      return QUIT;
    }
    // ----- Single flags ------------------------
    if ( arg == "--interactive" ) {
      interactive = true;
    } else if ( arg == "--suppress-all-output") {
      mprintf("Info: All further output will be suppressed.\n");
      SuppressAllOutput();
    // ----- Flags that precede values -----------
    } else if ( NotFinalArg(arg, "-debug", iarg, cmdLineArgs.Nargs()) ) {
      // -debug: Set overall debug level
      ArgList dbgarg( cmdLineArgs[++iarg] );
      State_.SetListDebug( dbgarg );
    } else if ( NotFinalArg(arg, "--log", iarg, cmdLineArgs.Nargs()) ) {
      // --log: Set up log file for interactive mode
      logfilename_ = cmdLineArgs[++iarg];
    } else if ( NotFinalArg(arg, "-p", iarg, cmdLineArgs.Nargs()) ) {
      // -p: Topology file
      AddArgs( topFiles, cmdLineArgs, iarg );
    } else if ( NotFinalArg(arg, "-d", iarg, cmdLineArgs.Nargs()) ) {
      // -d: Read data file
      AddArgs( dataFiles, cmdLineArgs, iarg );
    } else if ( NotFinalArg(arg, "-w", iarg, cmdLineArgs.Nargs()) ) {
      // -w: Write data file. Only one allowed. For data file conversion.
      dataOut.assign( cmdLineArgs[++iarg] );
    } else if ( NotFinalArg(arg, "-y", iarg, cmdLineArgs.Nargs()) ) {
      // -y: Trajectory file in.
      AddArgs( trajinFiles, cmdLineArgs, iarg );
    } else if ( NotFinalArg(arg, "-ya", iarg, cmdLineArgs.Nargs()) ) {
      // -ya: Trajectory file in arguments.
      AddArgs( trajinArgs, cmdLineArgs, iarg );
    } else if ( NotFinalArg(arg, "-x", iarg, cmdLineArgs.Nargs()) ) {
      // -x: Trajectory file out
      trajoutFiles.push_back( cmdLineArgs[++iarg] );
    } else if ( NotFinalArg(arg, "-c", iarg, cmdLineArgs.Nargs()) ) {
      // -c: Reference file
      AddArgs( refFiles, cmdLineArgs, iarg );
    } else if ( NotFinalArg(arg, "-i", iarg, cmdLineArgs.Nargs()) ) {
      // -i: Input file(s)
      AddArgs( inputFiles, cmdLineArgs, iarg );
    } else if ( NotFinalArg(arg, "-o", iarg, cmdLineArgs.Nargs()) ) {
      // -o: Output file
      FileName ofilename(cmdLineArgs[++iarg]);
      if (ofilename.empty()) {
        mprinterr("Error: Could not set up output file with name '%s'\n", ofilename.full());
        return ERROR;
      }
      if (OutputToFile(ofilename.full())) return ERROR;
    } else if ( NotFinalArg(arg, "-ms", iarg, cmdLineArgs.Nargs()) ) {
      // -ms: Parse mask string, print selected atom #s
      if (ProcessMask( topFiles, refFiles, cmdLineArgs[++iarg], false, false )) return ERROR;
      return QUIT;
    } else if ( NotFinalArg(arg, "-mr", iarg, cmdLineArgs.Nargs()) ) {
      // -mr: Parse mask string, print selected res #s
      if (ProcessMask( topFiles, refFiles, cmdLineArgs[++iarg], false, true )) return ERROR;
      return QUIT;
    } else if ( NotFinalArg(arg, "--mask", iarg, cmdLineArgs.Nargs()) ) {
      // --mask: Parse mask string, print selected atom details
      if (ProcessMask( topFiles, refFiles, cmdLineArgs[++iarg], true, false )) return ERROR;
      return QUIT;
    } else if ( NotFinalArg(arg, "--resmask", iarg, cmdLineArgs.Nargs()) ) {
      // --resmask: Parse mask string, print selected residue details
      if (ProcessMask( topFiles, refFiles, cmdLineArgs[++iarg], true, true )) return ERROR;
      return QUIT;
    } else if ( iarg == 0 ) {
      // For backwards compatibility with PTRAJ; Position 1 = TOP file
      topFiles.push_back( cmdLineArgs[iarg] );
    } else if ( iarg == 1 ) {
      // For backwards compatibility with PTRAJ; Position 2 = INPUT file
      inputFiles.push_back( cmdLineArgs[iarg] );
    } else {
      // Unrecognized
      mprintf("  Unrecognized input on command line: %i: %s\n", iarg+1, cmdLineArgs[iarg].c_str());
      Usage();
      return ERROR;
    }
  }
  Cpptraj::Intro();
  // Add all data files specified on command lin.
  for (Sarray::const_iterator dataFilename = dataFiles.begin();
                              dataFilename != dataFiles.end();
                            ++dataFilename)
  {
    DataFile dataIn;
    dataIn.SetDebug( State_.Debug() );
    if (dataIn.ReadDataIn( *dataFilename, ArgList(), State_.DSL()) != 0)
      return ERROR;
  }
  // Write all data sets from input data files if output data specified
  if (!dataOut.empty()) {
    hasInput = true; // This allows direct data conversion with no other input
    if (State_.DSL().empty()) {
      mprinterr("Error: '-w' specified but no input data sets '-d'\n");
      return ERROR;
    }
    DataFile DF;
    if (DF.SetupDatafile( dataOut, State_.Debug() )) return ERROR;
    for (DataSetList::const_iterator ds = State_.DSL().begin(); ds != State_.DSL().end(); ++ds)
      if (DF.AddDataSet( *ds )) {
        mprinterr("Error: Could not add data set '%s' to file '%s'\n", (*ds)->legend(),
                  dataOut.c_str());
        return ERROR;
      }
    mprintf("\tWriting sets to '%s', format '%s'\n", DF.DataFilename().full(), DF.FormatString());
    DF.WriteDataOut();
  }
  // Add all topology files specified on command line.
  for (Sarray::const_iterator topFilename = topFiles.begin();
                              topFilename != topFiles.end();
                              ++topFilename)
    if (State_.AddTopology( *topFilename, ArgList() )) return ERROR;
  // Add all reference trajectories specified on command line.
  for (Sarray::const_iterator refName = refFiles.begin();
                              refName != refFiles.end();
                              ++refName)
    if (State_.AddReference( *refName )) return ERROR;
  // Add all input trajectories specified on command line.
  // If there are fewer input trajectory arguments than input trajectories,
  // duplicate the last input trajectory argument.
  if (!trajinArgs.empty()) {
    mprintf("DEBUG: trajinArgs:\n");
    for (Sarray::const_iterator ta = trajinArgs.begin(); ta != trajinArgs.end(); ++ta)
      mprintf("\t%s\n", ta->c_str());
    if (trajinFiles.empty())
      mprintf("Warning: Input trajectory arguments specified but no input trajectories.\n");
    else {
      if (trajinArgs.size() < trajinFiles.size())
        trajinArgs.resize( trajinFiles.size(), trajinArgs.back() );
      else if (trajinArgs.size() > trajinFiles.size()) {
        mprintf("Warning: More input trajectory arguments specified than input trajectories.\n");
        trajinArgs.resize( trajinFiles.size() );
      }
    }
    for (unsigned int it = 0; it != trajinFiles.size(); it++)
      if (State_.AddInputTrajectory( trajinFiles[it] + " " + trajinArgs[it] )) return ERROR;
  } else {
    for (Sarray::const_iterator trajinName = trajinFiles.begin();
                                trajinName != trajinFiles.end();
                                ++trajinName)
      if (State_.AddInputTrajectory( *trajinName )) return ERROR;
  }
  // Add all output trajectories specified on command line.
  if (!trajoutFiles.empty()) {
    hasInput = true; // This allows direct traj conversion with no other input
    for (Sarray::const_iterator trajoutName = trajoutFiles.begin();
                                trajoutName != trajoutFiles.end();
                                ++trajoutName)
      if (State_.AddOutputTrajectory( *trajoutName )) return ERROR;
  }
  // Process all input files specified on command line.
  if ( !inputFiles.empty() ) {
    hasInput = true;
    for (Sarray::const_iterator inputFilename = inputFiles.begin();
                                inputFilename != inputFiles.end();
                                ++inputFilename)
    {
      CpptrajState::RetType c_err = Command::ProcessInput( State_, *inputFilename );
      if (c_err == CpptrajState::ERR && State_.ExitOnError()) return ERROR;
      if (Command::UnterminatedControl()) return ERROR;
      if (c_err == CpptrajState::QUIT) return QUIT;
    }
  }
  // Determine whether to enter interactive mode.
  if (interactive) {
    // User explicitly requested ``--interactive``. Do not check isatty.
    return INTERACTIVE;
  } else if (!hasInput) {
    if (isatty(fileno(stdin)))
      return INTERACTIVE;
    else {
      // "" means read from STDIN
      CpptrajState::RetType c_err = Command::ProcessInput( State_, "" ); 
      if (c_err == CpptrajState::ERR && State_.ExitOnError()) return ERROR;
      if (Command::UnterminatedControl()) return ERROR;
      if (c_err == CpptrajState::QUIT) return QUIT;
    }
  }
  return BATCH;
}

// Cpptraj::Interactive()
int Cpptraj::Interactive() {
  ReadLine inputLine;
  // By default when interactive do not exit on errors
  State_.SetNoExitOnError();
  // Open log file. If no name has been set, use default.
  CpptrajFile logfile_;
  if (logfilename_.empty())
    logfilename_.SetFileName("cpptraj.log");
# if defined (NO_READLINE) || defined (LIBCPPTRAJ)
  // No need to load history if not using readline (or this is libcpptraj)
# else
  if (File::Exists(logfilename_)) {
    // Load previous history.
    if (logfile_.OpenRead(logfilename_)==0) {
      mprintf("\tLoading previous history from log '%s'\n", logfile_.Filename().full());
      std::string previousLine = logfile_.GetLine();
      while (!previousLine.empty()) {
        if (previousLine[0] != '#') {
          // Remove any newline chars.
          std::size_t found = previousLine.find_first_of("\r\n");
          if (found != std::string::npos)
            previousLine[found] = '\0';
          inputLine.AddHistory( previousLine.c_str() );
        }
        previousLine = logfile_.GetLine();
      }
      logfile_.CloseFile();
    }
  }
# endif
  logfile_.OpenAppend(logfilename_);
  if (logfile_.IsOpen()) {
    // Write logfile header entry: date, cmd line opts, topologies
    logfile_.Printf("# %s\n", TimeString().c_str());
    if (!commandLine_.empty())
      logfile_.Printf("#%s\n", commandLine_.c_str());
    DataSetList tops = State_.DSL().GetSetsOfType("*", DataSet::TOPOLOGY);
    if (!tops.empty()) {
      logfile_.Printf("# Loaded topologies:\n");
      for (DataSetList::const_iterator top = tops.begin(); top != tops.end(); ++top)
        logfile_.Printf("#   %s\n", (*top)->Meta().Fname().full());
    }
  }
  CpptrajState::RetType readLoop = CpptrajState::OK;
  while ( readLoop != CpptrajState::QUIT ) {
    if (inputLine.GetInput()) {
      // EOF (Ctrl-D) specified. If state is not empty, ask before exiting.
      if (!State_.EmptyState()) {
        if (inputLine.YesNoPrompt("EOF (Ctrl-D) specified but there are actions/"
                                  "analyses/trajectories queued. Really quit? [y/n]> "))
          break;
      } else
        break;
    }
    if (!inputLine.empty()) {
      readLoop = Command::Dispatch( State_, *inputLine );
      if (logfile_.IsOpen() && (readLoop != CpptrajState::ERR || State_.RecordAllInput())) {
        logfile_.Printf("%s\n", inputLine.c_str());
        logfile_.Flush();
      }
    }
    // If state is not empty, ask before exiting.
    if (readLoop == CpptrajState::QUIT && !State_.EmptyState()) {
      if (inputLine.YesNoPrompt("There are actions/analyses/trajectories queued. "
                                "Really quit? [y/n]> "))
        break;
      else
        readLoop = CpptrajState::OK;
    }
  }
  logfile_.CloseFile();
  if (Command::UnterminatedControl()) return 1;
  if (readLoop == CpptrajState::ERR) return 1;
  return 0;
}
