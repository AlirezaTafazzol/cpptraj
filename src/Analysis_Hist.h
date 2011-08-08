#ifndef INC_ANALYSIS_HIST_H
#define INC_ANALYSIS_HIST_H
/// Class: Hist
/// Create an N-dimensional histogram from N input datasets
#include "Analysis.h"
#include "Histogram.h"
#include <vector>
class Hist : public Analysis {
    Histogram hist;
    std::vector<DataSet*> histdata;

    bool calcFreeE;
    double Temp;
    bool normalize;
    bool gnuplot;

    double min;
    double max;
    double step;
    int bins;

    int SetupDimension(char *,DataSetList *);
  public :
    Hist();
    ~Hist();

    int Setup(DataSetList*);
    //int Analyze();
    //void Print();
};
#endif
