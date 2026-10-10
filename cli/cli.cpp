#include "cli.hpp"

#include "calculate-plot/calculate-plot.hpp"

namespace calculate_plot::cli {

namespace {

const char* const kUsage =
    "usage: cplot --version | --licenses | --help\n";

const char* const kHelp =
    "cplot: draws figures from calculate-plot figure files.\n"
    "\n"
    "  --version    print the version\n"
    "  --licenses   print the third-party notices and licence texts of this program\n"
    "  --help       print this help\n";

}  // namespace

int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
    if (args.empty()) {
        err << kUsage;
        return 2;
    }
    const std::string& option = args[0];
    if (option == "--version") {
        out << "cplot " << versionString() << "\n";
        return 0;
    }
    if (option == "--licenses") {
        for (const auto& item : licenceTexts()) out << "==> " << item.path << " <==\n" << item.text << "\n";
        return 0;
    }
    if (option == "--help") {
        out << kHelp;
        return 0;
    }
    err << "cplot: unknown option " << option << "\n" << kUsage;
    return 2;
}

}  // namespace calculate_plot::cli
