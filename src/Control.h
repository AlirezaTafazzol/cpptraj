#ifndef INC_CONTROL_H
#define INC_CONTROL_H
#include "ArgList.h"
#include "AtomMask.h"
#include "DispatchObject.h"
/// Control structures.
class Control : public DispatchObject {
  public:
    typedef std::vector<ArgList> ArgArray;
    typedef ArgArray::const_iterator const_iterator;
    Control() : DispatchObject(CONTROL) {}
    /// Set up control structure.
    virtual int SetupControl(ArgList&) = 0;
    /// Check for control structure end command.
    virtual bool EndControl(ArgList const&) const = 0;
    /// Add command to control structure.
    virtual void AddCommand(ArgList const&) = 0;
    virtual unsigned int Ncommands() const = 0;
    virtual const_iterator begin() const = 0;
    virtual const_iterator end() const = 0;
    virtual bool NotDone() = 0;
};

/// Loop over mask expression etc
class Control_For : public Control {
  public:
    Control_For() {}
    void Help() const;
    DispatchObject* Alloc() const { return (DispatchObject*)new Control_For(); }

    int SetupControl(ArgList&);
    bool EndControl(ArgList const& a) const { return (a.CommandIs("done")); }
    void AddCommand(ArgList const& c) { commands_.push_back(c); }
    unsigned int Ncommands() const { return commands_.size(); }
    const_iterator begin() const { return commands_.begin(); }
    const_iterator end()   const { return commands_.end();   }
    bool NotDone() { return false; } // TODO
  private:
    enum ForType {ATOMS=0, RESIDUES, MOLECULES, UNKNOWN};
    AtomMask mask_;
    std::string varname_;
    ArgArray commands_;
    ForType varType_;
};
#endif
