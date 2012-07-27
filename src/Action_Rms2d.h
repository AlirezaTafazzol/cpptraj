#ifndef INC_ACTION_RMS2D_H
#define INC_ACTION_RMS2D_H
#include "Action.h"
#include "TrajectoryFile.h"
#include "CoordList.h"
#include "TriangleMatrix.h"
// Class: Action_Rms2d
/// Action to calculate the RMSD between two sets of frames.
/** Perform RMS calculation between each input frame and each other input 
  * frame, or each frame read in from a separate reference traj and each 
  * input frame. 
  * The actual calcuation is performed in the print function.
  */
class Action_Rms2d: public Action {
  public:
    Action_Rms2d();
    ~Action_Rms2d();

    void print();
  private:
    CoordList ReferenceCoords_; ///< Hold coords from input frames.
    bool nofit_;                ///< Do not perform rms fitting
    AtomMask RefMask_;          ///< Reference atom mask
    AtomMask FrameMask_;        ///< Target atom mask
    char* rmsdFile_;            ///< Output filename
    TrajectoryFile* RefTraj_;   ///< Reference trajectory, each frame used in turn
    Topology* RefParm_;         ///< Reference trajectory Parm
    char* corrfilename_;        ///< Auto-correlation output filename
    double* mass_ptr_;          ///< If useMass, hold mass info for parm.
    bool mass_setup_;           ///< Used to check if mass already set up.

    int AutoCorrelate(TriangleMatrix&);
    DataSet* CalcRmsToTraj();
    DataSet* Calc2drms();

    int init();
    int setup();
    int action();
};
#endif
