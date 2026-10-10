#pragma once

#include <string_view>
#include <vector>

namespace calculate_plot {

// The library's version, "major.minor.patch".
std::string_view versionString();

// The third-party notices and licence texts that must travel with every copy of the library:
// NOTICE first, then the licence of every component compiled into it. Show them in an "About" or
// "Licences" view of any program that ships calculate-plot.
struct LicenceText {
    std::string_view path;  // where the text lives in the source tree, e.g. "LICENSES/FTL.txt"
    std::string_view text;
};
std::vector<LicenceText> licenceTexts();

}  // namespace calculate_plot
