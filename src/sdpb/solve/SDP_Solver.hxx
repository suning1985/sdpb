//=======================================================================
// Copyright 2014-2015 David Simmons-Duffin.
// Distributed under the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#pragma once

#include "Block_Diagonal_Matrix.hxx"
#include "Block_Matrix.hxx"
#include "Block_Vector.hxx"
#include "SDP.hxx"
#include "SDP_Solver_Terminate_Reason.hxx"

#include "../SDP_Solver_Parameters.hxx"
#include "../Timers.hxx"

#include <boost/filesystem.hpp>

// SDPSolver contains the data structures needed during the running of
// the interior point algorithm.  Each structure is allocated when an
// SDPSolver is initialized, and reused in each iteration.
//
class SDP_Solver
{
public:
  // a Vector of length P = sdp.primalObjective.size()
  Block_Vector x;

  // a Block_Diagonal_Matrix with block sizes given by
  // sdp.psdMatrixBlockDims()
  Block_Diagonal_Matrix X;

  // a Vector of length N = sdp.dualObjective.size()
  Block_Vector y;

  // a Block_Diagonal_Matrix with the same structure as X
  Block_Diagonal_Matrix Y;

  /********************************************/
  // Solver status

  // NB: here, primalObjective and dualObjective refer to the current
  // values of the objective functions.  In the class SDP, they refer
  // to the vectors c and b.  Hopefully the name-clash won't cause
  // confusion.
  El::BigFloat primal_objective, // f + c . x
    dual_objective,              // f + b . y
    duality_gap;                 // normalized difference of objectives

  // Discrepancy in the primal equality constraints, a
  // Block_Diagonal_Matrix with the same structure as X, called 'P' in
  // the manual:
  //
  //   PrimalResidues = \sum_p A_p x_p - X
  //
  Block_Diagonal_Matrix primal_residues;
  El::BigFloat primal_error; // maxAbs(PrimalResidues)

  // Discrepancy in the dual equality constraints, a Vector of length
  // P, called 'd' in the manual:
  //
  //   dualResidues = c - Tr(A_* Y) - B y
  //
  Block_Vector dual_residues;
  El::BigFloat dual_error; // maxAbs(dualResidues)

  SDP_Solver(const SDP_Solver_Parameters &parameters,
             const Block_Info &block_info, const El::Grid &grid,
             const size_t &dual_objective_b_height,
             const boost::filesystem::path &checkpoint_directory);

  SDP_Solver_Terminate_Reason
  run(const SDP_Solver_Parameters &parameters,
      const boost::filesystem::path &checkpoint_directory,
      const Block_Info &block_info, const SDP &sdp, const El::Grid &grid,
      Timers &timers);

  void step(const SDP_Solver_Parameters &parameters, const size_t &iteration,
            const std::chrono::time_point<std::chrono::high_resolution_clock>
              &solver_start_time,
            const std::size_t &total_psd_rows,
            const bool &is_primal_and_dual_feasible,
            const Block_Info &block_info, const SDP &sdp, const El::Grid &grid,
            const Block_Diagonal_Matrix &X_cholesky,
            const Block_Diagonal_Matrix &Y_cholesky,
            const Block_Diagonal_Matrix &bilinear_pairings_X_inv,
            const Block_Diagonal_Matrix &bilinear_pairings_Y,
            El::BigFloat &primal_step_length, El::BigFloat &dual_step_length,
            SDP_Solver_Terminate_Reason &terminate_reason, bool &terminate_now,
            Timers &timers);

  void save_solution(const SDP_Solver_Terminate_Reason,
                     const boost::filesystem::path &out_file) const;
  void
  save_checkpoint(const boost::filesystem::path &checkpoint_directory) const;
  bool load_checkpoint(const boost::filesystem::path &checkpoint_directory);
};
