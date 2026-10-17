// Editing a figure file through the public API (the app's editor changes the document only this way): the calls
// it holds, a keyword or an argument set or removed, a call appended or removed. Everything not edited keeps its
// exact text (a float 3.0 stays a float, 1e-05 stays as written).
#include "calculate-plot/calculate-plot.hpp"

#include <gtest/gtest.h>

namespace cp = calculate_plot;

namespace {

const std::string kFigure =
    R"({"version": 1, "style": "ggplot", "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
    R"( {"on": "ax", "call": "plot", "args": [[0, 1.0, 2], [1e-05, 3.0, 2]], "kwargs": {"c": "C1", "linewidth": 2.50}, "as": "line"},)"
    R"( {"on": "ax", "call": "set_title", "args": ["Squares"]}]})";

}  // namespace

TEST(Edit, TheCallsOfAFigure) {
    std::string error;
    const auto calls = cp::figureCalls(kFigure, &error);
    ASSERT_EQ(error, "");
    ASSERT_EQ(calls.size(), 3u);
    EXPECT_EQ(calls[0].on, "");
    EXPECT_EQ(calls[0].method, "subplots");
    EXPECT_EQ(calls[0].as, (std::vector<std::string>{"fig", "ax"}));
    EXPECT_EQ(calls[1].on, "ax");
    EXPECT_EQ(calls[1].method, "plot");
    EXPECT_EQ(calls[1].as, (std::vector<std::string>{"line"}));
    EXPECT_EQ(calls[1].keywords, (std::vector<std::pair<std::string, std::string>>{{"c", "\"C1\""}, {"linewidth", "2.50"}}));
    EXPECT_EQ(calls[1].arguments, (std::vector<std::string>{"[0,1.0,2]", "[1e-05,3.0,2]"}));
    EXPECT_TRUE(cp::figureCalls("{", &error).empty());
    EXPECT_NE(error, "");
}

TEST(Edit, AKeywordIsSetReplacingItsAliasesAndKeepingTheRest) {
    std::string error;
    // color replaces c (an alias of the same property), in its place
    const std::string edited = cp::setKeyword(kFigure, 1, "color", R"("#ff0000")", {"c"}, &error);
    ASSERT_EQ(error, "");
    EXPECT_EQ(edited,
              R"({"version":1,"style":"ggplot","calls":[{"call":"subplots","as":["fig","ax"]},)"
              R"({"on":"ax","call":"plot","args":[[0,1.0,2],[1e-05,3.0,2]],"kwargs":{"color":"#ff0000","linewidth":2.50},"as":"line"},)"
              R"({"on":"ax","call":"set_title","args":["Squares"]}]})");
    // a new keyword goes last; a call without keywords gets them
    const std::string titled = cp::setKeyword(kFigure, 2, "fontsize", "14", {}, &error);
    ASSERT_EQ(error, "");
    EXPECT_NE(titled.find(R"({"on":"ax","call":"set_title","args":["Squares"],"kwargs":{"fontsize":14}})"), std::string::npos) << titled;
    // removing (no value) drops it and its aliases
    const std::string plain = cp::setKeyword(kFigure, 1, "color", "", {"c"}, &error);
    ASSERT_EQ(error, "");
    EXPECT_NE(plain.find(R"("kwargs":{"linewidth":2.50})"), std::string::npos) << plain;
}

TEST(Edit, AnArgumentIsSetOrAppended) {
    std::string error;
    const std::string edited = cp::setArgument(kFigure, 2, 0, R"("Cubes")", &error);
    ASSERT_EQ(error, "");
    EXPECT_NE(edited.find(R"({"on":"ax","call":"set_title","args":["Cubes"]})"), std::string::npos) << edited;
    const std::string more = cp::setArgument(kFigure, 1, 2, R"("o-")", &error);  // one past the end: appended
    ASSERT_EQ(error, "");
    EXPECT_NE(more.find(R"("args":[[0,1.0,2],[1e-05,3.0,2],"o-"])"), std::string::npos) << more;
    EXPECT_EQ(cp::setArgument(kFigure, 1, 4, "1", &error), "");
    EXPECT_EQ(error, "call 1 has 2 arguments: argument 4 cannot be set");
}

TEST(Edit, CallsAreAppendedAndRemoved) {
    std::string error;
    const std::string grid = cp::appendCall(kFigure, R"({"on": "ax", "call": "grid", "args": [true]})", &error);
    ASSERT_EQ(error, "");
    EXPECT_EQ(cp::figureCalls(grid, &error).size(), 4u);
    EXPECT_NE(grid.find(R"({"on":"ax","call":"grid","args":[true]}]})"), std::string::npos) << grid;
    const std::string removed = cp::removeCall(grid, 2, &error);
    ASSERT_EQ(error, "");
    const auto calls = cp::figureCalls(removed, &error);
    ASSERT_EQ(calls.size(), 3u);
    EXPECT_EQ(calls[2].method, "grid");
}

TEST(Edit, WhatWouldNotBeAFigureFileIsRefused) {
    std::string error;
    EXPECT_EQ(cp::setKeyword(kFigure, 7, "color", "1", {}, &error), "");
    EXPECT_EQ(error, "the figure file has no call 7");
    EXPECT_EQ(cp::setKeyword(kFigure, 1, "color", "[1, ", {}, &error), "");
    EXPECT_NE(error.find("not valid JSON"), std::string::npos) << error;
    EXPECT_EQ(cp::appendCall(kFigure, R"({"call": 3})", &error), "");
    EXPECT_NE(error, "");
    EXPECT_EQ(cp::removeCall(kFigure, 0, &error), "");  // the names it made are used later
    EXPECT_NE(error, "");
}
