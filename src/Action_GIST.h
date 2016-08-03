#ifndef INC_ACTION_GIST2_H
#define INC_ACTION_GIST2_H
// TODO change protect eventually
#include "Action.h"
#include "ImagedAction.h"
#include "DataSet_3D.h"
/// Class for applying Grid Inhomogenous Solvation Theory
class Action_GIST : public Action {
  public:
    Action_GIST();
    DispatchObject* Alloc() const { return (DispatchObject*)new Action_GIST(); }
    void Help() const;
  private:
    Action::RetType Init(ArgList&, ActionInit&, int);
    Action::RetType Setup(ActionSetup&);
    Action::RetType DoAction(int, ActionFrame&) { return Action::ERR; }
    void Print() {}

    ImagedAction image_; ///< Imaging routines.
    // GIST float grid datasets
    DataSet_3D* gO_;        ///< Solvent oxygen density
    DataSet_3D* gH_;        ///< Solvent hydrogen density
    DataSet_3D* Esw_;       ///< Solute-water energy
    DataSet_3D* Eww_;       ///< Water-water energy
    DataSet_3D* dTStrans_;  ///< Solvent translation entropy
    DataSet_3D* dTSorient_; ///< Solvent orentational entropy
    DataSet_3D* dTSsix_;
    DataSet_3D* neighbor_norm_;
    DataSet_3D* dipole_; // pol
    // GIST double grid datasets
    DataSet_3D* order_norm_; // qtet
    DataSet_3D* dipolex_;
    DataSet_3D* dipoley_;
    DataSet_3D* dipolez_;

    typedef std::vector<int> Iarray;
    Iarray mol_nums_;    ///< Absolute molecule number of each solvent molecule.
    Iarray water_voxel_; ///< Absolute grid voxel for each solvent molecule.

    GridBin_Nonortho grid_; ///< Hold common grid parameters
    CpptrajFile* datafile_; ///< GIST output
    double BULK_DENS_; ///< Bulk water density
    double temperature_; ///< Temperature
    int NFRAME_; ///< Total # frames analyzed
    bool doOrder_; ///< If true do the order calc
    bool doEij_; ///< If true do the i-j energy calc
    bool skipE_; ///< If true skip the nonbond energy calc
    
};
#endif
