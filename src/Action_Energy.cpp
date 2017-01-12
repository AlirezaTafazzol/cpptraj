#include "Action_Energy.h"
#include "CpptrajStdio.h"
#include "Ewald.h" // DEBUG

// CONSTRUCTOR
Action_Energy::Action_Energy() : currentParm_(0) {}


void Action_Energy::Help() const {
  mprintf("\t[<name>] [<mask1>] [out <filename>]\n"
          "\t[bond] [angle] [dihedral] [nb14] {[nonbond] | [elec] [vdw]}\n"
          "\t[ etype {simple | directsum [npoints <N>]} ]\n"
          "  Calculate energy for atoms in mask.\n");
}

/// DataSet aspects
static const char* Estring[] = {"bond", "angle", "dih", "vdw14", "elec14", "vdw", "elec", "total"};

/// Calculation types
static const char* Cstring[] = {"Bond", "Angle", "Torsion", "1-4 Nonbond", "Nonbond",
                                "Electrostatics", "van der Waals", "Electrostatics (Direct Sum)" };

// Action_Energy::AddSet()
int Action_Energy::AddSet(Etype typeIn, DataSetList& DslIn, DataFile* outfile,
                          std::string const& setname)
{
  Energy_[typeIn] = DslIn.AddSet(DataSet::DOUBLE, MetaData(setname, Estring[typeIn]));
  if (Energy_[typeIn] == 0) return 1;
  if (outfile != 0) outfile->AddDataSet( Energy_[typeIn] );
  return 0;
}

// Action_Energy::Init()
Action::RetType Action_Energy::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  ENE_.SetDebug( debugIn );
  // Get keywords
  DataFile* outfile = init.DFL().AddDataFile( actionArgs.GetStringKey("out"), actionArgs );

  // Which terms will be calculated?
  bool calc_vdw  = actionArgs.hasKey("vdw" );
  bool calc_elec = actionArgs.hasKey("elec");
  bool calc_nb   = actionArgs.hasKey("nonbond");
  if (calc_vdw && calc_elec)
    calc_nb = true;
  if (calc_nb) {
    calc_vdw = false;
    calc_elec = false;
  }
  // Electrostatics type. If specified always split the Elec/VDW calc.
  enum ElecType { SIMPLE, DIRECTSUM };
  ElecType etype = SIMPLE;
  std::string etypearg = actionArgs.GetStringKey("etype");
  if (!etypearg.empty()) {
    if (calc_nb) {
      calc_nb = false;
      calc_vdw = true;
    }
    if (etypearg == "directsum") {
      etype = DIRECTSUM;
      calc_elec = true;
      npoints_ = actionArgs.getKeyInt("npoints", 0);
    } else if (etypearg == "simple") {
      etype = SIMPLE;
      if (!calc_nb && !calc_elec) calc_elec = true;
    } else {
      mprinterr("Error: Unrecognized option for 'etype': %s\n", etypearg.c_str());
      return Action::ERR;
    }
  }
  // Set up calculations
  Ecalcs_.clear();
  if (actionArgs.hasKey("bond"))     Ecalcs_.push_back(BND);
  if (actionArgs.hasKey("angle"))    Ecalcs_.push_back(ANG);
  if (actionArgs.hasKey("dihedral")) Ecalcs_.push_back(DIH);
  if (actionArgs.hasKey("nb14"))     Ecalcs_.push_back(N14);
  if (calc_nb)                       Ecalcs_.push_back(NBD);
  if (calc_vdw)                      Ecalcs_.push_back(LJ);
  if (calc_elec) {
    switch (etype) {
      case SIMPLE:    Ecalcs_.push_back(COULOMB); break;
      case DIRECTSUM: Ecalcs_.push_back(DIRECT); break;
    }
  }
  // If nothing is selected, select all.
  if (Ecalcs_.empty()) {
    for (int c = 0; c <= (int)NBD; c++)
      Ecalcs_.push_back( (CalcType)c );
  }

  // Get Masks
  Mask1_.SetMaskString( actionArgs.GetMaskNext() );

  // DataSet
  std::string setname = actionArgs.GetStringNext();
  if (setname.empty())
    setname = init.DSL().GenerateDefaultName("ENE");
  Energy_.clear();
  Energy_.resize( (int)TOTAL + 1, 0 );
  for (calc_it calc = Ecalcs_.begin(); calc != Ecalcs_.end(); ++calc)
  {
    switch (*calc) {
      case BND: if (AddSet(BOND, init.DSL(), outfile, setname)) return Action::ERR; break;
      case ANG: if (AddSet(ANGLE, init.DSL(), outfile, setname)) return Action::ERR; break;
      case DIH: if (AddSet(DIHEDRAL, init.DSL(), outfile, setname)) return Action::ERR; break;
      case N14:
        if (AddSet(V14, init.DSL(), outfile, setname)) return Action::ERR;
        if (AddSet(Q14, init.DSL(), outfile, setname)) return Action::ERR;
        break;
      case NBD:
        if (AddSet(VDW, init.DSL(), outfile, setname)) return Action::ERR;
        if (AddSet(ELEC, init.DSL(), outfile, setname)) return Action::ERR;
        break;
      case LJ:
        if (AddSet(VDW, init.DSL(), outfile, setname)) return Action::ERR; break;
      case COULOMB:
        if (AddSet(ELEC, init.DSL(), outfile, setname)) return Action::ERR; break;
      case DIRECT:
        if (AddSet(ELEC, init.DSL(), outfile, setname)) return Action::ERR; break;
    }
  }
//  if (Ecalcs_.size() > 1) {
    if (AddSet(TOTAL, init.DSL(), outfile, setname)) return Action::ERR;
//  }
      
  mprintf("    ENERGY: Calculating energy for atoms in mask '%s'\n", Mask1_.MaskString());
  mprintf("\tCalculating terms:");
  for (calc_it calc = Ecalcs_.begin(); calc != Ecalcs_.end(); ++calc) {
    if (calc != Ecalcs_.begin()) mprintf(",");
    mprintf(" %s", Cstring[*calc]);
  }
  mprintf("\n");
  if (etype == DIRECTSUM) {
    if (npoints_ < 0)
      mprintf("\tDirect sum energy for up to %i unit cells in each direction will be calculated.\n",
              -npoints_);
    else
      mprintf("\tDirect sum energy for %i unit cells in each direction will be calculated.\n",
              npoints_);
  }
  return Action::OK;
}

// Action_Energy::Setup()
/** Set angle up for this parmtop. Get masks etc.
  */
Action::RetType Action_Energy::Setup(ActionSetup& setup) {
  if (setup.Top().SetupCharMask(Mask1_)) return Action::ERR;
  if (Mask1_.None()) {
    mprintf("Warning: Mask '%s' selects no atoms.\n", Mask1_.MaskString());
    return Action::SKIP;
  }
  Mask1_.MaskInfo();
  Imask_ = AtomMask(Mask1_.ConvertToIntMask(), Mask1_.Natom());
  // Check for LJ terms
  for (calc_it calc = Ecalcs_.begin(); calc != Ecalcs_.end(); ++calc)
    if ((*calc == N14 || *calc == NBD) && !setup.Top().Nonbond().HasNonbond())
    {
      mprinterr("Error: Nonbonded energy calc requested but topology '%s'\n"
                "Error:   does not have non-bonded parameters.\n", setup.Top().c_str());
      return Action::ERR;
    }
  currentParm_ = setup.TopAddress();
  return Action::OK;
}

/// For debugging the direct sum convergence
double Action_Energy::Dbg_Direct(Frame const& frameIn, int maxpoints) {
  // DEBUG
  double lastEQ = 0.0;
  for (int npoints = 0; npoints < maxpoints; npoints++) {
    double EQ = ENE_.E_DirectSum(frameIn, *currentParm_, Imask_, npoints);
    mprintf("DEBUG: %i points DirectSum= %12.4f", npoints, EQ);
    if (npoints > 0) {
      mprintf(" delta= %g", EQ - lastEQ);
    }
    mprintf("\n");
    lastEQ = EQ;
  }
  return lastEQ;
}

// Action_Energy::DoAction()
Action::RetType Action_Energy::DoAction(int frameNum, ActionFrame& frm) {
  double Etot = 0.0, ene, ene2;
  for (calc_it calc = Ecalcs_.begin(); calc != Ecalcs_.end(); ++calc)
  {
    switch (*calc) {
      case BND:
        ene = ENE_.E_bond(frm.Frm(), *currentParm_, Mask1_);
        Energy_[BOND]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case ANG:
        ene = ENE_.E_angle(frm.Frm(), *currentParm_, Mask1_);
        Energy_[ANGLE]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case DIH:
        ene = ENE_.E_torsion(frm.Frm(), *currentParm_, Mask1_);
        Energy_[DIHEDRAL]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case N14:
        ene = ENE_.E_14_Nonbond(frm.Frm(), *currentParm_, Mask1_, ene2);
        Energy_[V14]->Add(frameNum, &ene);
        Energy_[Q14]->Add(frameNum, &ene2);
        Etot += (ene + ene2);
        break;
      case NBD:
        ene = ENE_.E_Nonbond(frm.Frm(), *currentParm_, Imask_, ene2);
        Energy_[VDW]->Add(frameNum, &ene);
        Energy_[ELEC]->Add(frameNum, &ene2);
        Etot += (ene + ene2);
        break;
      case LJ:
        ene = ENE_.E_VDW(frm.Frm(), *currentParm_, Imask_);
        Energy_[VDW]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case COULOMB:
        ene = ENE_.E_Elec(frm.Frm(), *currentParm_, Imask_);
        Energy_[ELEC]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case DIRECT:
        if (npoints_ < 0)
          ene = Dbg_Direct(frm.Frm(), (-npoints_)+1);
        else
          ene = ENE_.E_DirectSum(frm.Frm(), *currentParm_, Imask_, npoints_);
        Energy_[ELEC]->Add(frameNum, &ene);
        Etot += ene;
        break;
    }
  }

  Energy_[TOTAL]->Add(frameNum, &Etot);

  // DEBUG
  Ewald ew;
  ew.SetupParams(5.6, 0.0000001, 0.0);
  ew.CalcSumQ(*currentParm_, Imask_);
  double ew_ene = ew.CalcEnergy(frm.Frm(), *currentParm_, Imask_);
  mprintf("DEBUG: EW energy= %g\n", ew_ene);

  return Action::OK;
}

void Action_Energy::Print() {
  mprintf("Timing for energy: '%s' ('%s')\n", Energy_[TOTAL]->legend(),
           Mask1_.MaskString());
  ENE_.PrintTiming();
}
