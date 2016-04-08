#include <algorithm> // sort
#include "Exec_CompareTop.h"
#include "CpptrajStdio.h"

void Exec_CompareTop::Help() const {
  mprintf("\t{%s} {%s} [out <file>] [atype] [lj] [bnd] [ang] [dih]\n",
          DataSetList::TopArgs, DataSetList::TopArgs);
}

// -----------------------------------------------------------------------------
/// Function for printing NameType
static void PrintAtype(CpptrajFile& output, Topology const& parm, NameType const& name, char dir)
{
  output.Printf("%c %s\n", dir, *name);
}

/// \return An array of unique atom types
static std::vector<NameType> AtypeArray( Topology const& parm ) {
  std::set<NameType> atypes;
  for (Topology::atom_iterator atom = parm.begin(); atom != parm.end(); ++atom)
    atypes.insert( atom->Type() );
  std::vector<NameType> out;
  for (std::set<NameType>::const_iterator it = atypes.begin(); it != atypes.end(); ++it)
    out.push_back( *it );
  return out;
}

// -----------------------------------------------------------------------------
/// Hold dihedral atoms and parameters
class DihT {
  public:
    DihT() {}
    DihT(DihedralType const& T, Topology const& topIn) :
      atoms_(T), parms_(topIn.DihedralParm()[T.Idx()])
    {
      rnums_[0] = topIn[T.A1()].ResNum();
      rnums_[1] = topIn[T.A2()].ResNum();
      rnums_[2] = topIn[T.A3()].ResNum();
      rnums_[3] = topIn[T.A4()].ResNum();
      anames_[0] = topIn[T.A1()].Name();
      anames_[1] = topIn[T.A2()].Name();
      anames_[2] = topIn[T.A3()].Name();
      anames_[3] = topIn[T.A4()].Name();
    }
    bool operator<(DihT const& rhs) const {
      // First sort on residue number
      for (int i = 0; i != 4; i++)
        if (rnums_[i] < rhs.rnums_[i])
          return true;
        else if (rnums_[i] > rhs.rnums_[i])
          return false;
      // All residue numbers are equal. Sort by atom names.
      for (int i = 0; i != 4; i++)
        if (anames_[i] < rhs.anames_[i])
          return true;
        else if (anames_[i] > rhs.anames_[i])
          return false;
      // All res numbers and atom names are equal. Sort by parameters.
      if (parms_ < rhs.parms_)
        return true;
      else
        return false;
    }
/*
    bool operator<(DihT const& rhs) const {
      if (atoms_ < rhs.atoms_)
        return true;
      else if (parms_ < rhs.parms_)
        return true;
      else
        return false;
    }
*/
    DihedralType atoms_;
    DihedralParmType parms_;
    int rnums_[4];
    NameType anames_[4];
};

/// Array of dihedral atoms and parameters
typedef std::vector<DihT> DihArrayT;

/// \return Sorted array of dihedral atoms and parameters from given Topology
static DihArrayT DihArray( Topology const& topIn ) {
  DihArrayT array;

  for (DihedralArray::const_iterator it = topIn.Dihedrals().begin();
                                     it != topIn.Dihedrals().end(); ++it)
    array.push_back( DihT( *it, topIn ) );
  for (DihedralArray::const_iterator it = topIn.DihedralsH().begin();
                                     it != topIn.DihedralsH().end(); ++it)
    array.push_back( DihT( *it, topIn ) );

  //std::sort( array.begin(), array.end() );
  return array;
}

/// Function for printing DihT
static void PrintDihT(CpptrajFile& output, Topology const& parm,
                      DihT const& DIH, char dir)
{
  DihedralType const& first = DIH.atoms_;
  output.Printf("%c %s - %s - %s - %s {%s-%s-%s-%s}", dir,
                parm.AtomMaskName(first.A1()).c_str(),
                parm.AtomMaskName(first.A2()).c_str(),
                parm.AtomMaskName(first.A3()).c_str(),
                parm.AtomMaskName(first.A4()).c_str(),
                *(parm[first.A1()].Type()),
                *(parm[first.A2()].Type()),
                *(parm[first.A3()].Type()),
                *(parm[first.A4()].Type()));
  DihedralParmType const& DP = DIH.parms_; 
    output.Printf(" Pk=%g Pn=%g Phase=%g SCEE=%g SCNB=%g",
                  DP.Pk(), DP.Pn(), DP.Phase(), DP.SCEE(), DP.SCNB());
  output.Printf("\n");
}

/// Function for printing DihedralType
static void PrintDihedralType(CpptrajFile& output, Topology const& parm,
                      DihedralType const& first, char dir)
{
  output.Printf("%c %s - %s - %s - %s {%s-%s-%s-%s}", dir,
                parm.AtomMaskName(first.A1()).c_str(),
                parm.AtomMaskName(first.A2()).c_str(),
                parm.AtomMaskName(first.A3()).c_str(),
                parm.AtomMaskName(first.A4()).c_str(),
                *(parm[first.A1()].Type()),
                *(parm[first.A2()].Type()),
                *(parm[first.A3()].Type()),
                *(parm[first.A4()].Type()));
  if (first.Idx() != -1) {
    DihedralParmType const& DP = parm.DihedralParm()[first.Idx()];
    output.Printf(" Pk=%g Pn=%g Phase=%g SCEE=%g SCNB=%g",
                  DP.Pk(), DP.Pn(), DP.Phase(), DP.SCEE(), DP.SCNB());
  }
  output.Printf("\n");
}

/// Function for printing DihedralParmType
static void PrintDihP(CpptrajFile& output, Topology const& parm,
                      DihedralParmType const& first, char dir)
{
  output.Printf("%c Pk= %g  Pn= %g  Phase= %g  SCEE= %g  SCNB= %g\n", dir,
                first.Pk(), first.Pn(), first.Phase(), first.SCEE(), first.SCNB());
}

// -----------------------------------------------------------------------------
/// Function for printing AngleType
static void PrintAngT(CpptrajFile& output, Topology const& parm,
                      AngleType const& first, char dir)
{
  output.Printf("%c %s - %s - %s {%s-%s-%s}", dir,
                parm.AtomMaskName(first.A1()).c_str(),
                parm.AtomMaskName(first.A2()).c_str(),
                parm.AtomMaskName(first.A3()).c_str(),
                *(parm[first.A1()].Type()),
                *(parm[first.A2()].Type()),
                *(parm[first.A3()].Type()));
  if (first.Idx() != -1) {
    AngleParmType const& AP = parm.AngleParm()[first.Idx()];
    output.Printf(" Tk=%g Teq=%g", AP.Tk(), AP.Teq());
  }
  output.Printf("\n");
}

/// Function for printing AngleParmType
static void PrintAngP(CpptrajFile& output, Topology const& parm,
                      AngleParmType const& first, char dir)
{
  output.Printf("%c Tk= %g  Teq= %g\n", dir, first.Tk(), first.Teq());
}

// -----------------------------------------------------------------------------
/// Function for printing BondType
static void PrintBndT(CpptrajFile& output, Topology const& parm,
                      BondType const& first, char dir)
{
  output.Printf("%c %s - %s {%s-%s}", dir,
                parm.AtomMaskName(first.A1()).c_str(),
                parm.AtomMaskName(first.A2()).c_str(),
                *(parm[first.A1()].Type()),
                *(parm[first.A2()].Type()));
  if (first.Idx() != -1) {
    BondParmType const& BP = parm.BondParm()[first.Idx()];
    output.Printf(" Rk=%g Req=%g", BP.Rk(), BP.Req());
  }
  output.Printf("\n");
}

/// Function for printing BondParmType
static void PrintBndP(CpptrajFile& output, Topology const& parm,
                      BondParmType const& first, char dir)
{
  output.Printf("%c Rk= %g  Req= %g\n", dir, first.Rk(), first.Req());
}

// -----------------------------------------------------------------------------
/// Class template for comparing two arrays of a given parameter type
template <class T> class Diff {
  public:
    /// Parameter type array
    typedef std::vector< T > ArrayType;
    /// Function pointer for printing parameter type
    typedef void (*PrintFxnType)(CpptrajFile&, Topology const&, T const&, char);
    Diff() {}
    /** Compare two parameter arrays and report differences.
      * \param a1_in First parameter array ('<').
      * \param a2_in Second parameter array ('>').
      */
    void Compare(const ArrayType& a1_in, const ArrayType& a2_in, PrintFxnType fxnIn,
                 CpptrajFile& output, Topology const& parm1, Topology const& parm2)
    {
      ArrayType a1 = a1_in;
      ArrayType a2 = a2_in;
      std::sort( a1.begin(), a1.end() );
      CpptrajFile out1;
      out1.OpenWrite("out1");
      out1.Printf("'%s': %zu parameters.\n", parm1.c_str(), a1.size());
      for (typename ArrayType::const_iterator it = a1.begin(); it != a1.end(); ++it)
        fxnIn(out1, parm1, *it, ' ');
      out1.CloseFile();

      std::sort( a2.begin(), a2.end() );
      CpptrajFile out2;
      out2.OpenWrite("out2");
      out2.Printf("'%s': %zu parameters.\n", parm2.c_str(), a2.size());
      for (typename ArrayType::const_iterator it = a2.begin(); it != a2.end(); ++it)
        fxnIn(out2, parm2, *it, ' ');
      out2.CloseFile();

      typename ArrayType::const_iterator first1 = a1.begin();
      typename ArrayType::const_iterator first2 = a2.begin();
      while (first1 != a1.end() && first2 != a2.end()) {
        if (*first1 < *first2) {
          fxnIn(output, parm1, *first1, '<');
          ++first1;
        } else if (*first2 < *first1) {
          fxnIn(output, parm2, *first2, '>');
          ++first2;
        } else { ++first1; ++first2; }
      }
      while (first1 != a1.end())
        fxnIn(output, parm1, *(first1++), '<');
      while (first2 != a2.end())
        fxnIn(output, parm2, *(first2++), '>');
    }
};

// -----------------------------------------------------------------------------
/// Hold LJ params for an atom
class LJatom {
  public:
    LJatom() : rmin_(0.0), eps_(0.0) {}
    LJatom(NameType const& n, double r, double e) : name_(n), rmin_(r), eps_(e) {}
    NameType name_;
    double rmin_;
    double eps_;
    bool operator<(const LJatom& rhs) const {
      if (name_ == rhs.name_) {
        if (rmin_ == rhs.rmin_) {
          return (eps_ < rhs.eps_);
        } else return (rmin_ < rhs.rmin_);
      } else return (name_ < rhs.name_);
    }
};

/// Array of LJatom
typedef std::vector<LJatom> LJarrayType;

/// \return Array containing LJ params for each atom.
static LJarrayType LJarray( Topology const& parm ) {
  std::set<LJatom> temp;
  for (int idx = 0; idx != parm.Natom(); idx++) {
    NonbondType const& NB = parm.GetLJparam(idx, idx);
    double eps;
    if (NB.A() > 0.0)
      eps = (NB.B() * NB.B()) / (4.0 * NB.A());
    else
      eps = 0.0;
    temp.insert( LJatom( parm[idx].Type(), parm.GetVDWradius(idx), eps ) );
  }
  LJarrayType out;
  for (std::set<LJatom>::const_iterator it = temp.begin(); it != temp.end(); ++it)
    out.push_back( *it );
  return out;
}

/// Function for printing LJatom
static void PrintLJatom(CpptrajFile& output, Topology const& parm,
                        LJatom const& first, char dir)
{
  output.Printf("%c %s Rmin= %g  Eps= %g\n", dir, *(first.name_), first.rmin_, first.eps_);
}

// -----------------------------------------------------------------------------
/// Compare two topologies, find differences
Exec::RetType Exec_CompareTop::Execute(CpptrajState& State, ArgList& argIn)
{
  mprintf("Warning: THIS COMMAND IS NOT FULLY IMPLEMENTED.\n");
  Topology* parm1 = State.DSL().GetTopology( argIn );
  Topology* parm2 = State.DSL().GetTopology( argIn );
  if (parm1 == 0 || parm2 == 0) {
    mprinterr("Error: Specify two topologies.\n");
    return CpptrajState::ERR;
  }
  Topology const& p1 = static_cast<Topology const&>( *parm1 );
  Topology const& p2 = static_cast<Topology const&>( *parm2 );
  CpptrajFile output;
  output.OpenWrite( argIn.GetStringKey("out") );
  mprintf("\tOutput to '%s'\n", output.Filename().full());
  output.Printf("#< %s\n#> %s\n", p1.c_str(), p2.c_str());
  bool cmp_atype = argIn.hasKey("atype");
  bool cmp_lj = argIn.hasKey("lj");
  bool cmp_bnd = argIn.hasKey("bnd");
  bool cmp_ang = argIn.hasKey("ang");
  bool cmp_dih = argIn.hasKey("dih");
  if (!cmp_atype && !cmp_lj && !cmp_bnd && !cmp_ang && !cmp_dih) {
    cmp_atype = cmp_lj = cmp_bnd = cmp_ang = cmp_dih = true;
  }
/*  if (cmp_atype) {
    // Atom Types
    output.Printf("# Atom types\n");
    Diff<NameType> diff_atype;
    diff_atype.Compare( AtypeArray(p1), AtypeArray(p2), PrintAtype, output, p1, p2 );
  }
  if (cmp_lj) {
    // LJ params
    output.Printf("# LJ params\n");
    Diff<LJatom> diff_lj;
    diff_lj.Compare( LJarray(p1), LJarray(p2), PrintLJatom, output, p1, p2 );
  }
  if (cmp_bnd) {
    // Bonds
    output.Printf("# Bonds\n");
    Diff<BondType> diff_bnd;
    diff_bnd.Compare( p1.Bonds(), p2.Bonds(), PrintBndT, output, p1, p2 );
    diff_bnd.Compare( p1.BondsH(), p2.BondsH(), PrintBndT, output, p1, p2 );
    // Bond parameters
    output.Printf("# Bond Parameters\n");
    Diff<BondParmType> diff_bndP;
    diff_bndP.Compare( p1.BondParm(), p2.BondParm(), PrintBndP, output, p1, p2 );
  }
  if (cmp_ang) {
    // Angles
    output.Printf("# Angles\n");
    Diff<AngleType> diff_ang;
    diff_ang.Compare( p1.Angles(), p2.Angles(), PrintAngT, output, p1, p2 );
    diff_ang.Compare( p1.AnglesH(), p2.AnglesH(), PrintAngT, output, p1, p2 );
    // Angle parameters
    output.Printf("# Angle Parameters\n");
    Diff<AngleParmType> diff_angP;
    diff_angP.Compare( p1.AngleParm(), p2.AngleParm(), PrintAngP, output, p1, p2 );
  }*/
  if (cmp_dih) {
    // Dihedrals
    output.Printf("# Dihedrals\n");
    //Diff<DihedralType> diff_dih;
    //diff_dih.Compare( p1.Dihedrals(), p2.Dihedrals(), PrintDihedralType, output, p1, p2 );
    //diff_dih.Compare( p1.DihedralsH(), p2.DihedralsH(), PrintDihedralType, output, p1, p2 );
    Diff<DihT> diff_dih;
    diff_dih.Compare( DihArray(p1), DihArray(p2), PrintDihT, output, p1, p2 );
    // Dihedral parameters
    //output.Printf("# Dihedral Parameters\n");
    //Diff<DihedralParmType> diff_dihP;
    //diff_dihP.Compare( p1.DihedralParm(), p2.DihedralParm(), PrintDihP, output, p1, p2 );
  }
  output.CloseFile();
  return CpptrajState::OK;
}
