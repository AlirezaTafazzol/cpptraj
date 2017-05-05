#ifndef INC_ANALYSIS_TI_H
#define INC_ANALYSIS_TI_H
#include "Analysis.h"
#include "Array1D.h"
class Analysis_TI : public Analysis {
  public:
    Analysis_TI();
    DispatchObject* Alloc() const { return (DispatchObject*)new Analysis_TI(); }
    void Help() const;

    Analysis::RetType Setup(ArgList&, AnalysisSetup&, int);
    Analysis::RetType Analyze();
  private:
    int SetQuadAndWeights(int);
    /// Averaging type
    enum AvgType { AVG = 0, SKIP, INCREMENT };
    /// Integration type
    enum ModeType { GAUSSIAN_QUAD = 0, TRAPEZOID };
    typedef std::vector<int> Iarray;
    typedef std::vector<double> Darray;
    typedef std::vector<DataSet*> DSarray;

    Array1D input_dsets_; ///< Input DV/DL data sets
    Iarray nskip_;        ///< Numbers of data points to skip in calculating <DV/DL>
    DataSet* dAout_;      ///< Free energy data set
    DataSet* orig_avg_;   ///< Average DV/DL
    DataSet* bs_avg_;     ///< Bootstrap average DV/DL
    DataSet* bs_sd_;      ///< Bootstrap DV/DL standard deviation
    DSarray curve_;       ///< TI curve data set for each skip value
    Darray xval_;         ///< Hold abscissas corresponding to data sets.
    Darray wgt_;          ///< Hold Gaussian quadrature weights
    ModeType mode_;       ///< Integration mode
    AvgType avgType_;     ///< Type of averaging to be performed.
    int debug_;
    int n_bootstrap_pts_; ///< # points for bootstrap error analysis
    int n_bootstrap_samples_; ///< # of times to resample for bootstrap analysis
};
#endif
