#include <cmath>
#include "Action_GIST.h"
#include "CpptrajStdio.h"
#include "Constants.h"
#include "DataSet_GridFlt.h"

Action_GIST::Action_GIST() :
  gO_(0),
  gH_(0),
  Esw_(0),
  Eww_(0),
  dTStrans_(0),
  dTSorient_(0),
  dTSsix_(0),
  neighbor_norm_(0),
  dipole_(0),
  order_norm_(0),
  dipolex_(0),
  dipoley_(0),
  dipolez_(0),
  datafile_(0),
  BULK_DENS_(0.0),
  temperature_(0.0),
  NFRAME_(0),
  max_nwat_(0),
  doOrder_(false),
  doEij_(false),
  skipE_(false)
{}

void Action_GIST::Help() const {
  mprintf("\t[doorder] [doeij] [skipE] [refdens <rdval>] [Temp <tval>]\n"
          "\t[gridcntr <xval> <yval> <zval>]\n"
          "\t[griddim <xval> <yval> <zval>] [gridspacn <spaceval>]\n"
          "\t[out <filename>] [noimage]\n");
}

Action::RetType Action_GIST::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  image_.InitImaging( !(actionArgs.hasKey("noimage")) );
  std::string gistout = actionArgs.GetStringKey("out");
  if (gistout.empty()) gistout.assign("gist-output.dat");
  datafile_ = init.DFL().AddCpptrajFile( gistout, "GIST output" );
  if (datafile_ == 0) return Action::ERR;
  doOrder_ = actionArgs.hasKey("doorder");
  doEij_ = actionArgs.hasKey("doeij");
  skipE_ = actionArgs.hasKey("skipE");
  // Set Bulk Density 55.5M
  BULK_DENS_ = actionArgs.getKeyDouble("refdens", 0.0334);
  if ( BULK_DENS_ > (0.0334*1.2) )
    mprintf("Warning: water reference density is high, consider using 0.0334 for 1g/cc water density\n");
  else if ( BULK_DENS_ < (0.0334*0.8) )
    mprintf("Warning: water reference density is low, consider using 0.0334 for 1g/cc water density\n");
  temperature_ = actionArgs.getKeyDouble("temp", 300.0);
  if (temperature_ < 0.0) {
    mprinterr("Error: Negative temperature specified.\n");
    return Action::ERR;
  }
  // Grid spacing
  double gridspacing = actionArgs.getKeyDouble("gridspacn", 0.50);
  // Grid center
  Vec3 gridcntr(0.0);
  if ( actionArgs.hasKey("gridcntr") ) {
    gridcntr[0] = actionArgs.getNextDouble(-1);
    gridcntr[1] = actionArgs.getNextDouble(-1);
    gridcntr[2] = actionArgs.getNextDouble(-1);
  } else
    mprintf("Warning: No grid center values specified, using default (origin)\n");
  // Grid dimensions
  int nx = 40;
  int ny = 40;
  int nz = 40;
  if ( actionArgs.hasKey("griddim") ) {
    nx = actionArgs.getNextInteger(-1);
    ny = actionArgs.getNextInteger(-1);
    nz = actionArgs.getNextInteger(-1);
  } else
    mprintf("Warning: No grid dimension values specified, using default (40,40,40)\n");
  // Data set name
  std::string dsname = actionArgs.GetStringKey("name");
  if (dsname.empty())
    dsname = init.DSL().GenerateDefaultName("GIST");

  // Set up grid data sets
  gO_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "gO"));
  gH_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "gH"));
  Esw_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "Esw"));
  Eww_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "Eww"));
  dTStrans_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "dTStrans"));
  dTSorient_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "dTSorient"));
  dTSsix_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "dTSsix"));
  neighbor_norm_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "neighbor"));
  dipole_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_FLT, MetaData(dsname, "dipole"));

  order_norm_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_DBL, MetaData(dsname, "order"));
  dipolex_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_DBL, MetaData(dsname, "dipolex"));
  dipoley_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_DBL, MetaData(dsname, "dipoley"));
  dipolez_ = (DataSet_3D*)init.DSL().AddSet(DataSet::GRID_DBL, MetaData(dsname, "dipolez"));
 
  // Set up grids. TODO non-orthogonal as well
  Vec3 v_spacing( gridspacing );
  gO_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  gH_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  Esw_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  Eww_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  dTStrans_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  dTSorient_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  dTSsix_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  neighbor_norm_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  dipole_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);

  order_norm_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  dipolex_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  dipoley_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);
  dipolez_->Allocate_N_C_D(nx, ny, nz, gridcntr, v_spacing);

  // Set up grid params TODO non-orthogonal as well
  G_max_ = Vec3( (double)nx * gridspacing + 1.5,
                 (double)ny * gridspacing + 1.5,
                 (double)nz * gridspacing + 1.5 );
  N_waters_.assign( gO_->Size(), 0 );
  N_hydrogens_.assign( gO_->Size(), 0 );
  voxel_xyz_.resize( gO_->Size() ); // [] = X Y Z
  voxel_Q_.resize( gO_->Size() ); // [] = W4 X4 Y4 Z4
  //Box gbox;
  //gbox.SetBetaLengths( 90.0, (double)nx * gridspacing,
  //                           (double)ny * gridspacing,
  //                           (double)nz * gridspacing );
  //grid_.Setup_O_Box( nx, ny, nz, gO_->GridOrigin(), gbox );
  //grid_.Setup_O_D( nx, ny, nz, gO_->GridOrigin(), v_spacing );

  mprintf("    GIST:\n");
  if(doOrder_)
    mprintf("\tDo Order calculation\n");
  else
    mprintf("\tSkip Order calculation\n");
  if(doEij_)
    mprintf("\tCompute and print water-water Eij matrix\n");
  else
    mprintf("\tSkip water-water Eij matrix\n");
  mprintf("\tWater reference density: %6.4f\n", BULK_DENS_); // TODO units
  mprintf("\tSimulation temperature: %6.4f K\n", temperature_);
  if (image_.UseImage())
    mprintf("\tDistances will be imaged.\n");
  else
    mprintf("\tDistances will not be imaged.\n");
  gO_->GridInfo();
  mprintf("\tNumber of voxels: %zu, voxel volume: %f Ang^3\n",
          gO_->Size(), gO_->VoxelVolume());
  mprintf("\t#Please cite these papers if you use GIST results in a publication:\n"
          "\t#    Steven Ramsey, Crystal Nguyen, Romelia Salomon-Ferrer, Ross C. Walker, Michael K. Gilson, and Tom Kurtzman J. Comp. Chem. 37 (21) 2016\n"
          "\t#    Crystal Nguyen, Michael K. Gilson, and Tom Young, arXiv:1108.4876v1 (2011)\n"
          "\t#    Crystal N. Nguyen, Tom Kurtzman Young, and Michael K. Gilson,\n"
          "\t#      J. Chem. Phys. 137, 044101 (2012)\n"
          "\t#    Lazaridis, J. Phys. Chem. B 102, 3531–3541 (1998)\n");

  return Action::OK;
}

static inline bool NotEqual(double v1, double v2) {
  return ( fabs(v1 - v2) > Constants::SMALL );
}

// Action_GIST::Setup()
Action::RetType Action_GIST::Setup(ActionSetup& setup) {
  // We need box info
  if (setup.CoordInfo().TrajBox().Type() == Box::NOBOX) {
    mprinterr("Error: Must have explicit solvent with periodic boundaries!");
    return Action::ERR;
  }
  image_.SetupImaging( setup.CoordInfo().TrajBox().Type() );

  // Get molecule number for each solvent molecule
  mol_nums_.clear();
  unsigned int midx = 0;
  bool isFirstSolvent = true;
  for (Topology::mol_iterator mol = setup.Top().MolStart();
                              mol != setup.Top().MolEnd(); ++mol, ++midx)
  {
    if (mol->IsSolvent()) {
      int o_idx = mol->BeginAtom();
      // Check that molecule has 3 atoms
      if (mol->NumAtoms() != 3) {
        mprinterr("Error: Molecule '%s' has %i atoms, expected 3 for water.\n",
                  setup.Top().TruncResNameNum( setup.Top()[o_idx].ResNum() ).c_str(),
                  mol->NumAtoms());
        return Action::ERR;
      }
      mol_nums_.push_back( midx ); // TODO needed?
      // Check that first atom is actually Oxygen
      if (setup.Top()[o_idx].Element() != Atom::OXYGEN) {
        mprinterr("Error: Molecule '%s' is not water or does not have oxygen atom.\n",
                  setup.Top().TruncResNameNum( setup.Top()[o_idx].ResNum() ).c_str());
        return Action::ERR;
      }
      O_idxs_.push_back( o_idx );
      // Check that the next two atoms are Hydrogens
      if (setup.Top()[o_idx+1].Element() != Atom::HYDROGEN ||
          setup.Top()[o_idx+2].Element() != Atom::HYDROGEN)
      {
        mprinterr("Error: Molecule '%s' does not have hydrogen atoms.\n",
                  setup.Top().TruncResNameNum( setup.Top()[o_idx].ResNum() ).c_str());
        return Action::ERR;
      }
      // If first solvent molecule, save charges. If not, check that charges match.
      if (isFirstSolvent) {
        q_O_  = setup.Top()[o_idx  ].Charge();
        q_H1_ = setup.Top()[o_idx+1].Charge();
        q_H2_ = setup.Top()[o_idx+2].Charge();
        // Sanity checks
        if (NotEqual(q_H1_, q_H2_))
          mprintf("Warning: Charges on water hydrogens do not match (%g, %g).\n", q_H1_, q_H2_);
        if (fabs( q_O_ + q_H1_ + q_H2_ ) > 0.0)
          mprintf("Warning: Charges on water do not sum to 0 (%g)\n", q_O_ + q_H1_ + q_H2_);
        mprintf("DEBUG: Water charges: O=%g  H1=%g  H2=%g\n", q_O_, q_H1_, q_H2_);
      } else {
        if (NotEqual(q_O_, setup.Top()[o_idx  ].Charge()))
          mprintf("Warning: Charge on water '%s' oxygen %g does not match first water %g.\n",
                  setup.Top().TruncResNameNum( setup.Top()[o_idx].ResNum() ).c_str(),
                  setup.Top()[o_idx  ].Charge(), q_O_);
        if (NotEqual(q_H1_, setup.Top()[o_idx+1].Charge()))
          mprintf("Warning: Charge on water '%s' H1 %g does not match first water %g.\n",
                  setup.Top().TruncResNameNum( setup.Top()[o_idx].ResNum() ).c_str(),
                  setup.Top()[o_idx+1].Charge(), q_H1_);
        if (NotEqual(q_H2_, setup.Top()[o_idx+2].Charge()))
          mprintf("Warning: Charge on water '%s' H2 %g does not match first water %g.\n",
                  setup.Top().TruncResNameNum( setup.Top()[o_idx].ResNum() ).c_str(),
                  setup.Top()[o_idx+2].Charge(), q_H2_);
      }
      isFirstSolvent = false;
    }
  }

  water_voxel_.assign( mol_nums_.size(), -1 );

  return Action::OK;
}

const Vec3 Action_GIST::x_lab_ = Vec3(1.0, 0.0, 0.0);
const Vec3 Action_GIST::y_lab_ = Vec3(0.0, 1.0, 0.0);
const Vec3 Action_GIST::z_lab_ = Vec3(0.0, 0.0, 1.0);

// Action_GIST::DoAction()
Action::RetType Action_GIST::DoAction(int frameNum, ActionFrame& frm) {
  NFRAME_++;

  int bin_i, bin_j, bin_k;
  Vec3 const& Origin = gO_->GridOrigin();
  // Loop over each solvent molecule
  for (unsigned int sidx = 0; sidx < mol_nums_.size(); sidx++)
  {
    water_voxel_[sidx] = -1;
    const double* O_XYZ  = frm.Frm().XYZ( O_idxs_[sidx]     );
    // Get vector of water oxygen to grid origin.
    Vec3 W_G( O_XYZ[0] - Origin[0],
              O_XYZ[1] - Origin[1],
              O_XYZ[2] - Origin[2] );
    // Check if water oxygen is no more then 1.5 Ang from grid
    // NOTE: using <= to be consistent with original code
    if ( W_G[0] <= G_max_[0] && W_G[0] >= -1.5 &&
         W_G[1] <= G_max_[1] && W_G[1] >= -1.5 &&
         W_G[2] <= G_max_[2] && W_G[2] >= -1.5 )
    {
      const double* H1_XYZ = frm.Frm().XYZ( O_idxs_[sidx] + 1 );
      const double* H2_XYZ = frm.Frm().XYZ( O_idxs_[sidx] + 2 );
      // Try to bin the oxygen
      if ( gO_->CalcBins( O_XYZ[0], O_XYZ[1], O_XYZ[2], bin_i, bin_j, bin_k ) )
      {
        // Oxygen is inside the grid. Record the voxel.
        int voxel = (int)gO_->CalcIndex(bin_i, bin_j, bin_k);
        water_voxel_[sidx] = voxel;
        N_waters_[voxel]++;
        max_nwat_ = std::max( N_waters_[voxel], max_nwat_ );
        // ----- EULER ---------------------------
        // Record XYZ coords of water in voxel
        voxel_xyz_[voxel].push_back( O_XYZ[0] );
        voxel_xyz_[voxel].push_back( O_XYZ[1] );
        voxel_xyz_[voxel].push_back( O_XYZ[2] );
        // Get O-HX vectors
        Vec3 H1_wat( H1_XYZ[0]-O_XYZ[0], H1_XYZ[1]-O_XYZ[1], H1_XYZ[2]-O_XYZ[2] );
        Vec3 H2_wat( H2_XYZ[0]-O_XYZ[0], H2_XYZ[1]-O_XYZ[1], H2_XYZ[2]-O_XYZ[2] );
        H1_wat.Normalize();
        H2_wat.Normalize();

        Vec3 ar1 = H1_wat.Cross( x_lab_ );
        Vec3 sar = ar1;
        ar1.Normalize();
        double dp1 = x_lab_ * H1_wat;
        double theta = acos(dp1);
        double sign = sar * H1_wat;
        if (sign > 0)
          theta /= 2.0;
        else
          theta /= -2.0;
        double w1 = cos(theta);
        double sin_theta = sin(theta);
        double x1 = ar1[0] * sin_theta;
        double y1 = ar1[1] * sin_theta;
        double z1 = ar1[2] * sin_theta;
        double w2 = w1;
        double x2 = x1;
        double y2 = y1;
        double z2 = z1;

        Vec3 H_temp;
        H_temp[0] = ((w2*w2+x2*x2)-(y2*y2+z2*z2))*H1_wat[0];
        H_temp[0] = (2*(x2*y2 - w2*z2)*H1_wat[1]) + H_temp[0];
        H_temp[0] = (2*(x2*z2-w2*y2)*H1_wat[2]) + H_temp[0];

        H_temp[1] = 2*(x2*y2 - w2*z2)* H1_wat[0];
        H_temp[1] = ((w2*w2-x2*x2+y2*y2-z2*z2)*H1_wat[1]) + H_temp[1];
        H_temp[1] = (2*(y2*z2+w2*x2)*H1_wat[2]) +H_temp[1];

        H_temp[2] = 2*(x2*z2+w2*y2) *H1_wat[0];
        H_temp[2] = (2*(y2*z2-w2*x2)*H1_wat[1]) + H_temp[2];
        H_temp[2] = ((w2*w2-x2*x2-y2*y2+z2*z2)*H1_wat[2]) + H_temp[2];

        H1_wat = H_temp;

        Vec3 H_temp2;
        H_temp2[0] = ((w2*w2+x2*x2)-(y2*y2+z2*z2))*H2_wat[0];
        H_temp2[0] = (2*(x2*y2 + w2*z2)*H2_wat[1]) + H_temp2[0];
        H_temp2[0] = (2*(x2*z2-w2*y2)+H2_wat[2]) +H_temp2[0];

        H_temp2[1] = 2*(x2*y2 - w2*z2) *H2_wat[0];
        H_temp2[1] = ((w2*w2-x2*x2+y2*y2-z2*z2)*H2_wat[1]) +H_temp2[1];
        H_temp2[1] = (2*(y2*z2+w2*x2)*H2_wat[2]) +H_temp2[1];

        H_temp2[2] = 2*(x2*z2+w2*y2)*H2_wat[0];
        H_temp2[2] = (2*(y2*z2-w2*x2)*H2_wat[1]) +H_temp2[2];
        H_temp2[2] = ((w2*w2-x2*x2-y2*y2+z2*z2)*H2_wat[2]) + H_temp2[2];

        H2_wat = H_temp2;

        Vec3 ar2 = H_temp.Cross(H_temp2);
        ar2.Normalize();
        double dp2 = ar2 * z_lab_;
        theta = acos(dp2);

        sar = ar2.Cross( z_lab_ );
        sign = sar * H_temp;

        if (sign < 0)
          theta /= 2.0;
        else
          theta /= -2.0;

        double w3 = cos(theta);
        sin_theta = sin(theta);
        double x3 = x_lab_[0] * sin_theta;
        double y3 = x_lab_[1] * sin_theta;
        double z3 = x_lab_[2] * sin_theta;

        double w4 = w1*w3 - x1*x3 - y1*y3 - z1*z3;
        double x4 = w1*x3 + x1*w3 + y1*z3 - z1*y3;
        double y4 = w1*y3 - x1*z3 + y1*w3 + z1*x3;
        double z4 = w1*z3 + x1*y3 - y1*x3 + z1*w3;

        voxel_Q_[voxel].push_back( w4 );
        voxel_Q_[voxel].push_back( x4 );
        voxel_Q_[voxel].push_back( y4 );
        voxel_Q_[voxel].push_back( z4 );
        //mprintf("DEBUG1: wxyz4= %g %g %g %g\n", w4, x4, y4, z4);
        // NOTE: No need for nw_angle_ here, it is same as N_waters_
        // ----- DIPOLE --------------------------
        dipolex_->UpdateVoxel(voxel, O_XYZ[0]*q_O_ + H1_XYZ[0]*q_H1_ + H2_XYZ[0]*q_H2_);
        dipoley_->UpdateVoxel(voxel, O_XYZ[1]*q_O_ + H1_XYZ[1]*q_H1_ + H2_XYZ[1]*q_H2_);
        dipolez_->UpdateVoxel(voxel, O_XYZ[2]*q_O_ + H1_XYZ[2]*q_H1_ + H2_XYZ[2]*q_H2_);
        // ---------------------------------------
      }

      // Water is at most 1.5A away from grid, so we need to check for H
      // even if O is outside grid.
      if (gO_->CalcBins( H1_XYZ[0], H1_XYZ[1], H1_XYZ[2], bin_i, bin_j, bin_k ) )
        N_hydrogens_[ (int)gO_->CalcIndex(bin_i, bin_j, bin_k) ]++;
      if (gO_->CalcBins( H2_XYZ[0], H2_XYZ[1], H2_XYZ[2], bin_i, bin_j, bin_k ) )
        N_hydrogens_[ (int)gO_->CalcIndex(bin_i, bin_j, bin_k) ]++;
    } // END water is within 1.5 Ang of grid
  } // END loop over each solvent molecule

  return Action::OK;
}

void Action_GIST::Print() {
  unsigned int MAX_GRID_PT = gO_->Size();
  double Vvox = gO_->VoxelVolume();
/*  int nx = (int)gO_->NX();
  int ny = (int)gO_->NY();
  int nz = (int)gO_->NZ();
  int addx = nz * ny;
  int addy = nz;
  int addz = 1;
  int subx = -1 * nz * ny;
  int suby = -1 * nz;
  int subz = -1;
  // Implement NN to compute orientational entropy for each voxel
  //double NNr, rx, ry, rz, rR, dbl;
  double dTSs = 0.0;
  double dTSst = 0.0;
  float NNs = 10000;
  float ds = 0;
  float dx = 0, dy = 0, dz = 0, dd = 0, NNd = 10000;
  double dTSo = 0, dTSot = 0, dTSt = 0, dTStt = 0;
  int nwts = 0;*/

  Farray dTSorient_norm( MAX_GRID_PT, 0.0 );

  mprintf("    GIST OUTPUT:\n");
  // LOOP over all voxels
  DataSet_GridFlt& dTSorient_dens = static_cast<DataSet_GridFlt&>( *dTSorient_ );
  double dTSorienttot = 0;
  int nwtt = 0;
  double dTSo = 0;
  for (unsigned int gr_pt = 0; gr_pt < MAX_GRID_PT; gr_pt++) {
    dTSorient_dens[gr_pt] = 0;
    dTSorient_norm[gr_pt] = 0;
    int nw_total = N_waters_[gr_pt]; // Total number of waters that have been in this voxel.
    //mprintf("DEBUG1: %u nw_total %i\n", gr_pt, nw_total);
    if (nw_total > 1) {
      nwtt += nw_total;
      int bound = 0;
      for (int n0 = 0; n0 < nw_total; n0++)
      {
        double NNr = 10000;
        float NNs = 10000;
        //float ds = 0;
        //float NNd = 10000;
        //float dd = 0;
        int q0 = n0 * 4; // Index into voxel_Q_ for n0
        for (int n1 = 0; n1 < nw_total; n1++)
        {
          if (n0 != n1) {
            int q1 = n1 * 4; // Index into voxel_Q_ for n1
            double rR = 2.0 * acos(  voxel_Q_[gr_pt][q1  ] * voxel_Q_[gr_pt][q0  ]
                                   + voxel_Q_[gr_pt][q1+1] * voxel_Q_[gr_pt][q0+1]
                                   + voxel_Q_[gr_pt][q1+2] * voxel_Q_[gr_pt][q0+2]
                                   + voxel_Q_[gr_pt][q1+3] * voxel_Q_[gr_pt][q0+3] );
            //mprintf("DEBUG1: %g\n", rR);
            if (rR > 0 && rR < NNr) NNr = rR;
          }
        } // END inner loop over all waters for this voxel

        //if (bound == 1) { // FIXME this appears never to be triggered.
        //  double dbl = 0;
        //  //dTSorient_norm[gr_pt] += dbl; // Why was this even here?
        //} else
        if (bound != 1 && NNr < 9999 && NNr > 0 && NNs > 0) {
          double dbl = log(NNr*NNr*NNr*nw_total / (3.0*Constants::TWOPI));
          //mprintf("DEBUG1: dbl %f\n", dbl);
          dTSorient_norm[gr_pt] += dbl;
          dTSo += dbl;
        }
      } // END outer loop over all waters for this voxel
      //mprintf("DEBUG1: dTSorient_norm %f\n", dTSorient_norm[gr_pt]);
      dTSorient_norm[gr_pt] = Constants::GASK_KCAL * temperature_ * 
                               ((dTSorient_norm[gr_pt]/nw_total) + Constants::EULER_MASC);
      dTSorient_dens[gr_pt] = dTSorient_norm[gr_pt] * nw_total / (NFRAME_ * Vvox);
      dTSorienttot += dTSorient_dens[gr_pt];
      //mprintf("DEBUG1: %f\n", dTSorienttot);
    }
  } // END loop over all grid points (voxels)
  dTSorienttot *= Vvox;
  mprintf("Maximum number of waters found in one voxel for %d frames = %d\n", NFRAME_, max_nwat_);
  mprintf("Total referenced orientational entropy of the grid: dTSorient = %9.5f kcal/mol, Nf=%d\n",
          dTSorienttot, NFRAME_);
  mprintf("DEBUG: x_vox_ size is %zu\n", voxel_xyz_.size());
}
