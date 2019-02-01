#include "Analysis_HausdorffDistance.h"
#include "CpptrajStdio.h"
#include "DataSet_MatrixFlt.h"

Analysis_HausdorffDistance::Analysis_HausdorffDistance() :
  outType_(BASIC),
  out_(0)
{}

/** Assume input matrix contains all distances beween sets A and B.
  * The directed Hausdorff distance from A to B will be the maximum of all
  * minimums in each row.
  * The directed Hausdorff distance from B to A will be the maximum of all
  * minimums in each column.
  * The symmetric Hausdorff distance is the max of the two directed distances.
  * \return the symmetric Hausdorff distance. 
  */
double Analysis_HausdorffDistance::CalcHausdorffFromMatrix(DataSet_2D const& m1) {
//  if (m1 == 0) {
//    mprinterr("Internal Error: Analysis_HausdorffDistance::(): Matrix set is null.\n");
//    return -1.0;
//  }
  if (m1.Size() < 1) {
    mprinterr("Error: '%s' is empty.\n", m1.legend());
    return -1.0;
  }
  // Hausdorff distance from A to B. 
  double hd_ab = 0.0;
  for (unsigned int row = 0; row != m1.Nrows(); row++)
  {
    double minRow = m1.GetElement(0, row);
    for (unsigned int col = 1; col != m1.Ncols(); col++)
      minRow = std::min( minRow, m1.GetElement(col, row) );
    mprintf("DEBUG: Min row %6u is %12.4f\n", row, minRow);
    hd_ab = std::max( hd_ab, minRow );
  }
  mprintf("DEBUG: Hausdorff A to B= %12.4f\n", hd_ab);
  // Hausdorff distance from B to A.
  double hd_ba = 0.0;
  for (unsigned int col = 0; col != m1.Ncols(); col++)
  {
    double minCol = m1.GetElement(col, 0);
    for (unsigned int row = 1; row != m1.Nrows(); row++)
      minCol = std::min( minCol, m1.GetElement(col, row) );
    mprintf("DEBUG: Min col %6u is %12.4f\n", col, minCol);
    hd_ba = std::max( hd_ba, minCol);
  }
  mprintf("DEBUG: Hausdorff B to A= %12.4f\n", hd_ba);
  // Symmetric Hausdorff distance
  double hd = std::max( hd_ab, hd_ba );
    
  return hd;
}

// Analysis_HausdorffDistance::Help()
void Analysis_HausdorffDistance::Help() const {
  mprintf("\t<set arg0> [<set arg1> ...] [outtype {basic|trimatrix nrows <#>}]\n"
          "\t[name <output set name>] [out <filename>]\n"
          "  Given 1 or more 2D matrices containing distances between two sets\n"
          "  A and B, calculate the symmetric Hausdorff distance for each matrix.\n"
          "  The results can be saved as an array or as an upper-triangular\n"
          "  matrix with the specified number of rows.\n");
}


// Analysis_HausdorffDistance::Setup()
Analysis::RetType Analysis_HausdorffDistance::Setup(ArgList& analyzeArgs, AnalysisSetup& setup, int debugIn)
{
  // Keywords
  int nrows = -1;
  std::string outtypearg = analyzeArgs.GetStringKey("outtype");
  if (!outtypearg.empty()) {
    if (outtypearg == "basic")
      outType_ = BASIC;
    else if (outtypearg == "trimatrix") {
      outType_ = UPPER_TRI_MATRIX;
      nrows = analyzeArgs.getKeyInt("nrows", -1);
      if (nrows < 1) {
        mprinterr("Error: 'nrows' must be specified and > 0 for 'trimatrix'\n");
        return Analysis::ERR;
      }
    } else {
      mprinterr("Error: Unrecognized keyword for 'outtype': %s\n", outtypearg.c_str());
      return Analysis::ERR;
    }
  } else
    outType_ = BASIC;
  std::string dsname = analyzeArgs.GetStringKey("name");
  DataFile* df = setup.DFL().AddDataFile( analyzeArgs.GetStringKey("out"), analyzeArgs );
  // Get input data sets
  std::string dsarg = analyzeArgs.GetStringNext();
  while (!dsarg.empty()) {
    inputSets_ += setup.DSL().GetMultipleSets( dsarg );
    dsarg = analyzeArgs.GetStringNext();
  }
  if (inputSets_.empty()) {
    mprinterr("Error: No data sets specified.\n");
    return Analysis::ERR;
  }
  // Output data set
  out_ = 0;
  if (outType_ == BASIC) {
    out_ = setup.DSL().AddSet(DataSet::FLOAT, dsname, "HAUSDORFF");
    if (out_ == 0) return Analysis::ERR;
  } else if (outType_ == UPPER_TRI_MATRIX) {
    out_ = setup.DSL().AddSet(DataSet::MATRIX_FLT, dsname, "HAUSDORFF");
    if (out_ == 0 || ((DataSet_2D*)out_)->AllocateTriangle( nrows )) return Analysis::ERR;
    if (out_->Size() != inputSets_.size())
      mprintf("Warning: Number of input data sets (%zu) != number of expected sets in matrix (%zu)\n",
              inputSets_.size(), out_->Size());
  }
  if (df != 0) df->AddDataSet( out_ );

  mprintf("    HAUSDORFF:\n");
  mprintf("\tCalculating Hausdorff distances from the following 2D distance matrices:\n");
  for (DataSetList::const_iterator it = inputSets_.begin(); it != inputSets_.end(); ++it)
    mprintf("\t  %s\n", (*it)->legend());
  if (outType_ == BASIC)
    mprintf("\tOutput will be stored in 1D array set '%s'\n", out_->legend());
  else if (outType_ == UPPER_TRI_MATRIX)
    mprintf("\tOutput will be stored in upper-triangular matrix set '%s' with %i rows.\n",
            out_->legend(), nrows);
  if (df != 0) mprintf("\tOutput set written to '%s'\n", df->DataFilename().full());

  return Analysis::OK;
}

// Analysis_HausdorffDistance::Analyze()
Analysis::RetType Analysis_HausdorffDistance::Analyze() {
  // Get the Hausdorff distance for each set.
  double hd = 0.0;
  int idx = 0;
  for (DataSetList::const_iterator it = inputSets_.begin(); it != inputSets_.end(); ++it)
  {
    hd = -1.0;
    if ( (*it)->Group() == DataSet::MATRIX_2D )
      hd = CalcHausdorffFromMatrix( static_cast<DataSet_2D const&>( *(*it) ) );
    else
      mprintf("Warning: '%s' type not yet supported for Hausdorff\n", (*it)->legend());
    mprintf("%12.4f %s\n", hd, (*it)->legend());
    float fhd = (float)hd;
    switch (outType_) {
      case BASIC : out_->Add(idx++, &fhd); break;
      case UPPER_TRI_MATRIX : ((DataSet_MatrixFlt*)out_)->AddElement( fhd ); break;
    }
  }
  return Analysis::OK;
}
