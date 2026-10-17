#include "support/glob.hpp"

namespace calculate_plot::support {

namespace {

// One segment: '*' matches any run of characters, '?' exactly one (segments hold no '/').
bool segmentMatch(std::string_view pattern, std::string_view text) {
    if (pattern.empty()) return text.empty();
    if (pattern[0] == '*') {
        for (std::size_t skip = 0; skip <= text.size(); ++skip)
            if (segmentMatch(pattern.substr(1), text.substr(skip))) return true;
        return false;
    }
    if (text.empty()) return false;
    if (pattern[0] != '?' && pattern[0] != text[0]) return false;
    return segmentMatch(pattern.substr(1), text.substr(1));
}

std::string_view firstSegment(std::string_view path) { return path.substr(0, path.find('/')); }

std::string_view afterFirstSegment(std::string_view path) {
    const auto slash = path.find('/');
    return slash == std::string_view::npos ? std::string_view() : path.substr(slash + 1);
}

bool hasMore(std::string_view path) { return path.find('/') != std::string_view::npos; }

}  // namespace

bool globMatch(std::string_view pattern, std::string_view path) {
    if (pattern.empty()) return path.empty();
    const std::string_view head = firstSegment(pattern);
    const std::string_view rest = afterFirstSegment(pattern);
    const bool patternContinues = hasMore(pattern);
    if (head == "**") {
        if (!patternContinues) return !path.empty();  // a trailing "**" matches at least one segment
        // Zero segments, or consume one and try again.
        if (globMatch(rest, path)) return true;
        return hasMore(path) && globMatch(pattern, afterFirstSegment(path));
    }
    if (path.empty() || !segmentMatch(head, firstSegment(path))) return false;
    if (patternContinues != hasMore(path)) return false;
    return !patternContinues || globMatch(rest, afterFirstSegment(path));
}

}  // namespace calculate_plot::support
