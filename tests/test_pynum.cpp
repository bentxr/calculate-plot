// Python's numeric semantics that ported code relies on.
#include "support/pynum.hpp"

#include <gtest/gtest.h>

using calculate_plot::support::pythonSum;

namespace {

// sum() as CPython 3.14 computes it (values checked with the oracle's Python): leading ints exactly,
// the first float added plainly, then Neumaier's compensated summation (ints included).
TEST(PythonSum, CompensatesAsCPython) {
    EXPECT_EQ(pythonSum({1e16, 1.0, -1e16}, {false, false, false}), 1.0);
    EXPECT_EQ(pythonSum({1e16, 1, -1e16}, {false, true, false}), 1.0);
    EXPECT_EQ(pythonSum({1, 1e16, 1, -1e16}, {true, false, true, false}), 1.0);
    EXPECT_EQ(pythonSum({1e16, 1, 1, -1e16}, {false, true, true, false}), 2.0);
    EXPECT_EQ(pythonSum(std::vector<double>(10, 0.1), std::vector<bool>(10, false)), 1.0);
    EXPECT_EQ(pythonSum({1.7, 1.7, 1, 0.5, 1.7}, {false, false, true, false, false}), 0x1.a666666666666p+2);
    EXPECT_EQ(pythonSum({}, {}), 0.0);
    EXPECT_EQ(pythonSum({2, 3}, {true, true}), 5.0);
}

}  // namespace
