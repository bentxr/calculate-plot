#include "cli.hpp"

#include "calculate-plot/calculate-plot.hpp"
#include "png/png.hpp"
#include "render/replay.hpp"
#include "render/svg_replay.hpp"

#include <fstream>
#include <optional>
#include <sstream>

namespace calculate_plot::cli {

namespace {

const char* const kUsage =
    "usage: cplot --version | --licenses | --help\n"
    "       cplot replay TRACE.json OUT.png --fonts DIR\n"
    "       cplot replay-svg SVG-TRACE.json OUT.svg --fonts DIR\n";

const char* const kHelp =
    "cplot: draws figures from calculate-plot figure files.\n"
    "\n"
    "  --version    print the version\n"
    "  --licenses   print the third-party notices and licence texts of this program\n"
    "  --help       print this help\n"
    "  replay       draw a trace recorded from matplotlib's Agg renderer as a PNG\n"
    "  replay-svg   write a trace recorded from matplotlib's SVG renderer as SVG\n"
    "  --fonts DIR  where the fonts named in a trace are\n";

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool writeFile(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    out << bytes;
    return bool(out);
}

// replay / replay-svg: TRACE OUT --fonts DIR
int replay(const std::vector<std::string>& args, std::ostream& err) {
    if (args.size() != 5 || args[3] != "--fonts") {
        err << kUsage;
        return 2;
    }
    const auto trace = readFile(args[1]);
    if (!trace) {
        err << "cplot: cannot read " << args[1] << "\n";
        return 1;
    }
    std::string output, error;
    if (args[0] == "replay") {
        render::ReplayOptions options;
        options.text = render::TextMode::Text;
        options.fontDirectory = args[4];
        const auto result = render::replayTrace(*trace, options);
        error = result.error;
        output = png::encodePng(result.image, {{"Software", "calculate-plot " + std::string(versionString())}});
    } else {
        const auto result = render::replaySvgTrace(*trace, args[4]);
        error = result.error;
        output = result.svg;
    }
    if (!error.empty()) {
        err << "cplot: " << args[1] << ": " << error << "\n";
        return 1;
    }
    if (!writeFile(args[2], output)) {
        err << "cplot: cannot write " << args[2] << "\n";
        return 1;
    }
    return 0;
}

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
    if (option == "replay" || option == "replay-svg") return replay(args, err);
    if (option == "--help") {
        out << kHelp;
        return 0;
    }
    err << "cplot: unknown option " << option << "\n" << kUsage;
    return 2;
}

}  // namespace calculate_plot::cli
