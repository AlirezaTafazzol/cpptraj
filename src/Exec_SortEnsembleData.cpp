#include "Exec_SortEnsembleData.h"
#include "CpptrajStdio.h"
#include "DataSet_PH.h"
#include "StringRoutines.h" // doubleToString

// Exec_SortEnsembleData::Help()
void Exec_SortEnsembleData::Help() const
{

}

inline bool CheckError(int err) {
# ifdef MPI
  if (Parallel::EnsembleComm().CheckError( err )) return 1;
# else
  if (err != 0) return 1;
# endif
  return 0;
}

//  Exec_SortEnsembleData::Sort_pH_Data()
int Exec_SortEnsembleData::Sort_pH_Data(DataSetList const& setsToSort, DataSetList& OutputSets)
const
{
  // Cast sets back to DataSet_PH
  typedef std::vector<DataSet_PH*> Parray;
  Parray PHsets;
  for (DataSetList::const_iterator ds = setsToSort.begin(); ds != setsToSort.end(); ++ds)
    PHsets.push_back( (DataSet_PH*)*ds );
  // Gather initial pH data values, ensure no duplicates
  typedef std::vector<double> Darray;
  Darray pHvalues;
  // member0 will hold index of first ensemble member.
# ifdef MPI
  pHvalues.resize( Parallel::Ensemble_Size() );
  Darray phtmp;
  for (Parray::const_iterator ds = PHsets.begin(); ds != PHsets.end(); ++ds)
    phtmp.push_back( (*ds)->pH_Values()[0] );
  if (Parallel::EnsembleComm().AllGather(&phtmp[0], phtmp.size(), MPI_DOUBLE, &pHvalues[0])) {
    rprinterr("Error: Gathering pH values.\n");
    return 1;
  }
# else
  for (Parray::const_iterator ds = PHsets.begin(); ds != PHsets.end(); ++ds)
    pHvalues.push_back( (*ds)->pH_Values()[0] );
# endif
  ReplicaInfo::Map<double> pH_map;
  if (pH_map.CreateMap( pHvalues )) {
    rprinterr("Error: Duplicate pH value detected (%.2f) in ensemble.\n", pH_map.Duplicate());
    return 1;
  }
  Darray sortedPH;
  mprintf("\tInitial pH values:");
  for (ReplicaInfo::Map<double>::const_iterator ph = pH_map.begin(); ph != pH_map.end(); ++ph)
  {
    mprintf(" %6.2f", ph->first);
    sortedPH.push_back( ph->first );
  }
  mprintf("\n");

  // Create sets to hold sorted pH values. Create a set for each pH value.
  // TODO check that residue info all the same
  MetaData md = PHsets[0]->Meta();
  unsigned int nframes = PHsets[0]->Nframes();
  rprintf("DEBUG: Sorting %u frames for %zu sets, %zu pH values.\n",
          nframes, PHsets.size(), pHvalues.size());
  for (unsigned int idx = 0; idx != sortedPH.size(); idx++) {
    OutputSets.SetEnsembleNum( idx );
    DataSet_PH* out = (DataSet_PH*)OutputSets.AddSet( DataSet::PH, md );
    if (out==0) return 1;
    out->SetLegend( "pH " + doubleToString( sortedPH[idx] ) );
    out->SetResidueInfo( PHsets[0]->Residues() );
    out->Resize( nframes );
  }

  for (unsigned int n = 0; n < nframes; n++)
  {
    for (Parray::const_iterator ds = PHsets.begin(); ds != PHsets.end(); ++ds)
    {
      float phval = (*ds)->pH_Values()[n];
      int idx = pH_map.FindIndex( phval );
      //rprintf("DEBUG: %6u Set %10s pH= %6.2f going to %2i\n", n+1, (*ds)->legend(), phval, idx);
      //mflush();
      DataSet_PH* out = (DataSet_PH*)OutputSets[idx];
      for (unsigned int res = 0; res < (*ds)->Residues().size(); res++)
      {
        //if (res == 0 && idx == 0) {
        //  rprintf("DEBUG: Frame %3u res %2u State %2i pH %6.2f\n", 
        //          n, res, (*ds)->Res(res).State(n), phval);
        //  mflush();
        //}
        out->SetState(res, n, (*ds)->Res(res).State(n), phval);
      }
    }
  }
# ifdef MPI
  // Now we need to reduce down each set onto the thread where it belongs.
  if (Parallel::World().Size() > 1) {
    typedef std::vector<int> Iarray;
    Iarray setDest( sortedPH.size(), 0 ); // TODO should this be done in Parallel?
    for (int idx = 0; idx != (int)sortedPH.size(); idx++) {
      DataSet_PH* out = (DataSet_PH*)OutputSets[idx];
      if (idx >= Parallel::Ensemble_Beg() && idx < Parallel::Ensemble_End()) {
        // OutputSet[idx] belongs to this thread.
        rprintf("DEBUG: %s belongs to me.\n", out->legend());
        setDest[idx] = Parallel::EnsembleComm().Rank();
      } else {
        // OutputSet[idx] belongs to another thread.
        rprintf("DEBUG: %s belongs to someone else.\n", out->legend());
      }
    }
    Iarray setDestination( sortedPH.size(), 0 );
    Parallel::EnsembleComm().AllReduce(&setDestination[0], &setDest[0], sortedPH.size(),
                                       MPI_INT, MPI_SUM);
    for (Iarray::const_iterator it = setDestination.begin(); it != setDestination.end(); ++it)
      mprintf("DEBUG: Set %u belongs to rank %i\n", it-setDestination.begin(), *it);
    for (int idx = 0; idx != (int)sortedPH.size(); idx++) {
      DataSet_PH* out = (DataSet_PH*)OutputSets[idx];
      mprintf("DEBUG: Consolidate set %s to rank %i\n", out->legend(), setDestination[idx]);
      out->Consolidate( Parallel::EnsembleComm(), setDestination[idx] );
    }
    // Remove sets that do not belong on this rank
    for (int idx = (int)sortedPH.size() - 1; idx > -1; idx--) {
      if (setDestination[idx] != Parallel::EnsembleComm().Rank()) {
        rprintf("DEBUG: Remove set %s (%i) from rank %i\n", OutputSets[idx]->legend(),
                idx, Parallel::EnsembleComm().Rank());
        OutputSets.RemoveSet( OutputSets[idx] );
     } 
    }
  }
# endif
  return 0;
}

// Exec_SortEnsembleData::SortData()
int Exec_SortEnsembleData::SortData(DataSetList const& setsToSort, DataSetList& OutputSets)
const
{
  int err = 0;
  if (setsToSort.empty()) {
    rprinterr("Error: No sets selected.\n");
    err = 1;
  }
  if (CheckError(err)) return 1;
# ifdef MPI
  // Number of sets to sort should be equal to # members I am responsible for.
  if (Parallel::N_Ens_Members() != (int)setsToSort.size()) {
    rprinterr("Internal Error: Number of ensemble members (%i) != # sets to sort (%zu)\n",
               Parallel::N_Ens_Members(), setsToSort.size());
    return 1;
  }
# endif

  DataSet::DataType dtype = setsToSort[0]->Type();
  for (DataSetList::const_iterator ds = setsToSort.begin(); ds != setsToSort.end(); ++ds) {
    rprintf("\t%s\n", (*ds)->legend());
    if ((*ds)->Size() < 1) { //TODO check sizes match
      rprinterr("Error: Set '%s' is empty.\n", (*ds)->legend());
      err = 1;
      break;
    }
    if (dtype != (*ds)->Type()) {
      rprinterr("Error: Set '%s' has different type than first set.\n", (*ds)->legend());
      err = 1;
      break;
    }
  }
  if (CheckError(err)) return 1; 

# ifdef MPI
  Parallel::EnsembleComm().Barrier(); // DEBUG
  typedef std::vector<int> Iarray;
  Iarray Dtypes( Parallel::EnsembleComm().Size(), -1 );
  if ( Parallel::EnsembleComm().AllGather( &dtype, 1, MPI_INT, &Dtypes[0] ) ) return 1;
  for (int rank = 1; rank < Parallel::EnsembleComm().Size(); rank++)
    if (Dtypes[0] != Dtypes[rank]) {
      rprinterr("Error: Set types on rank %i do not match types on rank 0.\n", rank);
      err = 1;
      break;
    }
  if (Parallel::EnsembleComm().CheckError( err )) return 1;
# endif

  // Only work for pH data for now.
  if (dtype != DataSet::PH) {
    rprinterr("Error: Only works for pH data for now.\n");
    return 1;
  }

  err = Sort_pH_Data( setsToSort, OutputSets );

  return err;
}

// Exec_SortEnsembleData::Execute()
Exec::RetType Exec_SortEnsembleData::Execute(CpptrajState& State, ArgList& argIn)
{
  rprintf("DEBUG: Entering sortensembledata.\n");
  DataSetList setsToSort;
  std::string dsarg = argIn.GetStringNext();
  while (!dsarg.empty()) {
    setsToSort += State.DSL().GetMultipleSets( dsarg );
    dsarg = argIn.GetStringNext();
  }
  setsToSort.List();

  int err = 0;
# ifdef MPI
  // For now, require ensemble mode in parallel.
  if (Parallel::EnsembleComm().IsNull()) {
    rprinterr("Error: Data set ensemble sort requires ensemble mode in parallel.\n");
    return CpptrajState::ERR;
  }
  // Only TrajComm masters have complete data.
  if (Parallel::TrajComm().Master()) {
    DataSetList OutputSets;
    err = SortData( setsToSort, OutputSets );
    if (err == 0) {
      // Remove unsorted sets. 
      for (DataSetList::const_iterator ds = setsToSort.begin(); ds != setsToSort.end(); ++ds)
        State.DSL().RemoveSet( *ds );
      // Add sorted sets.
      for (DataSetList::const_iterator ds = OutputSets.begin(); ds != OutputSets.end(); ++ds) {
        rprintf("DEBUG: Sorted set: %s\n", (*ds)->legend());
        State.DSL().AddSet( *ds );
      }
      // Since sorted sets have been transferred to master DSL, OutputSets now
      // just has copies.
      OutputSets.SetHasCopies( true );
    }
  }
  if (Parallel::World().CheckError( err ))
# else
  if (SortData( setsToSort )) 
# endif
    return CpptrajState::ERR;
  return CpptrajState::OK;
}
