#ifndef INC_TOPINFO_H
#include "CpptrajFile.h"
#include "Topology.h"
/// Class for printing formatted topology info to a file.
class TopInfo {
  public:
    TopInfo () : outfile_(0), parm_(0), toStdout_(false) {}
    ~TopInfo();
    TopInfo(Topology*);
    int SetupTopInfo(CpptrajFile*, Topology*);
    int SetupTopInfo(Topology* p) { return SetupTopInfo(0, p); }
    int PrintAtomInfo(std::string const&) const;
    int PrintBondInfo(std::string const&, std::string const&) const;
  private:
    void PrintBonds(BondArray const&, BondParmArray const&,
                    CharMask const&, CharMask const&, int&) const;

    CpptrajFile* outfile_;
    Topology* parm_;
    bool toStdout_;
};
#endif
