#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Checks that a source tree honours every licence of the code it borrows, as declared in its
// licensing/manifest.json. Each check returns the problems it found, one sentence each; an empty
// list means compliant. The checks are unit-tested on fabricated trees, then run on this repository.
namespace calculate_plot::licensing {

using Problems = std::vector<std::string>;

struct Tree {
    std::filesystem::path root;
};

Problems checkManifest(const Tree& tree);              // the manifest is well formed and consistent
Problems checkLicenceTexts(const Tree& tree);          // licence files exist and are verbatim upstream copies
Problems checkEveryFileAccounted(const Tree& tree);    // each file is ours or covered by exactly one component
Problems checkProvenanceHeaders(const Tree& tree);     // derived files name their origin and licence
Problems checkNotice(const Tree& tree);                // NOTICE carries every distributed component's notices
Problems checkChangeSummaries(const Tree& tree);       // derived or modified code summarises its changes
Problems checkForbiddenFiles(const Tree& tree);        // files we must never ship are absent
Problems checkFetchedDependencies(const Tree& tree);   // every download in the build is a declared component
Problems checkVendoredFiles(const Tree& tree);         // vendored copies are pristine or marked as modified
Problems checkIgnoredPaths(const Tree& tree);          // the walk skips exactly what git ignores
Problems checkOwnFiles(const Tree& tree);              // our own files carry no one else's copyright

Problems checkAll(const Tree& tree);

}  // namespace calculate_plot::licensing
