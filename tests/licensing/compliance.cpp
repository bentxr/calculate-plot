#include "licensing/compliance.hpp"

#include "support/glob.hpp"
#include "support/json.hpp"
#include "support/sha256.hpp"

#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>

namespace calculate_plot::licensing {

namespace fs = std::filesystem;
using support::globMatch;
using support::Json;

namespace {

const char* const kManifest = "licensing/manifest.json";
const char* const kModifiedMark = "Modified by calculate-plot";
constexpr int kHeaderLines = 40;  // provenance must be stated near the top of a file

std::optional<std::string> readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::string firstLines(const std::string& text, int lines) {
    std::size_t at = 0;
    for (int i = 0; i < lines && at != std::string::npos; ++i) {
        at = text.find('\n', at);
        if (at != std::string::npos) ++at;
    }
    return at == std::string::npos ? text : text.substr(0, at);
}

bool isBlank(const std::string& text) { return text.find_first_not_of(" \t\r\n") == std::string::npos; }

std::vector<std::string> strings(const Json* array) {
    std::vector<std::string> out;
    if (array && array->isArray())
        for (const Json& item : array->items())
            if (item.isString()) out.push_back(item.asString());
    return out;
}

std::string text(const Json& object, const char* key) {
    const Json* value = object.find(key);
    return value && value->isString() ? value->asString() : std::string();
}

bool matchesAny(const std::vector<std::string>& patterns, const std::string& path) {
    for (const auto& pattern : patterns)
        if (globMatch(pattern, path)) return true;
    return false;
}

struct Loaded {
    std::optional<Json> manifest;
    Problems problems;
};

Loaded load(const Tree& tree) {
    Loaded loaded;
    const auto contents = readFile(tree.root / kManifest);
    if (!contents) {
        loaded.problems.push_back(std::string("the manifest ") + kManifest + " is missing");
        return loaded;
    }
    support::JsonError error;
    loaded.manifest = support::parseJson(*contents, &error);
    if (!loaded.manifest)
        loaded.problems.push_back(std::string("the manifest is not valid JSON: ") + error.message + " at byte " +
                                  std::to_string(error.offset));
    else if (!loaded.manifest->isObject())
        loaded.problems.push_back("the manifest is not a JSON object"), loaded.manifest.reset();
    return loaded;
}

const std::vector<Json>& components(const Json& manifest) {
    static const std::vector<Json> none;
    const Json* list = manifest.find("components");
    return list && list->isArray() ? list->items() : none;
}

// Every file under the root, as '/'-separated relative paths; .git is never entered, and ignored
// paths are left out unless asked for.
std::vector<std::string> files(const Tree& tree, const Json& manifest, bool includeIgnored) {
    const std::vector<std::string> ignore = strings(manifest.find("ignore"));
    std::vector<std::string> out;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(tree.root, ec); it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) break;
        // Lexically: never resolve links (a virtual environment links to the system's Python).
        const std::string rel = it->path().lexically_relative(tree.root).generic_string();
        if (it->is_directory(ec) && !it->is_symlink(ec)) {
            if (rel == ".git" || (!includeIgnored && matchesAny(ignore, rel + "/."))) it.disable_recursion_pending();
            continue;
        }
        if (!includeIgnored && matchesAny(ignore, rel)) continue;
        out.push_back(rel);
    }
    return out;
}

// Files a component claims (vendored or derived).
std::vector<std::string> claims(const Json& component) {
    std::vector<std::string> globs = strings(component.find("vendored"));
    for (const auto& g : strings(component.find("derived"))) globs.push_back(g);
    return globs;
}

// The vendored files whose contents differ from the pristine upstream copy.
std::vector<std::string> modifiedVendored(const Tree& tree, const Json& manifest, const Json& component) {
    std::vector<std::string> modified;
    const std::vector<std::string> vendored = strings(component.find("vendored"));
    if (vendored.empty()) return modified;
    const auto pristineText = readFile(tree.root / text(component, "pristine"));
    const std::optional<Json> pristine = pristineText ? support::parseJson(*pristineText, nullptr) : std::nullopt;
    for (const auto& path : files(tree, manifest, false)) {
        if (!matchesAny(vendored, path)) continue;
        const Json* expected = pristine ? pristine->find(path) : nullptr;
        const auto contents = readFile(tree.root / path);
        if (expected && contents && support::sha256Hex(*contents) != expected->asString()) modified.push_back(path);
    }
    return modified;
}

#define LOAD_OR_RETURN(tree)                     \
    Loaded loaded = load(tree);                  \
    if (!loaded.manifest) return loaded.problems; \
    const Json& manifest = *loaded.manifest

}  // namespace

Problems checkManifest(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    const Json* version = manifest.find("version");
    if (!version || !version->isNumber() || version->asNumber() != 1)
        problems.push_back("the manifest version must be 1");
    for (const char* key : {"ignore", "own", "forbidden", "components"}) {
        const Json* value = manifest.find(key);
        if (!value || !value->isArray()) problems.push_back(std::string("the manifest needs an array \"") + key + "\"");
    }
    if (text(manifest, "notice").empty()) problems.push_back("the manifest must name the NOTICE file");

    const std::vector<std::string> all = files(tree, manifest, false);
    std::set<std::string> ids;
    for (const Json& component : components(manifest)) {
        const std::string id = text(component, "id");
        if (id.empty()) {
            problems.push_back("a component has no id");
            continue;
        }
        if (!ids.insert(id).second) problems.push_back("component id \"" + id + "\" is used twice");
        for (const char* key : {"name", "version", "homepage", "use", "licence"})
            if (text(component, key).empty()) problems.push_back(id + ": missing \"" + key + "\"");
        const std::string use = text(component, "use");
        if (use != "runtime" && use != "test" && use != "build")
            problems.push_back(id + ": unknown use \"" + use + "\" (runtime, test or build)");
        const Json* licenceFiles = component.find("licenceFiles");
        if (!licenceFiles || !licenceFiles->isArray() || licenceFiles->size() == 0)
            problems.push_back(id + ": no licence files");
        else
            for (const Json& file : licenceFiles->items())
                if (text(file, "path").empty() || text(file, "upstream").empty() || text(file, "sha256").size() != 64)
                    problems.push_back(id + ": a licence file needs path, upstream and a SHA-256");
        for (const char* key : {"fetch", "vendored", "derived", "header", "notices"}) {
            const Json* value = component.find(key);
            if (!value || !value->isArray()) problems.push_back(id + ": \"" + key + "\" must be an array");
        }
        if (const Json* fetch = component.find("fetch"))
            for (const Json& item : fetch->items())
                if (text(item, "url").empty() || text(item, "sha256").size() != 64)
                    problems.push_back(id + ": a download needs a url and a SHA-256");
        for (const auto& pattern : claims(component)) {
            bool any = false;
            for (const auto& path : all) any = any || globMatch(pattern, path);
            if (!any) problems.push_back(id + ": the pattern \"" + pattern + "\" matches no file");
        }
        if (!strings(component.find("vendored")).empty() && text(component, "pristine").empty())
            problems.push_back(id + ": vendored files need a pristine list");
    }
    return problems;
}

Problems checkLicenceTexts(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    for (const Json& component : components(manifest)) {
        const Json* licenceFiles = component.find("licenceFiles");
        if (!licenceFiles) continue;
        for (const Json& file : licenceFiles->items()) {
            const std::string path = text(file, "path");
            const auto contents = readFile(tree.root / path);
            if (!contents)
                problems.push_back(text(component, "id") + ": the licence file " + path + " is missing");
            else if (support::sha256Hex(*contents) != text(file, "sha256"))
                problems.push_back(text(component, "id") + ": " + path +
                                   " is not the verbatim upstream text (SHA-256 differs from " + text(file, "upstream") + ")");
        }
    }
    return problems;
}

Problems checkEveryFileAccounted(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    const std::vector<std::string> own = strings(manifest.find("own"));
    std::set<std::string> licenceFiles;
    for (const Json& component : components(manifest))
        if (const Json* list = component.find("licenceFiles"))
            for (const Json& file : list->items()) licenceFiles.insert(text(file, "path"));
    for (const auto& path : files(tree, manifest, false)) {
        if (path == kManifest || licenceFiles.count(path)) continue;
        std::vector<std::string> owners;
        for (const Json& component : components(manifest))
            if (matchesAny(claims(component), path)) owners.push_back(text(component, "id"));
        if (owners.size() > 1)
            problems.push_back(path + " is claimed by more than one component (" + owners[0] + ", " + owners[1] + ")");
        else if (owners.empty() && !matchesAny(own, path))
            problems.push_back(path + " is not accounted for: neither ours nor a declared component's");
    }
    return problems;
}

Problems checkProvenanceHeaders(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    const std::vector<std::string> all = files(tree, manifest, false);
    for (const Json& component : components(manifest)) {
        const std::vector<std::string> derived = strings(component.find("derived"));
        const std::string origin = "Derived from " + text(component, "name") + " " + text(component, "version");
        for (const auto& path : all) {
            if (!matchesAny(derived, path)) continue;
            const std::string head = firstLines(readFile(tree.root / path).value_or(""), kHeaderLines);
            if (head.find(origin) == std::string::npos)
                problems.push_back(path + " does not say \"" + origin + "\" near its top");
            for (const auto& line : strings(component.find("header")))
                if (head.find(line) == std::string::npos)
                    problems.push_back(path + " does not carry \"" + line + "\" near its top");
        }
    }
    return problems;
}

Problems checkNotice(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    const std::string noticePath = text(manifest, "notice");
    const auto notice = readFile(tree.root / noticePath);
    if (!notice) return {"the notice file " + noticePath + " is missing"};
    for (const Json& component : components(manifest)) {
        if (text(component, "use") != "runtime") continue;
        const std::string nameVersion = text(component, "name") + " " + text(component, "version");
        if (notice->find(nameVersion) == std::string::npos)
            problems.push_back(noticePath + " does not name " + nameVersion);
        for (const auto& line : strings(component.find("notices")))
            if (notice->find(line) == std::string::npos)
                problems.push_back(noticePath + " lacks the notice \"" + line + "\" of " + text(component, "id"));
    }
    return problems;
}

Problems checkChangeSummaries(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    for (const Json& component : components(manifest)) {
        const std::string id = text(component, "id");
        const bool changed = !strings(component.find("derived")).empty() ||
                             !modifiedVendored(tree, manifest, component).empty();
        if (!changed) continue;
        const std::string path = text(component, "changes");
        const auto summary = path.empty() ? std::nullopt : readFile(tree.root / path);
        if (!summary || isBlank(*summary)) {
            problems.push_back(id + ": changed code needs a summary of the changes (\"changes\")");
            continue;
        }
        for (const auto& field : strings(component.find("changesMustSay")))
            if (summary->find(field) == std::string::npos)
                problems.push_back(id + ": the summary of changes " + path + " must say \"" + field + "\"");
    }
    return problems;
}

Problems checkForbiddenFiles(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    const std::vector<std::string> forbidden = strings(manifest.find("forbidden"));
    for (const auto& path : files(tree, manifest, true))
        if (matchesAny(forbidden, path)) problems.push_back(path + " must never be in the tree (forbidden)");
    return problems;
}

Problems checkFetchedDependencies(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    std::set<std::pair<std::string, std::string>> inBuild;  // (url, sha256)
    for (const auto& path : files(tree, manifest, false)) {
        const std::string name = fs::path(path).filename().string();
        if (name != "CMakeLists.txt" && fs::path(path).extension() != ".cmake") continue;
        const std::string cmake = readFile(tree.root / path).value_or("");
        for (std::size_t at = cmake.find("FetchContent_Declare("); at != std::string::npos;
             at = cmake.find("FetchContent_Declare(", at + 1)) {
            const std::size_t end = cmake.find(')', at);
            std::istringstream words(cmake.substr(at, end - at));
            std::string word, url, hash;
            bool git = false;
            while (words >> word) {
                if (word == "URL") words >> url;
                else if (word == "URL_HASH") words >> hash;
                else if (word == "GIT_REPOSITORY") git = true;
            }
            if (git) problems.push_back(path + ": GIT_REPOSITORY downloads are not pinned by a hash; use URL and URL_HASH");
            if (url.empty()) continue;
            if (hash.rfind("SHA256=", 0) != 0) {
                problems.push_back(path + ": the download " + url + " has no SHA-256 hash");
                continue;
            }
            inBuild.insert({url, hash.substr(7)});
        }
    }
    std::set<std::pair<std::string, std::string>> declared;
    for (const Json& component : components(manifest)) {
        if (const Json* fetch = component.find("fetch"))
            for (const Json& item : fetch->items()) {
                const auto key = std::make_pair(text(item, "url"), text(item, "sha256"));
                declared.insert(key);
                if (!inBuild.count(key))
                    problems.push_back(text(component, "id") + ": declares the download " + key.first +
                                       " that the build does not make (with that hash)");
            }
    }
    for (const auto& item : inBuild)
        if (!declared.count(item))
            problems.push_back("the build downloads " + item.first + " (SHA-256 " + item.second +
                               ") but no component declares it");
    return problems;
}

Problems checkVendoredFiles(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    for (const Json& component : components(manifest)) {
        const std::vector<std::string> vendored = strings(component.find("vendored"));
        if (vendored.empty()) continue;
        const std::string pristinePath = text(component, "pristine");
        const auto pristineText = readFile(tree.root / pristinePath);
        const std::optional<Json> pristine = pristineText ? support::parseJson(*pristineText, nullptr) : std::nullopt;
        if (!pristine) {
            problems.push_back(text(component, "id") + ": the pristine list " + pristinePath + " is missing or invalid");
            continue;
        }
        for (const auto& path : files(tree, manifest, false)) {
            if (!matchesAny(vendored, path)) continue;
            if (!pristine->find(path))
                problems.push_back(path + " is vendored but not in the pristine list " + pristinePath);
        }
        for (const auto& path : modifiedVendored(tree, manifest, component)) {
            const std::string head = firstLines(readFile(tree.root / path).value_or(""), kHeaderLines);
            if (head.find(kModifiedMark) == std::string::npos)
                problems.push_back(path + " differs from its upstream copy but is not marked \"" + kModifiedMark + "\"");
        }
    }
    return problems;
}

Problems checkIgnoredPaths(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    const std::vector<std::string> ignore = strings(manifest.find("ignore"));
    std::istringstream lines(readFile(tree.root / ".gitignore").value_or(""));
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') continue;
        // "/dir/" is anchored at the root; "dir/" may appear anywhere; a trailing '/' means a directory.
        const bool anchored = line[0] == '/';
        std::string pattern = anchored ? line.substr(1) : "**/" + line;
        if (!pattern.empty() && pattern.back() == '/') pattern += "**";
        bool found = false;
        for (const auto& g : ignore) found = found || g == pattern;
        if (!found) problems.push_back(".gitignore skips \"" + line + "\" but the manifest's ignore list lacks \"" + pattern + "\"");
    }
    return problems;
}

Problems checkOwnFiles(const Tree& tree) {
    LOAD_OR_RETURN(tree);
    Problems problems;
    const std::vector<std::string> allowed = strings(manifest.find("mayMentionCopyright"));
    std::set<std::string> licenceFiles;
    for (const Json& component : components(manifest))
        if (const Json* list = component.find("licenceFiles"))
            for (const Json& file : list->items()) licenceFiles.insert(text(file, "path"));
    static const std::regex marks(R"(copyright|\(c\)\s*[0-9]{4}|spdx-license-identifier)", std::regex::icase);
    for (const auto& path : files(tree, manifest, false)) {
        if (path == kManifest || licenceFiles.count(path) || matchesAny(allowed, path)) continue;
        bool claimed = false;
        for (const Json& component : components(manifest)) claimed = claimed || matchesAny(claims(component), path);
        if (claimed) continue;
        const std::string contents = readFile(tree.root / path).value_or("");
        if (contents.find('\0') != std::string::npos) continue;  // binary: images, fonts
        if (std::regex_search(contents, marks))
            problems.push_back(path + " is listed as ours but carries a copyright or licence mark: "
                               "declare where it comes from, or list it under mayMentionCopyright");
    }
    return problems;
}

Problems checkAll(const Tree& tree) {
    Problems all;
    for (auto check : {checkManifest, checkLicenceTexts, checkEveryFileAccounted, checkProvenanceHeaders, checkNotice,
                       checkChangeSummaries, checkForbiddenFiles, checkFetchedDependencies, checkVendoredFiles,
                       checkIgnoredPaths, checkOwnFiles})
        for (auto& problem : check(tree)) all.push_back(std::move(problem));
    return all;
}

}  // namespace calculate_plot::licensing
