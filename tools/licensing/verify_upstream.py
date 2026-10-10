#!/usr/bin/env python3
"""Check licensing/manifest.json against the internet: every licence file must be byte-identical to
its upstream URL, and every declared download must still have its recorded SHA-256.

The C++ test suite proves the repository matches the manifest offline; this script proves the
manifest matches upstream. Run it whenever a component is added or updated (it needs a network).
Usage: python3 tools/licensing/verify_upstream.py [--skip-downloads]
"""
import hashlib
import json
import pathlib
import sys
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "calculate-plot licence check"})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def main():
    skip_downloads = "--skip-downloads" in sys.argv
    manifest = json.loads((ROOT / "licensing/manifest.json").read_text())
    failures = 0
    for component in manifest["components"]:
        for licence in component["licenceFiles"]:
            local = (ROOT / licence["path"]).read_bytes()
            upstream = fetch(licence["upstream"])
            ok = (hashlib.sha256(upstream).hexdigest() == licence["sha256"] and upstream == local)
            if not ok and licence.get("upstreamMayChange"):
                # An unversioned URL (e.g. the Unicode licence, whose year changes): report, don't fail.
                print(f'NOTE {component["id"]}: {licence["upstream"]} changed since {licence.get("retrieved")}; '
                      f'review and update {licence["path"]}')
                continue
            failures += not ok
            print(("ok  " if ok else "BAD ") + f'{component["id"]}: {licence["path"]} == {licence["upstream"]}')
        if skip_downloads:
            continue
        source = component.get("vendoredFrom")
        if source:
            pristine = json.loads((ROOT / component["pristine"]).read_text())
            for path, digest in pristine.items():
                url = source["base"] + path[len(source.get("strip", "")):]
                ok = hashlib.sha256(fetch(url)).hexdigest() == digest
                failures += not ok
                print(("ok  " if ok else "BAD ") + f'{component["id"]}: {path} == {url}')
        for item in component["fetch"]:
            digest = hashlib.sha256(fetch(item["url"])).hexdigest()
            ok = digest == item["sha256"]
            failures += not ok
            print(("ok  " if ok else "BAD ") + f'{component["id"]}: {item["url"]} sha256 {digest}')
    print("all upstream checks passed" if not failures else f"{failures} upstream check(s) FAILED")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
