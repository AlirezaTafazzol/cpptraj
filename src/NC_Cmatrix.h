#ifndef INC_NC_CMATRIX_H
#define INC_NC_CMATRIX_H
#include "FileName.h"
/// NetCDF cluster matrix file.
class NC_Cmatrix {
  public:
    NC_Cmatrix();
    ~NC_Cmatrix();
    /// \return true if file is NetCDF cluster matrix file.
    static bool ID_Cmatrix(FileName const&);
    /// Open cluster matrix file for reading. Set sieve ID.
    int OpenCmatrixRead(FileName const&, int&);
    /// Get cluster matrix element (col, row)
    double GetCmatrixElement(unsigned int, unsigned int) const;
    /// Get cluster matrix element (raw index)
    double GetCmatrixElement(unsigned int) const;
    /// Create cluster matrix file; # frames, # rows, sieve, cache status
    int OpenCmatrixWrite(FileName const&, unsigned int, unsigned int, int, bool);
    /// Write non-sieved frames array.
    int WriteFramesArray(std::vector<int> const&);
    /// Write cluster matrix element (col, row)
    int WriteCmatrixElement(unsigned int, unsigned int, double);
    /// Close cluster matrix file.
    void CloseCmatrix();
    /// \return Matrix size
    unsigned int MatrixSize() const { return mSize_; }
    /// \return Matrix rows.
    unsigned int MatrixRows() const { return nRows_; }
  private:
#   ifdef BINTRAJ
    static inline bool IsCpptrajCmatrix(int);
    long int CalcIndex(unsigned int, unsigned int) const;

    int ncid_;                  ///< NetCDF file ID.
    int n_original_frames_DID_; ///< Number of original frames dimension.
    int n_rows_DID_;            ///< Number of rows (actual frames, N) in matrix dimension.
    int msize_DID_;             ///< Actual matrix size ( (N*(N-1))/2 ).
    int cmatrix_VID_;           ///< Cluster matrix variable ID ( matrix size ).
    int actualFrames_VID_;      ///< Non-sieved frames array ( N ).
    unsigned int nRows_;        ///< Number of rows (actual frames, N) in matrix dimension.
    unsigned int mSize_;        ///< Actual matrix size.
#   endif
};
#endif
