// Functions as figure files write them ({"$lambda": "<body>"}) against NumPy 2.5.3's evaluation of
// `lambda x: <body>` over float64 arrays (tests/oracle/lambda.json, written by tools/oracle/make_lambda_oracle.py).
#include "support/json.hpp"
#include "support/pylambda.hpp"
#include "trace_compare.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using calculate_plot::support::Json;
using calculate_plot::support::PyLambda;

namespace {

const Json& oracle() {
    static const Json o = [] {
        std::ifstream in(fs::path(CALCULATE_PLOT_SOURCE_DIR) / "tests/oracle/lambda.json", std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return *calculate_plot::support::parseJson(text.str(), nullptr);
    }();
    return o;
}

double hex(const Json& v) { return std::strtod(v.asString().c_str(), nullptr); }

}  // namespace

TEST(PyLambda, ValuesAreNumpys) {
    std::vector<std::vector<double>> arrays;
    for (const Json& a : oracle().find("arrays")->items()) {
        arrays.emplace_back();
        for (const Json& v : a.items()) arrays.back().push_back(hex(v));
    }
    int checked = 0;
    for (const Json& c : oracle().find("taken")->items()) {
        const std::string body = c.find("body")->asString();
        std::string error;
        const auto f = PyLambda::parse(body, &error);
        ASSERT_TRUE(f.has_value()) << body << ": " << error;
        for (std::size_t k = 0; k < arrays.size(); ++k) {
            std::vector<double> got;
            ASSERT_TRUE(f->apply(arrays[k], &got, &error)) << body << ": " << error;
            const Json& want = (*c.find("results"))[k];
            ASSERT_EQ(got.size(), want.size()) << body;
            for (std::size_t i = 0; i < got.size(); ++i) {
                const double w = hex(want[i]);
                EXPECT_TRUE((std::isnan(got[i]) && std::isnan(w)) || calculate_plot::testing_support::sameNumber(got[i], w))
                    << body << " at x = " << arrays[k][i] << ": " << got[i] << " instead of " << w;
                ++checked;
            }
        }
    }
    EXPECT_EQ(checked, 138 * 147);
}

TEST(PyLambda, WhatPythonRaisesIsRefused) {
    for (const Json& c : oracle().find("errors")->items()) {
        const std::string body = c.find("body")->asString();
        std::string error;
        const auto f = PyLambda::parse(body, &error);
        std::vector<double> out;
        EXPECT_TRUE(!f || !f->apply({1.0}, &out, &error)) << body;
        EXPECT_FALSE(error.empty()) << body;
    }
}

TEST(PyLambda, WhatIsNotPortedIsRefused) {
    for (const Json& b : oracle().find("unsupported")->items()) {
        std::string error;
        const auto f = PyLambda::parse(b.asString(), &error);
        std::vector<double> out;
        EXPECT_TRUE(!f || !f->apply({1.0}, &out, &error)) << b.asString();
        EXPECT_FALSE(error.empty()) << b.asString();
    }
    std::string error;
    EXPECT_FALSE(PyLambda::parse("2 * y", &error));
    EXPECT_EQ(error, "unknown name y (the variable is x; NumPy is np)");
    EXPECT_FALSE(PyLambda::parse("np.tanh(x)", &error));
    EXPECT_EQ(error, "np.tanh is not supported");
    EXPECT_FALSE(PyLambda::parse("np.pi", &error));
    EXPECT_EQ(error, "the function does not use x");
    // what is not understood is named with its position (counted from 1)
    EXPECT_FALSE(PyLambda::parse("x $ 2", &error));
    EXPECT_EQ(error, "unexpected $ at position 3");
    EXPECT_FALSE(PyLambda::parse("x * $", &error));
    EXPECT_EQ(error, "unexpected $ at position 5");
    EXPECT_FALSE(PyLambda::parse("x + )", &error));
    EXPECT_EQ(error, "unexpected ) at position 5");
}

TEST(PyLambda, KnowsWhetherItUsesNumpy) {
    std::string error;
    EXPECT_FALSE(PyLambda::parse("x * 1.8 + 32", &error)->usesNumpy());
    EXPECT_TRUE(PyLambda::parse("np.sqrt(x)", &error)->usesNumpy());
    EXPECT_TRUE(PyLambda::parse("x * np.pi", &error)->usesNumpy());
}

// Over a masked array (scatter's offsets, their non-finite values masked) NumPy's masked arithmetic applies:
// division and powers mask what is not finite, the domained functions mask their domain; masked is NaN.
TEST(PyLambda, MaskedValuesAreNumpys) {
    std::vector<std::vector<double>> arrays;
    for (const Json& a : oracle().find("arrays")->items()) {
        arrays.emplace_back();
        for (const Json& v : a.items()) arrays.back().push_back(hex(v));
    }
    int checked = 0;
    for (const Json& c : oracle().find("taken")->items()) {
        const std::string body = c.find("body")->asString();
        std::string error;
        const auto f = PyLambda::parse(body, &error);
        ASSERT_TRUE(f.has_value()) << body << ": " << error;
        for (std::size_t k = 0; k < arrays.size(); ++k) {
            std::vector<double> got;
            ASSERT_TRUE(f->apply(arrays[k], &got, &error, true)) << body << ": " << error;
            const Json& want = (*c.find("masked"))[k];
            for (std::size_t i = 0; i < got.size(); ++i) {
                const double w = hex(want[i]);
                EXPECT_TRUE((std::isnan(got[i]) && std::isnan(w)) || calculate_plot::testing_support::sameNumber(got[i], w))
                    << body << " (masked) at x = " << arrays[k][i] << ": " << got[i] << " instead of " << w;
                ++checked;
            }
        }
    }
    EXPECT_EQ(checked, 138 * 147);
}
