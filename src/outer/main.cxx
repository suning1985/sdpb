#include "Mesh.hxx"
#include "Functional.hxx"

#include "../ostream_sequence.hxx"

// We convert the optimization problem into a regular linear
// programming problem.
//
// 1) Each polynomial in Block::polys adds two variables.  The
// weights of those polynomials are unbounded, while linear
// programming requires a strictly positive variable.  So we
// substitute W_n = w_n+ - w_n+, where both w_n+ and w_n- are
// strictly positive.  All blocks should have the same number of
// polynomials, so we only need to look at the first one.
//
// 2) Each constraint adds one 'slack' variable s_n.  There is one
// constraint per coordinate 'x', with each block having multiple,
// separate coordinates.
//
// 3) One more global variable delta, which gives the linear
// program something to minimize.
//
// This turns the problem
//
//   A_0 . W > 0
//   A_1 . W > 0
//   ...
//
// into
//
//   min delta
//
// where
//
//   A_0 . (w_+ - w_-) + delta - s_0 = 0
//   A_1 . (w_+ - w_-) + delta - s_1 = 0
//   ...
//   w_n+, w_n-, s_n, delta >= 0
//
// There is a constraint for every point that is sampled.  At the
// beginning, we sample the min and max for each block, so there are
// 2*num_blocks constraints.  For the single correlator example, there
// is an additional constraint on the first block at x=0.  In general
//
// => num_rows == num_constraints
// => num_columns == 2*num_weights + num_constraints + 1

std::vector<El::BigFloat>
load_vector(const boost::filesystem::path &vector_path);
bool is_feasible(const Functional &functional);
std::vector<El::BigFloat>
compute_optimal(const Functional &functional,
                const std::vector<El::BigFloat> &normalization,
                const std::vector<El::BigFloat> &objective);

int main(int argc, char **argv)
{
  El::Environment env(argc, argv);
  const int64_t precision(64);
  El::gmp::SetPrecision(precision);

  {
    Functional functional("test/single_corr_polys", "test/single_corr_poles");
    const bool is_single_corr_feasible(is_feasible(functional));
    std::cout << "feasible: " << std::boolalpha << is_single_corr_feasible
              << "\n";
  }
  {
    Functional functional("test/toy_polys");
    std::vector<El::BigFloat> objective(load_vector("test/toy_objective"));
    std::vector<El::BigFloat> normalization(
      load_vector("test/toy_normalization"));
    std::vector<El::BigFloat> weights(
      compute_optimal(functional, normalization, objective));
    El::BigFloat optimal(0);
    for(size_t index(0); index < objective.size(); ++index)
      {
        optimal += objective[index] * weights[index];
      }
    std::cout.precision(precision / 3.3);
    std::cout << "optimal: " << optimal << " " << weights << "\n";
  }
}
