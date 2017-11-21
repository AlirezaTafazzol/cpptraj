#include "DataSet_pH.h"
#include "CpptrajStdio.h"

DataSet_pH::DataSet_pH() :
  DataSet_1D(PH, TextFormat(TextFormat::INTEGER, 10))
{}

int DataSet_pH::Allocate(SizeArray const& sizeIn) {
  if (!sizeIn.empty()) {
    states_.reserve( sizeIn[0] );
  }
  return 0;
}

void DataSet_pH::WriteBuffer(CpptrajFile &cbuffer, SizeArray const& pIn) const {
  if (pIn[0] >= states_.size())
    cbuffer.Printf(format_.fmt(), 0);
  else
    cbuffer.Printf(format_.fmt(), states_[pIn[0]]);
}

void DataSet_pH::Resize(size_t n) {
  states_.resize( n );
}

void DataSet_pH::Info() const {
  mprintf(" (%s %i pH= %.2f)", *(res_.Name()), res_.Num(), solvent_pH_);
}
# ifdef MPI
void DataSet_pH::Consolidate(Parallel::Comm const& commIn, int rank)
{
  if (commIn.Rank() == rank) {
    //commIn.Barrier(); // DEBUG
    //rprintf("MASTER:\n");
    //for (std::vector<int>::const_iterator it = residues_.front().begin(); it != residues_.front().end(); ++it)
    //  rprintf("%4u %1i\n", it-residues_.front().begin()+1, *it);
    //mflush();
    //commIn.Barrier(); // DEBUG
    std::vector<int> itmp = states_;
    commIn.Reduce(rank, &states_[0], &itmp[0], states_.size(), MPI_INT, MPI_SUM);
  } else {
    //commIn.Barrier(); // DEBUG
    //commIn.Barrier();
    //rprintf("CHILD_:\n");
    //for (std::vector<int>::const_iterator it = residues_.front().begin(); it != residues_.front().end(); ++it)
    //  rprintf("%4u %1i\n", it-residues_.front().begin()+1, *it);
    commIn.Reduce(rank, 0, &states_[0], states_.size(), MPI_INT, MPI_SUM);
  }
}
#endif
