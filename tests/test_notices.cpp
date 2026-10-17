#include "calculate-plot/calculate-plot.hpp"
#include "support/json.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace {

std::string fileText(const std::string& relative) {
    std::ifstream in(std::string(CALCULATE_PLOT_SOURCE_DIR) + "/" + relative, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

}  // namespace

TEST(Notices, NoticeComesFirst) {
    const auto texts = calculate_plot::licenceTexts();
    ASSERT_FALSE(texts.empty());
    EXPECT_EQ(texts.front().path, "NOTICE");
}

TEST(Notices, EveryEmbeddedTextIsByteIdenticalToItsFile) {
    for (const auto& item : calculate_plot::licenceTexts())
        EXPECT_EQ(std::string(item.text), fileText(std::string(item.path))) << item.path;
}

TEST(Notices, TheBinaryCarriesExactlyTheDistributedComponentsLicences) {
    // NOTICE plus the licence files of every component that ships in the library ("use": "runtime").
    calculate_plot::support::JsonError error;
    const auto manifest = calculate_plot::support::parseJson(fileText("licensing/manifest.json"), &error);
    ASSERT_TRUE(manifest.has_value()) << error.message;
    std::set<std::string> expected{"NOTICE"};
    for (const auto& component : manifest->find("components")->items())
        if (component.find("use")->asString() == "runtime")
            for (const auto& file : component.find("licenceFiles")->items())
                expected.insert(file.find("path")->asString());
    std::set<std::string> embedded;
    for (const auto& item : calculate_plot::licenceTexts()) embedded.insert(std::string(item.path));
    EXPECT_EQ(embedded, expected);
}
