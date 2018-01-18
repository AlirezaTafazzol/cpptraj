#include "DataIO_CharmmRtfPrm.h"
#include "CpptrajStdio.h"
#include "DataSet_Parameters.h"
#include "BufferedLine.h"

/// CONSTRUCTOR
DataIO_CharmmRtfPrm::DataIO_CharmmRtfPrm()
{

}

// DataIO_CharmmRtfPrm::ID_DataFormat()
bool DataIO_CharmmRtfPrm::ID_DataFormat(CpptrajFile& infile)
{

  return false;
}

// DataIO_CharmmRtfPrm::ReadHelp()
void DataIO_CharmmRtfPrm::ReadHelp()
{

}

// DataIO_CharmmRtfPrm::processReadArgs()
int DataIO_CharmmRtfPrm::processReadArgs(ArgList& argIn)
{

  return 0;
}

static inline std::string Input(const char* line) {
  std::string input;
  const char* ptr = line;
  while (*ptr != '\0') {
    if (*ptr == '!') break;
    input += *ptr;
    ++ptr;
  }
  return input;
}

// DataIO_CharmmRtfPrm::ReadData()
int DataIO_CharmmRtfPrm::ReadData(FileName const& fname, DataSetList& dsl, std::string const& dsname)
{
  BufferedLine infile;
  if (infile.OpenFileRead(fname)) return 1;
  const char* line = infile.Line();
  if (line == 0) return 1;
  // Allocate data set
  MetaData md( dsname );
  DataSet* ds = dsl.CheckForSet( md );
  if (ds != 0) {
    if (ds->Type() != DataSet::PARAMETERS) {
      mprinterr("Error: Set '%s' does not have parameters, cannot append.\n", ds->legend());
      return 1;
    }
    mprintf("\tAdding to existing set %s\n", ds->legend());
  } else {
    ds = dsl.AddSet( DataSet::PARAMETERS, md );
    if (ds == 0) return 1;
  }
  DataSet_Parameters& prm = static_cast<DataSet_Parameters&>( *ds );

  int readParam = -1;
  while (line != 0) {
    if (line[0] != '*') {
      // Input() will read everything up to comment character
      ArgList args( Input(line), " \t" );
      if (args.Nargs() > 0) {
        // Check for continuation
        while (args[args.Nargs()-1] == "-") {
          args.MarkArg(args.Nargs()-1);
          line = infile.Line();
          args.Append( ArgList(Input(line)) );
        }
        if (readParam == -1) {
          if (args.Nargs() >= 2 && args[0] == "read" && args.hasKey("param"))
            readParam = 0; 
        } else {
          // Reading parameters
          mprintf("DBG: %s\n", args.ArgLine());
          if (args.hasKey("ATOMS")) readParam = 1;
          else if (args.hasKey("BONDS")) readParam = 2;
          else if (args.hasKey("ANGLES")) readParam = 3;
          else if (args.hasKey("DIHEDRALS")) readParam = 4;
          else if (args.hasKey("IMPROPERS")) readParam = 5;
          else if (args.hasKey("NONBONDED")) readParam = 6;
          else if (args.hasKey("END")) break;
          else if (readParam == 2) {
            // BOND PARAMETERS
            AtomTypeHolder types(2);
            types.AddName( args.GetStringNext() );
            types.AddName( args.GetStringNext() );
            prm.AT().CheckForAtomType( types[0] );
            prm.AT().CheckForAtomType( types[1] );
            prm.BP().AddBondParm(types,
                                 BondParmType(args.getNextDouble(0), args.getNextDouble(0)),
                                 false);
          }
        }
      }
    }
    line = infile.Line();
  }
 
  prm.Debug(); 
  return 1;
}

// DataIO_CharmmRtfPrm::WriteHelp()
void DataIO_CharmmRtfPrm::WriteHelp()
{

}

// DataIO_CharmmRtfPrm::processWriteArgs()
int DataIO_CharmmRtfPrm::processWriteArgs(ArgList& argIn)
{

  return 0;
}

// DataIO_CharmmRtfPrm::WriteData()
int DataIO_CharmmRtfPrm::WriteData(FileName const& fname, DataSetList const& dsl)
{

  return 1;
}
