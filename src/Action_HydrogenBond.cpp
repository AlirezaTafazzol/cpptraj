#include <cmath> // sqrt
#include <algorithm> // sort TODO may be able to do without this
#include "Action_HydrogenBond.h"
#include "CpptrajStdio.h"
#include "Constants.h"
#include "TorsionRoutines.h"
#include "StringRoutines.h" // ByteString

// CONSTRUCTOR
Action_HydrogenBond::Action_HydrogenBond() :
  CurrentParm_(0),
  masterDSL_(0),
  NumHbonds_(0),
  NumSolvent_(0),
  NumBridge_(0),
  BridgeID_(0),
  UUseriesout_(0),
  UVseriesout_(0),
  avgout_(0),
  solvout_(0),
  bridgeout_(0),
  dcut2_(0.0),
  acut_(0.0),
  Nframes_(0),
  debug_(0),
  series_(0),
  useAtomNum_(false),
  noIntramol_(false),
  hasDonorMask_(false),
  hasDonorHmask_(false),
  hasAcceptorMask_(false),
  hasSolventDonor_(false),
  calcSolvent_(false),
  hasSolventAcceptor_(false)
{}

// void Action_HydrogenBond::Help()
void Action_HydrogenBond::Help() const {
  mprintf("\t[<dsname>] [out <filename>] [<mask>] [angle <acut>] [dist <dcut>]\n"
          "\t[donormask <dmask> [donorhmask <dhmask>]] [acceptormask <amask>]\n"
          "\t[avgout <filename>] [printatomnum] [nointramol] [image]\n"
          "\t[solventdonor <sdmask>] [solventacceptor <samask>]\n"
          "\t[solvout <filename>] [bridgeout <filename>]\n"
          "\t[series [uuseries <filename>] [uvseries <filename>]]\n"
          "  Hydrogen bond is defined as A-HD, where A is acceptor heavy atom, H is\n"
          "  hydrogen, D is donor heavy atom. Hydrogen bond is formed when\n"
          "  A to D distance < dcut and A-H-D angle > acut; if acut < 0 it is ignored.\n"
          "  Search for hydrogen bonds using atoms in the region specified by mask.\n"
          "  If just <mask> specified donors and acceptors will be automatically searched for.\n"
          "  If donormask is specified but not acceptormask, acceptors will be\n"
          "  automatically searched for in <mask>.\n"
          "  If acceptormask is specified but not donormask, donors will be automatically\n"
          "  searched for in <mask>.\n"
          "  If both donormask and acceptor mask are specified no automatic searching will occur.\n"
          "  If donorhmask is specified atoms in that mask will be paired with atoms in\n"
          "  donormask instead of automatically searching for hydrogen atoms.\n");
}

// Action_HydrogenBond::Init()
Action::RetType Action_HydrogenBond::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
# ifdef MPI
  trajComm_ = init.TrajComm();
# endif
  debug_ = debugIn;
  // Get keywords
  Image_.InitImaging( (actionArgs.hasKey("image")) );
  DataFile* DF = init.DFL().AddDataFile( actionArgs.GetStringKey("out"), actionArgs );
  series_ = actionArgs.hasKey("series");
  if (series_) {
    UUseriesout_ = init.DFL().AddDataFile(actionArgs.GetStringKey("uuseries"), actionArgs);
    UVseriesout_ = init.DFL().AddDataFile(actionArgs.GetStringKey("uvseries"), actionArgs);
    init.DSL().SetDataSetsPending(true);
  }
  std::string avgname = actionArgs.GetStringKey("avgout");
  std::string solvname = actionArgs.GetStringKey("solvout");
  if (solvname.empty()) solvname = avgname;
  std::string bridgename = actionArgs.GetStringKey("bridgeout");
  if (bridgename.empty()) bridgename = solvname;
  
  useAtomNum_ = actionArgs.hasKey("printatomnum");
  acut_ = actionArgs.getKeyDouble("angle",135.0);
  noIntramol_ = actionArgs.hasKey("nointramol");
  // Convert angle cutoff to radians
  acut_ *= Constants::DEGRAD;
  double dcut = actionArgs.getKeyDouble("dist",3.0);
  dcut = actionArgs.getKeyDouble("distance", dcut); // for PTRAJ compat.
  dcut2_ = dcut * dcut;
  // Get donor mask
  std::string mask = actionArgs.GetStringKey("donormask");
  if (!mask.empty()) {
    DonorMask_.SetMaskString(mask);
    hasDonorMask_=true;
    // Get donorH mask (if specified)
    mask = actionArgs.GetStringKey("donorhmask");
    if (!mask.empty()) {
      DonorHmask_.SetMaskString(mask);
      hasDonorHmask_=true;
    }
  }
  // Get acceptor mask
  mask = actionArgs.GetStringKey("acceptormask");
  if (!mask.empty()) {
    AcceptorMask_.SetMaskString(mask);
    hasAcceptorMask_=true;
  }
  // Get solvent donor mask
  mask = actionArgs.GetStringKey("solventdonor");
  if (!mask.empty()) {
    SolventDonorMask_.SetMaskString(mask);
    hasSolventDonor_ = true;
    calcSolvent_ = true;
  }
  // Get solvent acceptor mask
  mask = actionArgs.GetStringKey("solventacceptor");
  if (!mask.empty()) {
    SolventAcceptorMask_.SetMaskString(mask);
    hasSolventAcceptor_ = true;
    calcSolvent_ = true;
  }
  // Get generic mask
  Mask_.SetMaskString(actionArgs.GetMaskNext());

  // Setup datasets
  hbsetname_ = actionArgs.GetStringNext();
  if (hbsetname_.empty())
    hbsetname_ = init.DSL().GenerateDefaultName("HB");
  NumHbonds_ = init.DSL().AddSet(DataSet::INTEGER, MetaData(hbsetname_, "UU"));
  if (NumHbonds_==0) return Action::ERR;
  if (DF != 0) DF->AddDataSet( NumHbonds_ );
  avgout_ = init.DFL().AddCpptrajFile(avgname, "Avg. solute-solute HBonds");
  if (calcSolvent_) {
    NumSolvent_ = init.DSL().AddSet(DataSet::INTEGER, MetaData(hbsetname_, "UV"));
    if (NumSolvent_ == 0) return Action::ERR;
    if (DF != 0) DF->AddDataSet( NumSolvent_ );
    NumBridge_ = init.DSL().AddSet(DataSet::INTEGER, MetaData(hbsetname_, "Bridge"));
    if (NumBridge_ == 0) return Action::ERR;
    if (DF != 0) DF->AddDataSet( NumBridge_ );
    BridgeID_ = init.DSL().AddSet(DataSet::STRING, MetaData(hbsetname_, "ID"));
    if (BridgeID_ == 0) return Action::ERR;
    if (DF != 0) DF->AddDataSet( BridgeID_ );
    solvout_ = init.DFL().AddCpptrajFile(solvname,"Avg. solute-solvent HBonds");
    bridgeout_ = init.DFL().AddCpptrajFile(bridgename,"Solvent bridging info");
  }

  mprintf( "  HBOND: ");
  if (!hasDonorMask_ && !hasAcceptorMask_)
    mprintf("Searching for Hbond donors/acceptors in region specified by %s\n",
            Mask_.MaskString());
  else if (hasDonorMask_ && !hasAcceptorMask_)
    mprintf("Donor mask is %s, acceptors will be searched for in region specified by %s\n",
            DonorMask_.MaskString(), Mask_.MaskString());
  else if (hasAcceptorMask_ && !hasDonorMask_)
    mprintf("Acceptor mask is %s, donors will be searched for in a region specified by %s\n",
            AcceptorMask_.MaskString(), Mask_.MaskString());
  else
    mprintf("Donor mask is %s, Acceptor mask is %s\n",
            DonorMask_.MaskString(), AcceptorMask_.MaskString());
  if (hasDonorHmask_)
    mprintf("\tSeparate donor H mask is %s\n", DonorHmask_.MaskString() );
  if (noIntramol_)
    mprintf("\tOnly looking for intermolecular hydrogen bonds.\n");
  if (hasSolventDonor_)
    mprintf("\tWill search for hbonds between solute and solvent donors in [%s]\n",
            SolventDonorMask_.MaskString());
  if (hasSolventAcceptor_)
    mprintf("\tWill search for hbonds between solute and solvent acceptors in [%s]\n",
            SolventAcceptorMask_.MaskString());
  mprintf("\tDistance cutoff = %.3f, Angle Cutoff = %.3f\n",dcut,acut_*Constants::RADDEG);
  if (DF != 0) 
    mprintf("\tWriting # Hbond v time results to %s\n", DF->DataFilename().full());
  if (avgout_ != 0)
    mprintf("\tWriting Hbond avgs to %s\n",avgout_->Filename().full());
  if (calcSolvent_ && solvout_ != 0)
    mprintf("\tWriting solute-solvent hbond avgs to %s\n", solvout_->Filename().full());
  if (calcSolvent_ && bridgeout_ != 0)
    mprintf("\tWriting solvent bridging info to %s\n", bridgeout_->Filename().full());
  if (useAtomNum_)
    mprintf("\tAtom numbers will be written to output.\n");
  if (series_) {
    mprintf("\tTime series data for each hbond will be saved for analysis.\n");
    if (UUseriesout_ != 0) mprintf("\tWriting solute-solute time series to %s\n",
                                   UUseriesout_->DataFilename().full());
    if (UVseriesout_ != 0) mprintf("\tWriting solute-solvent time series to %s\n",
                                   UVseriesout_->DataFilename().full());
  }
  if (Image_.UseImage())
    mprintf("\tImaging enabled.\n");
  masterDSL_ = init.DslPtr();

  return Action::OK;
}

//  IsFON()
inline bool IsFON(Atom const& atm) {
  return (atm.Element() == Atom::FLUORINE ||
          atm.Element() == Atom::OXYGEN ||
          atm.Element() == Atom::NITROGEN);
}

// Action_HydrogenBond::Setup()
Action::RetType Action_HydrogenBond::Setup(ActionSetup& setup) {
  CurrentParm_ = setup.TopAddress();
  Image_.SetupImaging( setup.CoordInfo().TrajBox().Type() );

  // Set up generic mask
  if (!hasDonorMask_ || !hasAcceptorMask_) {
    if ( setup.Top().SetupIntegerMask( Mask_ ) ) return Action::ERR;
    if ( Mask_.None() ) {
      mprintf("Warning: Mask has no atoms.\n");
      return Action::SKIP;
    }
  }

  // ACCEPTOR MASK SETUP
  if (hasAcceptorMask_) {
    // Acceptor mask specified
    if ( setup.Top().SetupIntegerMask( AcceptorMask_ ) ) return Action::ERR;
    if (AcceptorMask_.None()) {
      mprintf("Warning: AcceptorMask has no atoms.\n");
      return Action::SKIP;
    }
  } else {
    // No specified acceptor mask; search generic mask.
    AcceptorMask_.ResetMask();
    for (AtomMask::const_iterator at = Mask_.begin(); at != Mask_.end(); ++at) {
      // Since an acceptor mask was not specified ignore solvent.
      int molnum = setup.Top()[*at].MolNum();
      if (!setup.Top().Mol(molnum).IsSolvent() && IsFON( setup.Top()[*at] ))
        AcceptorMask_.AddSelectedAtom( *at );
    }
    AcceptorMask_.SetNatoms( Mask_.NmaskAtoms() );
  }

  // SOLUTE DONOR/ACCEPTOR SITE SETUP
  Sarray donorOnly;
  if (hasDonorMask_) {
    // Donor heavy atom mask specified
    if ( setup.Top().SetupIntegerMask( DonorMask_ ) ) return Action::ERR;
    if (DonorMask_.None()) {
      mprintf("Warning: DonorMask has no atoms.\n");
      return Action::SKIP;
    }
    if ( hasDonorHmask_ ) {
      // Donor hydrogen mask also specified
      if ( setup.Top().SetupIntegerMask( DonorHmask_ ) ) return Action::ERR;
      if ( DonorHmask_.None() ) {
        mprintf("Warning: Donor H mask has no atoms.\n");
        return Action::SKIP;
      }
      if ( DonorHmask_.Nselected() != DonorMask_.Nselected() ) {
        mprinterr("Error: There is not a 1 to 1 correspondance between donor and donorH masks.\n");
        mprinterr("Error: donor (%i atoms), donorH (%i atoms).\n",DonorMask_.Nselected(),
                  DonorHmask_.Nselected());
        return Action::ERR;
      }
      int maxAtom = std::max( AcceptorMask_.back(), DonorMask_.back() ) + 1;
      AtomMask::const_iterator a_atom = AcceptorMask_.begin();
      AtomMask::const_iterator d_atom = DonorMask_.begin();
      AtomMask::const_iterator h_atom = DonorHmask_.begin();
      bool isDonor, isAcceptor;
      int H_at = -1;
      for (int at = 0; at != maxAtom; at++)
      {
        isDonor = false;
        isAcceptor = false;
        if ( d_atom != DonorMask_.end() && *d_atom == at ) {
          isDonor = true;
          ++d_atom;
          H_at = *(h_atom++);
        } 
        if ( a_atom != AcceptorMask_.end() && *a_atom == at ) {
          isAcceptor = true;
          ++a_atom;
        }
        if (isDonor && isAcceptor)
          Both_.push_back( Site(at, H_at) );
        else if (isDonor)
          donorOnly.push_back( Site(at, H_at) );
        else if (isAcceptor)
          Acceptor_.push_back( at );
      }
    } else {
      // No donor hydrogen mask; use any hydrogens bonded to donor heavy atoms.
      int maxAtom = std::max( AcceptorMask_.back(), DonorMask_.back() ) + 1;
      AtomMask::const_iterator a_atom = AcceptorMask_.begin();
      AtomMask::const_iterator d_atom = DonorMask_.begin();
      bool isDonor, isAcceptor;
      for (int at = 0; at != maxAtom; at++)
      {
        Iarray Hatoms;
        isDonor = false;
        isAcceptor = false;
        if ( d_atom != DonorMask_.end() && *d_atom == at ) {
          ++d_atom;
          for (Atom::bond_iterator H_at = setup.Top()[at].bondbegin();
                                   H_at != setup.Top()[at].bondend(); ++H_at)
            if (setup.Top()[*H_at].Element() == Atom::HYDROGEN)
              Hatoms.push_back( *H_at );
          isDonor = !Hatoms.empty();
        }
        if ( a_atom != AcceptorMask_.end() && *a_atom == at ) {
          isAcceptor = true;
          ++a_atom;
        }
        if (isDonor && isAcceptor)
          Both_.push_back( Site(at, Hatoms) );
        else if (isDonor)
          donorOnly.push_back( Site(at, Hatoms) );
        else if (isAcceptor)
          Acceptor_.push_back( at );
      }
    }
  } else {
    // No specified donor mask; search generic mask.
    int maxAtom = std::max( AcceptorMask_.back(), Mask_.back() ) + 1;
    AtomMask::const_iterator a_atom = AcceptorMask_.begin();
    AtomMask::const_iterator d_atom = Mask_.begin();
    bool isDonor, isAcceptor;
    for (int at = 0; at != maxAtom; at++)
    {
      // Since an acceptor mask was not specified ignore solvent.
      int molnum = setup.Top()[at].MolNum();
      if (!setup.Top().Mol(molnum).IsSolvent())
      {
        Iarray Hatoms;
        isDonor = false;
        isAcceptor = false;
        if ( d_atom != Mask_.end() && *d_atom == at) {
          ++d_atom;
          if ( IsFON( setup.Top()[at] ) ) {
            for (Atom::bond_iterator H_at = setup.Top()[at].bondbegin();
                                     H_at != setup.Top()[at].bondend(); ++H_at)
              if (setup.Top()[*H_at].Element() == Atom::HYDROGEN)
                Hatoms.push_back( *H_at );
            isDonor = !Hatoms.empty();
          }
        }
        if ( a_atom != AcceptorMask_.end() && *a_atom == at ) {
          isAcceptor = true;
          ++a_atom;
        }
        if (isDonor && isAcceptor)
          Both_.push_back( Site(at, Hatoms) );
        else if (isDonor)
          donorOnly.push_back( Site(at, Hatoms) );
        else if (isAcceptor)
          Acceptor_.push_back( at );
      }
    }
  }
  // Place donor-only sites at the end of Both_
  bothEnd_ = Both_.size();
  for (Sarray::const_iterator site = donorOnly.begin(); site != donorOnly.end(); ++site)
    Both_.push_back( *site );

  mprintf("Acceptor atoms (%zu):\n", Acceptor_.size());
  for (Iarray::const_iterator at = Acceptor_.begin(); at != Acceptor_.end(); ++at)
    mprintf("\t%20s %8i\n", setup.Top().TruncResAtomName(*at).c_str(), *at+1);
  mprintf("Donor/acceptor sites (%u):\n", bothEnd_);
  Sarray::const_iterator END = Both_.begin() + bothEnd_;
  for (Sarray::const_iterator si = Both_.begin(); si != END; ++si) {
    mprintf("\t%20s %8i", setup.Top().TruncResAtomName(si->Idx()).c_str(), si->Idx()+1);
    for (Iarray::const_iterator at = si->Hbegin(); at != si->Hend(); ++at)
      mprintf(" %s", setup.Top()[*at].c_str());
    mprintf("\n");
  }
  mprintf("Donor sites (%zu):\n", Both_.size() - bothEnd_);
  for (Sarray::const_iterator si = END; si != Both_.end(); ++si) {
    mprintf("\t%20s %8i", setup.Top().TruncResAtomName(si->Idx()).c_str(), si->Idx()+1);
    for (Iarray::const_iterator at = si->Hbegin(); at != si->Hend(); ++at)
      mprintf(" %s", setup.Top()[*at].c_str());
    mprintf("\n");
  }

  // SOLVENT SITE SETUP
  if (calcSolvent_) {
    int at_beg = 0;
    int at_end = 0;
    // Set up solvent donor/acceptor masks
    if (hasSolventDonor_) {
      if (setup.Top().SetupIntegerMask( SolventDonorMask_ )) return Action::ERR;
      if (SolventDonorMask_.None()) {
        mprintf("Warning: SolventDonorMask has no atoms.\n");
        return Action::SKIP;
      }
      at_beg = SolventDonorMask_[0];
      at_end = SolventDonorMask_.back() + 1;
    }
    if (hasSolventAcceptor_) {
      if (setup.Top().SetupIntegerMask( SolventAcceptorMask_ )) return Action::ERR;
      if (SolventAcceptorMask_.None()) {
        mprintf("Warning: SolventAcceptorMask has no atoms.\n");
        return Action::SKIP;
      }
      if (!hasSolventDonor_) {
        at_beg = SolventAcceptorMask_[0];
        at_end = SolventAcceptorMask_.back() + 1;
      } else {
        at_beg = std::min( SolventDonorMask_[0], SolventAcceptorMask_[0] );
        at_end = std::max( SolventDonorMask_.back(), SolventAcceptorMask_.back() ) + 1;
      }
    }
    AtomMask::const_iterator a_atom = SolventAcceptorMask_.begin();
    AtomMask::const_iterator d_atom = SolventDonorMask_.begin();
    bool isDonor, isAcceptor;
    for (int at = at_beg; at != at_end; at++)
    {
      Iarray Hatoms;
      isDonor = false;
      isAcceptor = false;
      if ( d_atom != SolventDonorMask_.end() && *d_atom == at ) {
        ++d_atom;
        if ( IsFON( setup.Top()[at] ) ) {
          for (Atom::bond_iterator H_at = setup.Top()[at].bondbegin();
                                   H_at != setup.Top()[at].bondend(); ++H_at)
            if (setup.Top()[*H_at].Element() == Atom::HYDROGEN)
              Hatoms.push_back( *H_at );
        }
        isDonor = !Hatoms.empty();
      }
      if ( a_atom != SolventAcceptorMask_.end() && *a_atom == at ) {
        isAcceptor = true;
        ++a_atom;
      }
      if (isDonor || isAcceptor)
        SolventSites_.push_back( Site(at, Hatoms) );
    }

    mprintf("Solvent sites (%zu):\n", SolventSites_.size());
    for (Sarray::const_iterator si = SolventSites_.begin(); si != SolventSites_.end(); ++si) {
      mprintf("\t%20s %8i", setup.Top().TruncResAtomName(si->Idx()).c_str(), si->Idx()+1);
      for (Iarray::const_iterator at = si->Hbegin(); at != si->Hend(); ++at)
        mprintf(" %s", setup.Top()[*at].c_str());
      mprintf("\n");
    }
  }

  return Action::OK;
}

// Action_HydrogenBond::Angle()
double Action_HydrogenBond::Angle(const double* XA, const double* XH, const double* XD) const
{
  if (acut_ < 0.0) // Indicates skip angle calc
    return 0.0;
  if (Image_.ImageType() == NOIMAGE)
    return (CalcAngle(XA, XH, XD));
  else {
    double angle;
    Vec3 VH = Vec3(XH);
    Vec3 H_A = MinImagedVec(VH, Vec3(XA), ucell_, recip_);
    Vec3 H_D = Vec3(XD) - VH;
    double rha = H_A.Magnitude2();
    double rhd = H_D.Magnitude2();
    if (rha > Constants::SMALL && rhd > Constants::SMALL) {
      angle = (H_A * H_D) / sqrt(rha * rhd);
      if      (angle >  1.0) angle =  1.0;
      else if (angle < -1.0) angle = -1.0;
      angle = acos(angle);
    } else
      angle = 0.0;
    return angle;
  }
}

//  Action_HydrogenBond::CalcSolvHbonds()
void Action_HydrogenBond::CalcSolvHbonds(int frameNum, double dist2,
                                         Site const& SiteD, const double* XYZD,
                                         int a_atom,        const double* XYZA,
                                         Frame const& frmIn, int& numHB, bool soluteDonor)
{
  // The two sites are close enough to hydrogen bond.
  int d_atom = SiteD.Idx();
  // Determine if angle cutoff is satisfied
  for (Iarray::const_iterator h_atom = SiteD.Hbegin(); h_atom != SiteD.Hend(); ++h_atom)
  {
    double angle = 0.0;
    bool angleSatisfied = true;
    // For ions, donor atom will be same as h atom so no angle needed.
    if (d_atom != *h_atom) {
      angle = Angle(XYZA, frmIn.XYZ(*h_atom), XYZD);
      angleSatisfied = !(angle < acut_);
    }
    if (angleSatisfied)
    {
      ++numHB;
      double dist = sqrt(dist2);
      // Index U-H .. V hydrogen bonds by solute H atom.
      // Index U .. H-V hydrogen bonds by solute A atom.
      int hbidx;
      if (soluteDonor)
        hbidx = *h_atom;
      else
        hbidx = a_atom;
      UVmapType::iterator it = UV_Map_.lower_bound( hbidx );
      if (it == UV_Map_.end() || it->first != hbidx)
      {
//        mprintf("DBG1: NEW hbond : %8i .. %8i - %8i\n", a_atom+1,*h_atom+1,d_atom+1);
        DataSet_integer* ds = 0;
        if (series_) {
          ds = (DataSet_integer*)
               masterDSL_->AddSet(DataSet::INTEGER,MetaData(hbsetname_,"solventhb",hbidx));
          if (UVseriesout_ != 0) UVseriesout_->AddDataSet( ds );
          ds->AddVal( frameNum, 1 );
        }
        Hbond hb;
        if (soluteDonor) { // Do not care about which solvent acceptor
          if (ds != 0) ds->SetLegend( CurrentParm_->TruncResAtomName(*h_atom) + "-V" );
          hb = Hbond(dist,angle,ds,-1,*h_atom,d_atom);
        } else {           // Do not care about which solvent donor
          if (ds != 0) ds->SetLegend( CurrentParm_->TruncResAtomName(a_atom) + "-V" );
          hb = Hbond(dist,angle,ds,a_atom,-1,-1);
        }
        UV_Map_.insert(it, std::pair<int,Hbond>(hbidx,hb));
      } else {
//        mprintf("DBG1: OLD hbond : %8i .. %8i - %8i\n", a_atom+1,*h_atom+1,d_atom+1);
        it->second.Update(dist,angle,frameNum);
      }
    }
  }
}

// Action_HydrogenBond::CalcSiteHbonds()
void Action_HydrogenBond::CalcSiteHbonds(int frameNum, double dist2,
                                         Site const& SiteD, const double* XYZD,
                                         int a_atom,        const double* XYZA,
                                         Frame const& frmIn, int& numHB)
{
  // The two sites are close enough to hydrogen bond.
  int d_atom = SiteD.Idx();
  // Determine if angle cutoff is satisfied
  for (Iarray::const_iterator h_atom = SiteD.Hbegin(); h_atom != SiteD.Hend(); ++h_atom)
  {
    double angle = Angle(XYZA, frmIn.XYZ(*h_atom), XYZD);
    if ( !(angle < acut_) )
    {
      ++numHB;
      double dist = sqrt(dist2);
      // Index UU hydrogen bonds by DonorH-Acceptor
      Hpair hbidx(*h_atom, a_atom);
      UUmapType::iterator it = UU_Map_.lower_bound( hbidx );
      if (it == UU_Map_.end() || it->first != hbidx)
      {
//        mprintf("DBG1: NEW hbond : %8i .. %8i - %8i\n", a_atom+1,*h_atom+1,d_atom+1);
        DataSet_integer* ds = 0;
        if (series_) {
          std::string hblegend = CurrentParm_->TruncResAtomName(a_atom) + "-" +
                                 CurrentParm_->TruncResAtomName(d_atom) + "-" +
                                 (*CurrentParm_)[*h_atom].Name().Truncated();
          ds = (DataSet_integer*)
               masterDSL_->AddSet(DataSet::INTEGER,MetaData(hbsetname_,"solutehb",UU_Map_.size()));
          if (UUseriesout_ != 0) UUseriesout_->AddDataSet( ds );
          ds->SetLegend( hblegend );
          ds->AddVal( frameNum, 1 );
        }
        UU_Map_.insert(it, std::pair<Hpair,Hbond>(hbidx,Hbond(dist,angle,ds,a_atom,*h_atom,d_atom)));
      } else {
//        mprintf("DBG1: OLD hbond : %8i .. %8i - %8i\n", a_atom+1,*h_atom+1,d_atom+1);
        it->second.Update(dist,angle,frameNum);
      }
    }
  }
}

// Action_HydrogenBond::DoAction()
Action::RetType Action_HydrogenBond::DoAction(int frameNum, ActionFrame& frm) {
  t_action_.Start();
  if (Image_.ImagingEnabled())
    frm.Frm().BoxCrd().ToRecip(ucell_, recip_);

  // Loop over all solute donor sites
  t_uu_.Start();
  int numHB = 0;
  for (unsigned int sidx0 = 0; sidx0 != Both_.size(); sidx0++)
  {
    const double* XYZ0 = frm.Frm().XYZ( Both_[sidx0].Idx() );
    // Loop over solute sites that can be both donor and acceptor
    for (unsigned int sidx1 = sidx0 + 1; sidx1 < bothEnd_; sidx1++)
    {
      const double* XYZ1 = frm.Frm().XYZ( Both_[sidx1].Idx() );
      double dist2 = DIST2( XYZ0, XYZ1, Image_.ImageType(), frm.Frm().BoxCrd(), ucell_, recip_ );
      if ( !(dist2 > dcut2_) )
      {
        // Site 0 donor, Site 1 acceptor
        CalcSiteHbonds(frameNum, dist2, Both_[sidx0], XYZ0, Both_[sidx1].Idx(), XYZ1, frm.Frm(), numHB);
        // Site 1 donor, Site 0 acceptor
        CalcSiteHbonds(frameNum, dist2, Both_[sidx1], XYZ1, Both_[sidx0].Idx(), XYZ0, frm.Frm(), numHB);
      }
    }
    // Loop over solute acceptor-only
    for (Iarray::const_iterator a_atom = Acceptor_.begin(); a_atom != Acceptor_.end(); ++a_atom)
    {
      const double* XYZ1 = frm.Frm().XYZ( *a_atom );
      double dist2 = DIST2( XYZ0, XYZ1, Image_.ImageType(), frm.Frm().BoxCrd(), ucell_, recip_ );
      if ( !(dist2 > dcut2_) )
        CalcSiteHbonds(frameNum, dist2, Both_[sidx0], XYZ0, *a_atom, XYZ1, frm.Frm(), numHB);
    }
  }
  NumHbonds_->Add(frameNum, &numHB);
  t_uu_.Stop();

  // Loop over all solvent sites
  if (calcSolvent_) {
    t_uv_.Start();
    numHB = 0;
    for (unsigned int vidx = 0; vidx != SolventSites_.size(); vidx++)
    {
      Site const& Vsite = SolventSites_[vidx];
      const double* VXYZ = frm.Frm().XYZ( Vsite.Idx() );
      // Loop over solute sites that can be both donor and acceptor
      for (unsigned int sidx = 0; sidx < bothEnd_; sidx++)
      {
        const double* UXYZ = frm.Frm().XYZ( Both_[sidx].Idx() );
        double dist2 = DIST2( VXYZ, UXYZ, Image_.ImageType(), frm.Frm().BoxCrd(), ucell_, recip_ );
        if ( !(dist2 > dcut2_) )
        {
          // Solvent site donor, solute site acceptor
          CalcSolvHbonds(frameNum, dist2, Vsite, VXYZ, Both_[sidx].Idx(), UXYZ, frm.Frm(), numHB, false);
          // Solvent site acceptor, solute site donor
          CalcSolvHbonds(frameNum, dist2, Both_[sidx], UXYZ, Vsite.Idx(), VXYZ, frm.Frm(), numHB, true);
        }
      }
      // Loop over solute sites that are donor only
      for (unsigned int sidx = bothEnd_; sidx < Both_.size(); sidx++)
      {
        const double* UXYZ = frm.Frm().XYZ( Both_[sidx].Idx() );
        double dist2 = DIST2( VXYZ, UXYZ, Image_.ImageType(), frm.Frm().BoxCrd(), ucell_, recip_ );
        if ( !(dist2 > dcut2_) )
          // Solvent site acceptor, solute site donor
          CalcSolvHbonds(frameNum, dist2, Both_[sidx], UXYZ, Vsite.Idx(), VXYZ, frm.Frm(), numHB, true);
      }
      // Loop over solute sites that are acceptor only
      for (Iarray::const_iterator a_atom = Acceptor_.begin(); a_atom != Acceptor_.end(); ++a_atom)
      {
        const double* UXYZ = frm.Frm().XYZ( *a_atom );
        double dist2 = DIST2( VXYZ, UXYZ, Image_.ImageType(), frm.Frm().BoxCrd(), ucell_, recip_ );
        if ( !(dist2 > dcut2_) )
          // Solvent site donor, solute site acceptor
          CalcSolvHbonds(frameNum, dist2, Vsite, VXYZ, *a_atom, UXYZ, frm.Frm(), numHB, false);
      }
    } // END loop over solvent sites
    NumSolvent_->Add(frameNum, &numHB);
    t_uv_.Stop();
  }

  Nframes_++;
  t_action_.Stop();
  return Action::OK;
}

#ifdef MPI
// TODO Use in other hbond functions
static inline std::string CreateHBlegend(Topology const& topIn, int a_atom, int h_atom, int d_atom)
{
  if (a_atom == -1)
    return (topIn.TruncResAtomName(h_atom) + "-V");
  else if (d_atom == -1)
    return (topIn.TruncResAtomName(a_atom) + "-V");
  else
    return (topIn.TruncResAtomName(a_atom) + "-" +
            topIn.TruncResAtomName(d_atom) + "-" +
            topIn[h_atom].Name().Truncated());
}

// Action_HydrogenBond::SyncMap
void Action_HydrogenBond::SyncMap(UUmapType& mapIn, std::vector<int> const& rank_frames,
                           std::vector<int> const& rank_offsets,
                           const char* aspect, Parallel::Comm const& commIn)
const
{
  // Need to know how many hbonds on each thread.
  int num_hb = (int)mapIn.size();
  std::vector<int> nhb_on_rank;
  if (commIn.Master())
    nhb_on_rank.resize( commIn.Size() );
  commIn.GatherMaster( &num_hb, 1, MPI_INT, &nhb_on_rank[0] );
  std::vector<double> dArray;
  std::vector<int> iArray;
  if (commIn.Master()) {
    for (int rank = 1; rank < commIn.Size(); rank++) {
      if (nhb_on_rank[rank] > 0) {
        //mprintf("DEBUG:\tReceiving %i hbonds from rank %i.\n", nhb_on_rank[rank], rank);
        dArray.resize( 2 * nhb_on_rank[rank] );
        iArray.resize( 5 * nhb_on_rank[rank] );
        commIn.Recv( &(dArray[0]), dArray.size(), MPI_DOUBLE, rank, 1300 );
        commIn.Recv( &(iArray[0]), iArray.size(), MPI_INT,    rank, 1301 );
        HbondType HB;
        int ii = 0, id = 0;
        for (int in = 0; in != nhb_on_rank[rank]; in++, ii += 5, id += 2) {
          UUmapType::iterator it = mapIn.find( iArray[ii] ); // hbidx
          if (it == mapIn.end() ) {
            // Hbond on rank that has not been found on master
            HB.dist  = dArray[id  ];
            HB.angle = dArray[id+1];
            HB.A      = iArray[ii+1];
            HB.H      = iArray[ii+2];
            HB.D      = iArray[ii+3];
            HB.Frames = iArray[ii+4];
            HB.data_ = 0;
            //mprintf("\tNEW Hbond %i: %i-%i-%i D=%g A=%g %i frames", iArray[ii],
            //        HB.A+1, HB.H+1, HB.D+1, HB.dist,
            //        HB.angle, HB.Frames);
            if (series_) {
              HB.data_ = (DataSet_integer*)
                         masterDSL_->AddSet( DataSet::INTEGER,
                                             MetaData(hbsetname_, aspect, iArray[ii]) );
              // FIXME: This may be incorrect if CurrentParm_ has changed
              HB.data_->SetLegend( CreateHBlegend(*CurrentParm_, HB.A, HB.H, HB.D) );
             // mprintf(" \"%s\"", HB.data_->legend());
            }
            //mprintf("\n");
            mapIn.insert( it, std::pair<int,HbondType>(iArray[ii], HB) );
          } else {
            // Hbond on rank and master. Update on master.
            //mprintf("\tAPPENDING Hbond %i: %i-%i-%i D=%g A=%g %i frames\n", iArray[ii],
            //        it->second.A+1, it->second.H+1, it->second.D+1, dArray[id],
            //        dArray[id+1], iArray[ii+4]);
            it->second.dist  += dArray[id  ];
            it->second.angle += dArray[id+1];
            it->second.Frames += iArray[ii+4];
            if (series_)
              HB.data_ = it->second.data_;
          }
          if (series_) {
            HB.data_->Resize( Nframes_ );
            int* d_beg = HB.data_->Ptr() + rank_offsets[ rank ];
            //mprintf("\tResizing hbond series data to %i, starting frame %i, # frames %i\n",
            //        Nframes_, rank_offsets[rank], rank_frames[rank]);
            commIn.Recv( d_beg, rank_frames[ rank ], MPI_INT, rank, 1304 + in );
            HB.data_->SetNeedsSync( false );
          }
        } // END master loop over hbonds from rank
      }
    } // END master loop over ranks
    // At this point we have all hbond sets from all ranks. Mark all HB sets
    // smaller than Nframes_ as synced and ensure the time series has been
    // updated to reflect overall # frames.
    if (series_) {
      const int ZERO = 0;
      for (UUmapType::iterator hb = mapIn.begin(); hb != mapIn.end(); ++hb)
        if ((int)hb->second.data_->Size() < Nframes_) {
          hb->second.data_->SetNeedsSync( false );
          hb->second.data_->Add( Nframes_-1, &ZERO );
        }
    }
  } else {
    if (mapIn.size() > 0) {
      dArray.reserve( 2 * mapIn.size() );
      iArray.reserve( 5 * mapIn.size() );
      for (UUmapType::const_iterator hb = mapIn.begin(); hb != mapIn.end(); ++hb) {
        dArray.push_back( hb->second.dist );
        dArray.push_back( hb->second.angle );
        iArray.push_back( hb->first );
        iArray.push_back( hb->second.A );
        iArray.push_back( hb->second.H );
        iArray.push_back( hb->second.D );
        iArray.push_back( hb->second.Frames );
      }
      commIn.Send( &(dArray[0]), dArray.size(), MPI_DOUBLE, 0, 1300 );
      commIn.Send( &(iArray[0]), iArray.size(), MPI_INT,    0, 1301 );
      // Send series data to master
      if (series_) {
        int in = 0; // For tag
        for (UUmapType::const_iterator hb = mapIn.begin(); hb != mapIn.end(); ++hb, in++) {
          commIn.Send( hb->second.data_->Ptr(), hb->second.data_->Size(),
                                  MPI_INT, 0, 1304 + in );
          hb->second.data_->SetNeedsSync( false );
        }
      }
    }
  }
}

/** PARALLEL NOTES:
  * The following tags are used for MPI send/receive:
  *   1300  : Array containing hbond double info on rank.
  *   1301  : Array containing hbond integer info on rank.
  *   1302  : Number of bridges to expect from rank.
  *   1303  : Array containing bridge integer info on rank.
  *   1304+X: Array of hbond X series info from rank.
  */
int Action_HydrogenBond::SyncAction() {
  // Make sure all time series are updated at this point.
  UpdateSeries();
  // TODO consolidate # frames / offset calc code with Action_NAstruct
  // Get total number of frames
  std::vector<int> rank_frames( trajComm_.Size() );
  trajComm_.GatherMaster( &Nframes_, 1, MPI_INT, &rank_frames[0] );
  if (trajComm_.Master()) {
    for (int rank = 1; rank < trajComm_.Size(); rank++)
      Nframes_ += rank_frames[rank];
  }
  // Convert rank frames to offsets.
  std::vector<int> rank_offsets( trajComm_.Size(), 0 );
  if (trajComm_.Master()) {
    for (int rank = 1; rank < trajComm_.Size(); rank++)
      rank_offsets[rank] = rank_offsets[rank-1] + rank_frames[rank-1];
  }
  // Need to send hbond data from all ranks to master.
  SyncMap( UU_Map_, rank_frames, rank_offsets, "solutehb", trajComm_ );
  if (calcSolvent_) {
    SyncMap( UV_Map_, rank_frames, rank_offsets, "solventhb", trajComm_ );
    // iArray will contain for each bridge: Nres, res1, ..., resN, Frames
    std::vector<int> iArray;
    int iSize;
    if (trajComm_.Master()) {
      for (int rank = 1; rank < trajComm_.Size(); rank++)
      {
        // Receive size of iArray
        trajComm_.Recv( &iSize,           1, MPI_INT, rank, 1302 );
        //mprintf("DEBUG: Receiving %i bridges from rank %i\n", iSize, rank);
        iArray.resize( iSize );
        trajComm_.Recv( &(iArray[0]), iSize, MPI_INT, rank, 1303 );
        unsigned int idx = 0;
        while (idx < iArray.size()) {
          std::set<int> residues;
          unsigned int i2 = idx + 1;
          for (int ir = 0; ir != iArray[idx]; ir++, i2++)
            residues.insert( iArray[i2] );
          BridgeType::iterator b_it = BridgeMap_.find( residues );
          if (b_it == BridgeMap_.end() ) // New Bridge 
            BridgeMap_.insert( b_it, std::pair<std::set<int>,int>(residues, iArray[i2]) );
          else                           // Increment bridge #frames
            b_it->second += iArray[i2];
          idx = i2 + 1;
        }
      }
    } else {
       // Construct bridge info array.
       for (BridgeType::const_iterator b = BridgeMap_.begin(); b != BridgeMap_.end(); ++b)
       {
         iArray.push_back( b->first.size() ); // # of bridging res
         for ( std::set<int>::const_iterator r = b->first.begin(); r != b->first.end(); ++r)
           iArray.push_back( *r ); // Bridging res
         iArray.push_back( b->second ); // # frames
      }
      // Since the size of each bridge can be different (i.e. differing #s of
      // residues may be bridged), first send size of the transport array.
      iSize = (int)iArray.size();
      trajComm_.Send( &iSize,           1, MPI_INT, 0, 1302 );
      trajComm_.Send( &(iArray[0]), iSize, MPI_INT, 0, 1303 );
    }
  }
  return 0;
}
#endif

// Action_HydrogenBond::UpdateSeries()
void Action_HydrogenBond::UpdateSeries() {
  if (seriesUpdated_) return;
  if (series_ && Nframes_ > 0) {
    for (UUmapType::iterator hb = UU_Map_.begin(); hb != UU_Map_.end(); ++hb)
      hb->second.FinishSeries(Nframes_);
    for (UVmapType::iterator hb = UV_Map_.begin(); hb != UV_Map_.end(); ++hb)
      hb->second.FinishSeries(Nframes_);
  }
  // Should only be called once.
  seriesUpdated_ = true;
}

// Action_Hbond::MemoryUsage()
std::string Action_HydrogenBond::MemoryUsage(size_t nPairs, size_t nFrames) const {
  static const size_t HBmapTypeElt = 32 + sizeof(int) + 
                                     (2*sizeof(double) + sizeof(DataSet_integer*) + 4*sizeof(int));
  size_t memTotal = nPairs * HBmapTypeElt;
  // If calculating series every hbond will have time series.
  // NOTE: This does not include memory used by DataSet.
  if (series_ && nFrames > 0) {
    size_t seriesSet = (nFrames * sizeof(int)) + sizeof(std::vector<int>);
    memTotal += (seriesSet * nPairs);
  }
/*
  // Current memory used by bridging solvent
  static const size_t BridgeTypeElt = 32 + sizeof(std::set<int>) + sizeof(int);
  for (BridgeType::const_iterator it = BridgeMap_.begin(); it != BridgeMap_.end(); ++it)
    memTotal += (it->first.size() * sizeof(int));
  memTotal += (BridgeMap_.size() * BridgeTypeElt);
*/
  return ByteString( memTotal, BYTE_DECIMAL );
}

// Action_HydrogenBond::Print()
/** Print average occupancies over all frames for all detected Hbonds. */
void Action_HydrogenBond::Print() {
  typedef std::vector<Hbond> Harray;
  Harray HbondList; // For sorting
  std::string Aname, Hname, Dname;

  // Final memory usage
  mprintf("    HBOND: Actual memory usage is %s\n",
          MemoryUsage(UU_Map_.size()+UV_Map_.size(), Nframes_).c_str());
  mprintf("\t%zu solute-solute hydrogen bonds.\n", UU_Map_.size());
  if (calcSolvent_) {
   mprintf("\t%zu solute-solvent hydrogen bonds.\n", UV_Map_.size());
//   mprintf("\t%zu unique solute-solvent bridging interactions.\n", BridgeMap_.size());
  }

  t_uu_.WriteTiming(    2,  "Solute-Solute   :",t_action_.Total());
  if (calcSolvent_) {
    t_uv_.WriteTiming( 2,   "Solute-Solvent  :",t_uv_.Total());
    t_bridge_.WriteTiming(2,"Bridging waters :",t_action_.Total());
  }
  t_action_.WriteTiming(1,"Total:");

  // Ensure all series have been updated for all frames.
  UpdateSeries();

  if (CurrentParm_ == 0) return;
  // Calculate necessary column width for strings based on how many residues.
  // ResName+'_'+ResNum+'@'+AtomName | NUM = 4+1+R+1+4 = R+10
  int NUM = DigitWidth( CurrentParm_->Nres() ) + 10;
  // If useAtomNum_ +'_'+AtomNum += 1+A
  if (useAtomNum_) NUM += ( DigitWidth( CurrentParm_->Natom() ) + 1 );

  // Solute Hbonds 
  if (avgout_ != 0) { 
    // Place all detected Hbonds in a list and sort.
    for (UUmapType::const_iterator it = UU_Map_.begin(); it != UU_Map_.end(); ++it) {
      HbondList.push_back( it->second );
      // Calculate average distance and angle for this hbond.
      HbondList.back().CalcAvg();
    }
    UU_Map_.clear();
    // Sort and Print 
    sort( HbondList.begin(), HbondList.end() );
    avgout_->Printf("%-*s %*s %*s %8s %12s %12s %12s\n", NUM, "#Acceptor", 
                   NUM, "DonorH", NUM, "Donor", "Frames", "Frac", "AvgDist", "AvgAng");
    for (Harray::const_iterator hbond = HbondList.begin(); hbond != HbondList.end(); ++hbond ) 
    {
      double avg = ((double)hbond->Frames()) / ((double) Nframes_);
      Aname = CurrentParm_->TruncResAtomName(hbond->A());
      Hname = CurrentParm_->TruncResAtomName(hbond->H());
      Dname = CurrentParm_->TruncResAtomName(hbond->D());
      if (useAtomNum_) {
        Aname.append("_" + integerToString(hbond->A()+1));
        Hname.append("_" + integerToString(hbond->H()+1));
        Dname.append("_" + integerToString(hbond->D()+1));
      }
      avgout_->Printf("%-*s %*s %*s %8i %12.4f %12.4f %12.4f\n",
                     NUM, Aname.c_str(), NUM, Hname.c_str(), NUM, Dname.c_str(),
                     hbond->Frames(), avg, hbond->Dist(), hbond->Angle());
    }
  }

  // Solute-solvent Hbonds 
  if (solvout_ != 0 && calcSolvent_) {
    HbondList.clear();
    for (UVmapType::const_iterator it = UV_Map_.begin(); it != UV_Map_.end(); ++it) {
      HbondList.push_back( it->second );
      // Calculate average distance and angle for this hbond.
      HbondList.back().CalcAvg();
    }
    UV_Map_.clear();
    sort( HbondList.begin(), HbondList.end() );
    // Calc averages and print
    solvout_->Printf("#Solute-Solvent Hbonds:\n");
    solvout_->Printf("%-*s %*s %*s %8s %12s %12s %12s\n", NUM, "#Acceptor", 
                   NUM, "DonorH", NUM, "Donor", "Count", "Frac", "AvgDist", "AvgAng");
    for (Harray::const_iterator hbond = HbondList.begin(); hbond != HbondList.end(); ++hbond )
    {
      // Average has slightly diff meaning since for any given frame multiple
      // solvent can bond to the same solute.
      double avg = ((double)hbond->Frames()) / ((double) Nframes_);
      if (hbond->A()==-1) // Solvent acceptor
        Aname = "SolventAcc";
      else {
        Aname = CurrentParm_->TruncResAtomName(hbond->A());
        if (useAtomNum_) Aname.append("_" + integerToString(hbond->A()+1));
      }
      if (hbond->D()==-1) { // Solvent donor
        Dname = "SolventDnr";
        Hname = "SolventH";
      } else {
        Dname = CurrentParm_->TruncResAtomName(hbond->D());
        Hname = CurrentParm_->TruncResAtomName(hbond->H());
        if (useAtomNum_) {
          Dname.append("_" + integerToString(hbond->D()+1));
          Hname.append("_" + integerToString(hbond->H()+1));
        }
      }
      solvout_->Printf("%-*s %*s %*s %8i %12.4f %12.4f %12.4f\n",
                     NUM, Aname.c_str(), NUM, Hname.c_str(), NUM, Dname.c_str(),
                     hbond->Frames(), avg, hbond->Dist(), hbond->Angle());
    }
  }
/*
  // BRIDGING INFO
  if (bridgeout_ != 0 && calcSolvent_) {
    bridgeout_->Printf("#Bridging Solute Residues:\n");
    // Place bridging values in a vector for sorting
    std::vector<std::pair< std::set<int>, int> > bridgevector;
    for (BridgeType::const_iterator it = BridgeMap_.begin();
                                    it != BridgeMap_.end(); ++it)
      bridgevector.push_back( *it );
    std::sort( bridgevector.begin(), bridgevector.end(), bridge_cmp() );
    for (std::vector<std::pair< std::set<int>, int> >::const_iterator bv = bridgevector.begin();
                                                                      bv != bridgevector.end();
                                                                    ++bv)
    {
      bridgeout_->Printf("Bridge Res");
      for (std::set<int>::const_iterator res = bv->first.begin();
                                         res != bv->first.end(); ++res)
        bridgeout_->Printf(" %i:%s", *res+1, CurrentParm_->Res( *res ).c_str());
      bridgeout_->Printf(", %i frames.\n", bv->second);
    } 
  }
*/
}

// ===== Action_HydrogenBond::Hbond ============================================
/** Calculate average distance and angle for hbond. */
void Action_HydrogenBond::Hbond::CalcAvg() {
  double dFrames = (double)frames_;
  dist_ /= dFrames;
  angle_ /= dFrames;
  angle_ *= Constants::RADDEG;
}

/** For filling in series data. */
const int Action_HydrogenBond::Hbond::ZERO = 0;

/** Calculate average angle/distance. */
void Action_HydrogenBond::Hbond::FinishSeries(unsigned int N) {
  if (data_ != 0 && N > 0) {
    if ( data_->Size() < N ) data_->Add( N-1, &ZERO );
  }
}
