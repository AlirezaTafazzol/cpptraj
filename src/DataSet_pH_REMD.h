#ifndef INC_DATASET_PH_REMD_H
#define INC_DATASET_PH_REMD_H
#include "DataSet.h"
#include "CphResidue.h"
/// Hold unsorted data from constant pH REMD simulations; protonation states of each residue.
class DataSet_pH_REMD : public DataSet {
    typedef std::vector<int> Iarray;
    typedef std::vector<bool> Barray;
    typedef std::vector<float> Farray;
  public:
    DataSet_pH_REMD();
    static DataSet* Alloc() { return (DataSet*)new DataSet_pH_REMD(); }

    typedef std::vector<CphResidue> Rarray;
    typedef Rarray::const_iterator const_iterator;
    // Residue functions
    void SetResidueInfo(Rarray const& r) { residues_ = r; }
    Rarray const& Residues()       const { return residues_; }
    CphResidue const& Res(int idx) const { return residues_[idx]; }

    typedef Farray::const_iterator ph_iterator;
    Farray const& pH_Values() const { return solvent_pH_; }

    Iarray const& ResStates() const { return resStates_; }

    void AddState(Iarray const& states, float pH) {
      for (Iarray::const_iterator it = states.begin(); it != states.end(); ++it)
        resStates_.push_back( *it );
      solvent_pH_.push_back( pH );
    }

    void Resize(size_t);
    // ----- DataSet functions -------------------
    size_t Size() const { return solvent_pH_.size(); }
    void Info()   const;
    void WriteBuffer(CpptrajFile&, SizeArray const&) const { return; }
    /// Reserve space for states of each residue
    int Allocate(SizeArray const&);
    void Add( size_t, const void* ) { return; }
    int Append(DataSet*)            { return 1; }
#   ifdef MPI
    int Sync(size_t, std::vector<int> const&, Parallel::Comm const&) { return 1; }
    void Consolidate(Parallel::Comm const&, int);
#   endif
  private:
    Rarray residues_;      ///< Array of residues.
    Farray solvent_pH_;    ///< Solvent pH values each frame
    Iarray resStates_;     ///< State of each residue each frame: R00, R10, R20, R01, ...
};
#endif
