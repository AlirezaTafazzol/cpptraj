#ifndef INC_ACTION_HYDROGENBOND_H
#define INC_ACTION_HYDROGENBOND_H
#include <map>
#include "Action.h"
#include "ImagedAction.h"
#include "DataSet_integer.h"
#include "Timer.h"
class Action_HydrogenBond : public Action {
  public:
    Action_HydrogenBond();
    DispatchObject* Alloc() const { return (DispatchObject*)new Action_HydrogenBond(); }
    void Help() const;
  private:
    Action::RetType Init(ArgList&, ActionInit&, int);
    Action::RetType Setup(ActionSetup&);
    Action::RetType DoAction(int, ActionFrame&);
#   ifdef MPI
    int SyncAction();
    Parallel::Comm trajComm_;
#   endif
    void Print();

    typedef std::vector<int> Iarray;

    class Site;
    class Hbond;

    inline double Angle(const double*, const double*, const double*) const;
    void CalcSiteHbonds(int,int,double,Site const&,const double*,int,const double*,
                        Frame const&, int&);

    typedef std::vector<Site> Sarray;
    typedef std::map<int,Hbond> HBmapType;

    ImagedAction Image_; ///< Hold imaging info.
    Sarray Donor_;    ///< Array of sites that are just donor.
    Sarray Both_;     ///< Array of sites that are donor and acceptor
    Iarray Acceptor_; ///< Array of acceptor-only atom indices

    HBmapType UU_Map_;
    HBmapType UV_Map_;

    std::string hbsetname_;
    AtomMask DonorMask_;
    AtomMask DonorHmask_;
    AtomMask AcceptorMask_;
    AtomMask SolventDonorMask_;
    AtomMask SolventAcceptorMask_;
    AtomMask Mask_;
    Matrix_3x3 ucell_, recip_;
    Timer t_action_;
    Topology* CurrentParm_; ///< Used to set atom/residue labels
    DataSetList* masterDSL_;
    DataSet* NumHbonds_;
    DataSet* NumSolvent_;
    DataSet* NumBridge_;
    DataSet* BridgeID_;
    DataFile* UUseriesout_;
    DataFile* UVseriesout_;
    CpptrajFile* avgout_;
    CpptrajFile* solvout_;
    CpptrajFile* bridgeout_;
    double dcut2_;
    double acut_;
    int debug_;
    bool series_;
    bool useAtomNum_;
    bool noIntramol_;
    bool hasDonorMask_;
    bool hasDonorHmask_;
    bool hasAcceptorMask_;
    bool hasSolventDonor_;
    bool calcSolvent_;
    bool hasSolventAcceptor_;
};

// ----- CLASSES ---------------------------------------------------------------
class Action_HydrogenBond::Site {
  public:
    Site() : idx_(-1), isV_(false) {}
    /// Solute site - heavy atom, hydrogen atom
    Site(int d, int h) : hlist_(1,h), idx_(d), isV_(false) {}
    /// Solute site - heavy atom, list of hydrogen atoms
    Site(int d, Iarray const& H) : hlist_(H), idx_(d), isV_(false) {}
    /// \return heavy atom index
    int Idx() const { return idx_; }
    /// \return iterator to beginning of hydrogen indices
    Iarray::const_iterator Hbegin() const { return hlist_.begin(); }
    /// \return iterator to end of hydrogen indices
    Iarray::const_iterator Hend()   const { return hlist_.end(); }
  private:
    Iarray hlist_; ///< List of hydrogen indices
    int idx_;      ///< Heavy atom index
    bool isV_;     ///< True if site is solvent
};

class Action_HydrogenBond::Hbond {
  public:
    Hbond() : dist_(0.0), angle_(0.0), data_(0), A_(-1), H_(-1), D_(-1), frames_(0) {}
    /// New hydrogen bond
    Hbond(double d, double a, DataSet_integer* s, int ia, int ih, int id) :
      dist_(d), angle_(a), data_(s), A_(ia), H_(ih), D_(id), frames_(1) {}
    void Update(double d, double a, int f) {
      dist_ += d;
      angle_ += a;
      if (data_ != 0) data_->AddVal(f, 1);
    }
  private:
    double dist_;  ///< Used to calculate average distance of hydrogen bond
    double angle_; ///< Used to calculate average angle of hydrogen bond
    DataSet_integer* data_; ///< Hold time series data
    int A_; ///< Acceptor atom index
    int H_; ///< Hydrogen atom index
    int D_; ///< Donor atom index
    int frames_; ///< # frames this hydrogen bond has been present
};
#endif
