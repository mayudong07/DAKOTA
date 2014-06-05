/*  _______________________________________________________________________

    DAKOTA: Design Analysis Kit for Optimization and Terascale Applications
    Copyright 2014 Sandia Corporation.
    This software is distributed under the GNU Lesser General Public License.
    For more information, see the README file in the top Dakota directory.
    _______________________________________________________________________ */

//- Class:        RelaxedVarConstraints
//- Description:  Class implementation
//- Owner:        Mike Eldred

#include "RelaxedVarConstraints.hpp"
#include "ProblemDescDB.hpp"
#include "dakota_data_io.hpp"
#include "dakota_data_util.hpp"

static const char rcsId[]="@(#) $Id";


namespace Dakota {

/** In this class, a relaxed data approach is used in which continuous
    and discrete arrays are combined into a single continuous array
    (integrality is relaxed; the converse of truncating reals is not
    currently supported but could be in the future if needed).
    Iterators which use this class include: BranchBndOptimizer. */
RelaxedVarConstraints::
RelaxedVarConstraints(const ProblemDescDB& problem_db,
		      const SharedVariablesData& svd):
  Constraints(BaseConstructor(), problem_db, svd)
{
  const SizetArray& vc_totals = svd.components_totals();
  size_t num_cdv = vc_totals[0], num_cauv = vc_totals[3],
    num_ceuv  = vc_totals[6], num_csv = vc_totals[9],
    num_ddrv  = sharedVarsData.vc_lookup(DISCRETE_DESIGN_RANGE),
    num_ddsiv = sharedVarsData.vc_lookup(DISCRETE_DESIGN_SET_INT),
    num_dauiv = vc_totals[4], num_deuiv = vc_totals[7],
    num_dsrv  = sharedVarsData.vc_lookup(DISCRETE_STATE_RANGE),
    num_dssiv = sharedVarsData.vc_lookup(DISCRETE_STATE_SET_INT),
    num_ddsrv = vc_totals[2], num_daurv = vc_totals[5],
    num_deurv = vc_totals[8],
    num_acv   = num_cdv + num_cauv + num_ceuv + num_csv +
                vc_totals[1] + num_dauiv + num_deuiv + vc_totals[10] +
                num_ddsrv + num_daurv + num_deurv + vc_totals[11];

  allContinuousLowerBnds.sizeUninitialized(num_acv);
  allContinuousUpperBnds.sizeUninitialized(num_acv);

  int start = 0;
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_design.lower_bounds"), allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_design.upper_bounds"), allContinuousUpperBnds, start);
  start += num_cdv;
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_design_range.lower_bounds"),
    allContinuousLowerBnds, start);
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_design_range.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_ddrv;
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_design_set_int.lower_bounds"),
    allContinuousLowerBnds, start);
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_design_set_int.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_ddsiv;
  copy_data_partial(problem_db.get_rv(
    "variables.discrete_design_set_real.lower_bounds"),
    allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
    "variables.discrete_design_set_real.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_ddsrv;
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_aleatory_uncertain.lower_bounds"),
    allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_aleatory_uncertain.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_cauv;
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_aleatory_uncertain_int.lower_bounds"),
    allContinuousLowerBnds, start);
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_aleatory_uncertain_int.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_dauiv;
  copy_data_partial(problem_db.get_rv(
    "variables.discrete_aleatory_uncertain_real.lower_bounds"),
    allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
    "variables.discrete_aleatory_uncertain_real.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_daurv;
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_epistemic_uncertain.lower_bounds"),
    allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_epistemic_uncertain.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_ceuv;
  merge_data_partial(problem_db.get_iv(
   "variables.discrete_epistemic_uncertain_int.lower_bounds"),
   allContinuousLowerBnds, start);
  merge_data_partial(problem_db.get_iv(
   "variables.discrete_epistemic_uncertain_int.upper_bounds"),
   allContinuousUpperBnds, start);
  start += num_deuiv;
  copy_data_partial(problem_db.get_rv(
   "variables.discrete_epistemic_uncertain_real.lower_bounds"),
   allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
   "variables.discrete_epistemic_uncertain_real.upper_bounds"),
   allContinuousUpperBnds, start);
  start += num_deurv;
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_state.lower_bounds"), allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
    "variables.continuous_state.upper_bounds"), allContinuousUpperBnds, start);
  start += num_csv;
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_state_range.lower_bounds"),
    allContinuousLowerBnds, start);
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_state_range.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_dsrv;
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_state_set_int.lower_bounds"),
    allContinuousLowerBnds, start);
  merge_data_partial(problem_db.get_iv(
    "variables.discrete_state_set_int.upper_bounds"),
    allContinuousUpperBnds, start);
  start += num_dssiv;
  copy_data_partial(problem_db.get_rv(
    "variables.discrete_state_set_real.lower_bounds"),
    allContinuousLowerBnds, start);
  copy_data_partial(problem_db.get_rv(
    "variables.discrete_state_set_real.upper_bounds"),
    allContinuousUpperBnds, start);

  // Construct active/inactive views of all arrays
  build_views();

  // Manage linear constraints.
  manage_linear_constraints(problem_db);

#ifdef REFCOUNT_DEBUG
  Cout << "Letter instantiated: variablesView active = " 
       << sharedVarsData.view().first << " inactive = " 
       << sharedVarsData.view().second << std::endl;
#endif
}


void RelaxedVarConstraints::read(std::istream& s)
{
  read_data(s, allContinuousLowerBnds);
  read_data(s, allContinuousUpperBnds);
}


void RelaxedVarConstraints::write(std::ostream& s) const
{
  write_data(s, allContinuousLowerBnds);
  write_data(s, allContinuousUpperBnds);
}

} // namespace Dakota
