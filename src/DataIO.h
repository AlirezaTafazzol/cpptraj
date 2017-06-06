#ifndef INC_DATAIO_H
#define INC_DATAIO_H
#include "ArgList.h"
#include "DataSetList.h"
#include "CpptrajFile.h"
#include "BaseIOtype.h"
/// Base class that all DataIO objects inherit from.
class DataIO : public BaseIOtype {
  public:
    DataIO() :
      debug_(0), x_format_set_(false), valid1d_(false), valid2d_(false), valid3d_(false) {}
    DataIO(bool v1, bool v2, bool v3) :
      debug_(0), x_format_set_(false), valid1d_(v1), valid2d_(v2), valid3d_(v3) {}
    virtual ~DataIO() {}
    // ----- Inherited Functions -----------------
    virtual int processReadArgs(ArgList&) = 0;
    virtual int ReadData(FileName const&, DataSetList&, std::string const&) = 0;
    virtual int processWriteArgs(ArgList&) = 0;
    virtual int WriteData(FileName const&, DataSetList const&) = 0;
    virtual bool ID_DataFormat(CpptrajFile&) = 0; // TODO: -> BaseIOtype?
    /// \return True if this DataIO valid for given DataSet
    bool CheckValidFor(DataSet const&) const;
    void SetDebug(int d) { debug_ = d; }
    /// Set x column format, width, and precision
    void SetXcolFmt(TextFormat::FmtType t, int w, int p) {
      xcol_fmt_ = t; xcol_width_ = w; xcol_prec_ = p; x_format_set_ = true;
    }
  protected:
    /// Indicate this DataIO is valid for given DataSet type
    void SetValid(DataSet::DataType t) { valid_.push_back( t ); }
    /// Check that all sets in given list have given dimension.
    static int CheckAllDims(DataSetList const&, unsigned int);
    /// Check that X dim for all sets in given list match; all must be 1D.
    static int CheckXDimension(DataSetList const&);
    /// \return max size of DataSets in given list.
    static size_t DetermineMax(DataSetList const&);
    /// Convert flattened matrix array to matrix in DataSetList.
    static DataSet* DetermineMatrixType(std::vector<double> const&, int, int,
                                        DataSetList&, std::string const&);
    TextFormat::FmtType XcolFmt() const { return xcol_fmt_; }
    int XcolWidth()               const { return xcol_width_; }
    int XcolPrec()                const { return xcol_prec_;  }
    bool XcolFmtSet()             const { return x_format_set_; }
    int debug_;
  private:
    std::vector<DataSet::DataType> valid_; ///< Data sets for which DataIO is valid writer.
    TextFormat::FmtType xcol_fmt_; ///< X column format type
    int xcol_width_;               ///< X column width
    int xcol_prec_;                ///< X column precision
    bool x_format_set_;            ///< True if X column format has been explicitly set.
    bool valid1d_; ///< Valid for all 1D data sets. //TODO Remove
    bool valid2d_; ///< Valid for all 2D data sets.
    bool valid3d_; ///< Valid for all 3D data sets.
};
#endif
