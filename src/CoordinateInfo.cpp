#include "CoordinateInfo.h"
#include "CpptrajStdio.h"

/** Default constructor. */
CoordinateInfo::CoordinateInfo() :
  ensembleSize_(0),
  hasCrd_(true),
  hasVel_(false),
  hasTemp_(false),
  has_pH_(false),
  hasRedox_(false),
  hasTime_(false),
  hasFrc_(false)
{}

/** Box, velocity, temperature, time. TODO pH, redox? */
CoordinateInfo::CoordinateInfo(Box const& b, bool v, bool t, bool m) :
  box_(b),
  ensembleSize_(0),
  hasCrd_(true),
  hasVel_(v),
  hasTemp_(t),
  has_pH_(false),
  hasRedox_(false),
  hasTime_(m),
  hasFrc_(false)
{}

/** Box, coords, velocity, force, time. */
CoordinateInfo::CoordinateInfo(Box const& b, bool c, bool v, bool f, bool m) :
  box_(b),
  ensembleSize_(0),
  hasCrd_(c),
  hasVel_(v),
  hasTemp_(false),
  has_pH_(false),
  hasRedox_(false),
  hasTime_(m),
  hasFrc_(f)
{}

/** Constructor - All */
CoordinateInfo::CoordinateInfo(int e, ReplicaDimArray const& r, Box const& b,
                               bool c, bool v, bool t, bool m, bool f) :
  remdDim_(r),
  box_(b),
  ensembleSize_(e),
  hasCrd_(c),
  hasVel_(v),
  hasTemp_(t),
  has_pH_(false),
  hasRedox_(false),
  hasTime_(m),
  hasFrc_(f)
{}

/** DEBUG: Print info to stdout. */
void CoordinateInfo::PrintCoordInfo(const char* name, const char* parm) const {
  mprintf("DBG: '%s' parm '%s' CoordInfo={ box type %s", name, parm, box_.TypeName());
  if (remdDim_.Ndims() > 0) mprintf(", %i rep dims", remdDim_.Ndims());
  if (hasVel_) mprintf(", velocities");
  if (hasTemp_) mprintf(", temps");
  if (hasTime_) mprintf(", times");
  if (hasFrc_) mprintf(", forces");
  if (ensembleSize_ > 0) mprintf(", ensemble size %i", ensembleSize_);
  mprintf(" }\n");
}

static inline void Append(std::string& meta, std::string const& str) {
  if (meta.empty())
    meta.assign( str );
  else
    meta.append(", " + str);
}

/** \return String describing elements that are present. */
std::string CoordinateInfo::InfoString() const {
  std::string meta;
  if ( HasCrd() )         Append(meta, "coordinates");
  if ( HasVel() )         Append(meta, "velocities");
  if ( HasForce() )       Append(meta, "forces");
  if ( HasTemp() )        Append(meta, "temperature");
  if ( HasTime() )        Append(meta, "time");
  if ( HasReplicaDims() ) Append(meta, "replicaDims");
  if ( HasBox() )         Append(meta, "box");
  return meta;
}

#ifdef MPI
#define CINFOMPISIZE 8
int CoordinateInfo::SyncCoordInfo(Parallel::Comm const& commIn) {
  // ensSize, hasvel, hastemp, hastime, hasfrc, NrepDims, Dim1, ..., DimN, 
  int* iArray;
  int iSize;
  if (commIn.Master()) {
    iSize = remdDim_.Ndims() + CINFOMPISIZE;
    commIn.MasterBcast( &iSize, 1, MPI_INT );
    iArray = new int[ iSize ];
    iArray[0] = ensembleSize_;
    iArray[1] = (int)hasVel_;
    iArray[2] = (int)hasTemp_;
    iArray[3] = (int)hasTime_;
    iArray[4] = (int)hasFrc_;
    iArray[5] = (int)has_pH_;
    iArray[6] = (int)hasRedox_;
    iArray[7] = remdDim_.Ndims();
    unsigned int ii = CINFOMPISIZE;
    for (int ir = 0; ir != remdDim_.Ndims(); ir++, ii++)
      iArray[ii] = remdDim_[ir];
    commIn.MasterBcast( iArray, iSize, MPI_INT );
  } else {
    commIn.MasterBcast( &iSize, 1, MPI_INT );
    iArray = new int[ iSize ];
    commIn.MasterBcast( iArray, iSize, MPI_INT );
    ensembleSize_ = iArray[0];
    hasVel_       = (bool)iArray[1];
    hasTemp_      = (bool)iArray[2];
    hasTime_      = (bool)iArray[3];
    hasFrc_       = (bool)iArray[4];
    has_pH_       = (bool)iArray[5];
    hasRedox_     = (bool)iArray[6];
    remdDim_.clear();
    unsigned int ii = CINFOMPISIZE;
    for (int ir = 0; ir != iArray[7]; ir++, ii++)
      remdDim_.AddRemdDimension( iArray[ii] );
  }
  delete[] iArray;
  box_.SyncBox( commIn );
  return 0;
}
#undef CINFOMPISIZE
#endif
