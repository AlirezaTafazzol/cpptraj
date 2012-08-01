#include <sstream>
#ifdef DATAFILE_TIME
#include <ctime>
#endif
#include "DataFile.h"
#include "CpptrajStdio.h"
// All DataIO classes go here
#include "DataIO_Std.h"
#include "DataIO_Grace.h"
#include "DataIO_Gnuplot.h"

// CONSTUCTOR
DataFile::DataFile() {
  debug_ = 0;
  dataType_ = DATAFILE;
  dataio_ = NULL;
  isInverted_ = false;
}

// DESTRUCTOR
DataFile::~DataFile() {
  if (dataio_ != NULL) delete dataio_;
}

// DataFile::SetDebug()
void DataFile::SetDebug(int debugIn) {
  debug_ = debugIn;
}

// DataFile::SetupDatafile()
int DataFile::SetupDatafile(const char* fnameIn) {
  DataIO basicData;

  if (fnameIn==NULL) return 1;
  // Open basic data file
  int err = basicData.SetupWrite(fnameIn,debug_);
  if (err!=0) return 1;
  // Determine data format from extension 
  std::string data_extension = basicData.Extension();
  if (data_extension==".agr")
    dataType_ = XMGRACE;
  else if (data_extension==".gnu")
    dataType_ = GNUPLOT;
  else if (data_extension==".dat")
    dataType_ = DATAFILE;
  // Set up DataIO based on format. 
  switch (dataType_) {
    case DATAFILE : dataio_ = new DataIO_Std(); break;
    case XMGRACE  : dataio_ = new DataIO_Grace(); break;
    case GNUPLOT  : dataio_ = new DataIO_Gnuplot(); break;
    default       : dataio_ = new DataIO_Std(); break;
  }
  if (dataio_==NULL) return 1;

  // Place the basic file in the data IO class
  dataio_->DataIO::operator=( basicData );

  // Set Debug
  dataio_->SetDebug( debug_ );
  
  return 0;
}

// DataFile::AddSet()
int DataFile::AddSet(DataSet* dataIn) {
  if (dataIn == NULL) return 1;
  SetList_.AddCopyOfSet( dataIn );
  return 0;
}

// DataFile::ProcessArgs()
int DataFile::ProcessArgs(ArgList &argIn) {
  if (dataio_==NULL) return 1;
  if (argIn.hasKey("invert")) {
    isInverted_ = true;
    // Currently GNUPLOT files cannot be inverted.
    if (dataType_ == GNUPLOT) {
      mprintf("Warning: (%s) Gnuplot files cannot be inverted.\n",dataio_->Name());
      isInverted_ = false;;
    }
  }
  if (dataio_->processWriteArgs(argIn)==1) return 1;
  if (dataio_->ProcessCommonArgs(argIn)==1) return 1;
  argIn.CheckForMoreArgs();
  return 0;
}

// DataFile::ProcessArgs()
int DataFile::ProcessArgs(const char *argsIn) {
  ArgList args(argsIn, " ");
  return ProcessArgs(args);
}

// DataFile::SetXlabel()
void DataFile::SetXlabel(const char *xlabel) {
  ArgList args;
  args.AddArg("xlabel");
  args.AddArg(xlabel);
  ProcessArgs(args);
}

// DataFile::SetYlabel()
void DataFile::SetYlabel(const char *ylabel) {
  ArgList args;
  args.AddArg("ylabel");
  args.AddArg(ylabel);
  ProcessArgs(args);
}

// DataFile::SetCoordMinStep()
void DataFile::SetCoordMinStep(double xminIn, double xstepIn,
                               double yminIn, double ystepIn)
{
  std::ostringstream oss;
  ArgList args;
  oss << "xmin " << xminIn << " xstep " << xstepIn;
  oss << " ymin " << yminIn << " ystep " << ystepIn;
  args.SetList((char*)oss.str().c_str(), " ");
  ProcessArgs(args);
}

// DataFile::Write()
void DataFile::Write() {
  if (dataio_->OpenFile()) return;

  // Remove data sets that do not contain data. Also determine max X and 
  //ensure all datasets in this file have same dimension.
  int maxFrames = 0;
  int currentDim = 0;
  DataSetList::const_iterator Dset = SetList_.end();
  while (Dset != SetList_.begin()) {
    --Dset;
    // Check dimension
    if (currentDim == 0) 
      currentDim = (*Dset)->Dim();
    else if (currentDim != (*Dset)->Dim()) {
      mprinterr("Error: Writing files with datasets of different dimensions\n");
      mprinterr("Error: is currently not supported (%i and %i present).\n",
                currentDim, (*Dset)->Dim());
      return;
    }
    // Check if set has no data.
    if ( (*Dset)->Empty() ) {
      // If set has no data, remove it
      mprintf("Warning: Set %s contains no data. Skipping.\n",(*Dset)->c_str());
      SetList_.erase( Dset );
      Dset = SetList_.end();
    } else {
      // If set has data, set its format to right-aligned initially. Also 
      // determine what the maximum x value for the set is.
      if ( (*Dset)->SetDataSetFormat(false) ) {
        mprinterr("Error: could not set format string for set %s. Skipping.\n", (*Dset)->c_str());
        SetList_.erase( Dset );
        Dset = SetList_.end();
      } else {
        int maxSetFrames = (*Dset)->Xmax();
        if (maxSetFrames > maxFrames)
          maxFrames = maxSetFrames;
      }
    }
  }
  // If all data sets are empty no need to write
  if (SetList_.empty()) {
    mprintf("Warning: file %s has no sets containing data.\n", dataio_->Name());
    return;
  }

  // Since maxFrames is the last frame, the actual # of frames is 
  // maxFrames+1 (for use in loops).
  ++maxFrames;

  mprintf("DEBUG:\tFile %s has %i sets, dimension=%i, maxFrames=%i\n", dataio_->Name(),
          SetList_.size(), currentDim, maxFrames);
#ifdef DATAFILE_TIME
  clock_t t0 = clock();
#endif
  if ( currentDim == 1 ) {
    mprintf("%s: Writing %i frames.\n",dataio_->Name(),maxFrames);
    if (maxFrames>0) {
      dataio_->SetMaxFrames( maxFrames );
      if (!isInverted_)
        dataio_->WriteData(SetList_);
      else
        dataio_->WriteDataInverted(SetList_);
    } else {
      mprintf("Warning: DataFile %s has no valid sets - skipping.\n",
              dataio_->Name());
    }
  } else if ( currentDim == 2) {
    int err = 0;
    for ( DataSetList::const_iterator set = SetList_.begin();
                                      set != SetList_.end(); ++set)
      err = dataio_->WriteData2D( *(*set) );
    if (err > 0) 
      mprinterr("Error writing 2D DataSets to %s\n", dataio_->Name());
  }
#ifdef DATAFILE_TIME
  clock_t tf = clock();
  mprinterr("DataFile %s Write took %f seconds.\n", dataio_->Name(),
            ((float)(tf - t0)) / CLOCKS_PER_SEC);
#endif
  dataio_->CloseFile();
}

// DataFile::SetPrecision()
/** Set precision of the specified dataset to width.precision. If no name or
  * asterisk specified set for all datasets in file.
  */
void DataFile::SetPrecision(char *dsetName, int widthIn, int precisionIn) {
  if (widthIn<1) {
    mprintf("Error: SetPrecision (%s): Cannot set width < 1.\n",dataio_->Name());
    return;
  }
  int precision = precisionIn;
  if (precisionIn<0) precision = 0;
  // If NULL or <dsetName>=='*' specified set precision for all data sets
  if (dsetName==NULL || dsetName[0]=='*') {
    mprintf("    Setting width.precision for all sets in %s to %i.%i\n",
            dataio_->Name(),widthIn,precision);
    for (DataSetList::const_iterator set = SetList_.begin(); set!=SetList_.end(); ++set)
      (*set)->SetPrecision(widthIn,precision);

  // Otherwise find dataset <dsetName> and set precision
  } else {
    mprintf("    Setting width.precision for dataset %s to %i.%i\n",
            dsetName,widthIn,precision);
    DataSet *Dset = SetList_.Get(dsetName);
    if (Dset!=NULL)
      Dset->SetPrecision(widthIn,precision);
    else
      mprintf("Error: Dataset %s not found in datafile %s\n",dsetName,dataio_->Name());
  }
}

// DataFile::Filename()
const char *DataFile::Filename() {
  return dataio_->BaseName();
}

// DataFile::DataSetNames()
/** Print Dataset names to one line. If the number of datasets is greater 
  * than 10 just print the first and last 4 data sets.
  */
void DataFile::DataSetNames() {
  DataSetList::const_iterator set = SetList_.begin();
  if (SetList_.size() > 10) {
    int setnum = 0;
    while (setnum < 4) {
      mprintf(" %s",(*set)->Legend().c_str());
      ++setnum;
      ++set;
    }
    mprintf(" ...");
    set = SetList_.end();
    setnum=0;
    while (setnum < 4) {
      --set;
      mprintf(" %s",(*set)->Legend().c_str());
      ++setnum;
    }
  } else {
    for (; set != SetList_.end(); set++)
      mprintf(" %s",(*set)->Legend().c_str());
  }
}

