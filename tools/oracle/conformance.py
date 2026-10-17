#!/usr/bin/env python3
"""Matplotlib's own image tests as a conformance count for calculate-plot.

Phase 1: every image_comparison test of matplotlib 3.11.2 with a PNG baseline runs under pytest as
matplotlib runs it (its fixtures, styles and test settings), one test module per process. A recorder
notes the calls the test makes on pyplot, on figures, axes and axes' axis, and on the artists those
calls return, and writes them as a figure file with the rcParams the test draws with (savefig's
keywords as savefig.* settings). What the recorder cannot write (a call on an object no recorded call
made, an argument that is no plain value, a change of the rcParams between calls, several figures) makes
the test "not recorded".
Phase 2: each figure file is drawn by cplot and, as a check of the recording, by matplotlib from the
exported script; both are compared at tolerance 0 (matplotlib.testing.compare.compare_images) with the
image matplotlib drew for the test, and cplot's also with the baseline (which matplotlib itself may
meet only within the test's tolerance).

  MPL_IGNORE_SYSTEM_FONTS=1 tools/oracle/.venv/bin/python -I tools/oracle/conformance.py build/native/cplot \\
      --baselines DIR [--phase2-only] [test_module ...]

The baseline images are not in the wheel: --baselines names lib/matplotlib/tests/baseline_images of
matplotlib 3.11.2's source distribution, linked into the venv's matplotlib/tests as baseline_images
(matplotlib.tests does not load without them).
Results: build/conformance/results.json and a summary on standard output.
"""
import argparse
import collections
import concurrent.futures
import functools
import json
import math
import os
import pathlib
import resource
import shutil
import subprocess
import sys
import threading

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
WORK = ROOT / "build" / "conformance"
MEMORY = 4 * 1024 ** 3  # every matplotlib process: a test may ask for far more and be killed

# ---------------------------------------------------------------------------------------------------
# The recorder (phase 1, inside pytest)


class NotRecorded(Exception):
    pass


_depth = threading.local()
ACTIVE = None  # the Recorder of the running test, if any


def plain(v, names):
    """A value as a figure file writes it."""
    import numpy as np
    if v is None or isinstance(v, (bool, str)):
        return v
    if isinstance(v, (np.bool_,)):
        return bool(v)
    if isinstance(v, (int, np.integer)):
        return int(v)
    if isinstance(v, (float, np.floating)):
        f = float(v)
        if math.isnan(f):
            return {"$float": "nan"}
        if math.isinf(f):
            return {"$float": "inf" if f > 0 else "-inf"}
        return f
    if isinstance(v, np.ma.MaskedArray):
        raise NotRecorded("a masked array")
    if isinstance(v, np.ndarray):
        if v.dtype.kind not in "biuf":
            raise NotRecorded(f"an array of {v.dtype}")
        return [plain(x, names) for x in v.tolist()] if v.ndim else plain(v.item(), names)
    if isinstance(v, tuple):
        return {"$tuple": [plain(x, names) for x in v]}
    if isinstance(v, (list, range)):
        return [plain(x, names) for x in v]
    if id(v) in names:
        raise NotRecorded(f"an object as an argument ({type(v).__name__})")
    raise NotRecorded(f"an argument of type {type(v).__module__}.{type(v).__qualname__}")


def rc_value(key, v):
    from cycler import Cycler
    if isinstance(v, Cycler):
        parts = ", ".join(f"{k}={list(vs)!r}" for k, vs in v.by_key().items())
        return f"cycler({parts})"
    return plain(v, {})


def rc_now():
    import matplotlib
    return {k: rc_value(k, matplotlib.rcParams[k]) for k in matplotlib.rcParams if k != "backend"}


class Recorder:
    def __init__(self):
        self.calls = []
        self.names = {}       # id(object) -> its name in the figure file
        self.keep = []        # the named objects, so that their ids stay theirs
        self.used = set()     # names a call was made on
        self.counter = collections.Counter()
        self.problem = None
        self.rc = None

    def fail(self, why):
        if self.problem is None:
            self.problem = why

    def name(self, obj, prefix):
        if id(obj) in self.names:
            return self.names[id(obj)]
        self.counter[prefix] += 1
        n = f"{prefix}{self.counter[prefix]}"
        self.names[id(obj)] = n
        self.keep.append(obj)
        return n

    def before(self, name, receiver, args, kwargs):
        if self.problem:
            return None
        rc = rc_now()
        if self.rc is None:
            self.rc = rc
        elif rc != self.rc:
            changed = sorted(k for k in rc if rc[k] != self.rc[k])
            self.fail("rcParams changed between calls: " + ", ".join(changed[:5]))
            return None
        call = {}
        if receiver is not None:
            on = self.names.get(id(receiver))
            if on is None:
                self.fail(f"{type(receiver).__name__}.{name} on an object no recorded call made")
                return None
            call["on"] = on
            self.used.add(on.split("[")[0])
        call["call"] = name
        try:
            call["args"] = [plain(a, self.names) for a in args]
            call["kwargs"] = {k: plain(v, self.names) for k, v in kwargs.items()}
        except NotRecorded as e:
            self.fail(f"{name}: {e}")
            return None
        self.calls.append(call)
        return call

    def after(self, call, result):
        if call is None:
            return
        names = self.result_names(result)
        if names is not None:
            call["_as"] = names

    def result_names(self, result):
        """Names for what a call returns: a figure, axes, artists, or a tuple or array of them."""
        import numpy as np
        from matplotlib.artist import Artist
        from matplotlib.axes import Axes
        from matplotlib.figure import FigureBase
        if isinstance(result, tuple):
            parts = [self.result_names(r) for r in result]
            return parts if all(isinstance(p, str) for p in parts) else None
        if isinstance(result, FigureBase):
            return self.name(result, "fig")
        if isinstance(result, Axes):
            return self.name(result, "ax")
        if isinstance(result, np.ndarray) and result.dtype == object and all(isinstance(a, Axes) for a in result.flat):
            n = self.name(result, "axs")
            for index in np.ndindex(result.shape):
                self.names.setdefault(id(result[index]), f"{n}[{', '.join(map(str, index))}]")
                self.keep.append(result[index])
            return n
        if isinstance(result, Artist):
            return self.name(result, "artist")
        if isinstance(result, list) and result and all(isinstance(a, Artist) for a in result):
            n = self.name(result, "artists")
            for i, a in enumerate(result):
                self.names.setdefault(id(a), f"{n}[{i}]")
                self.keep.append(a)
            return n
        return None

    def figure_file(self, savefig_kwargs):
        import matplotlib
        rc = {}
        defaults = {k: rc_value(k, matplotlib.rcParamsDefault[k]) for k in matplotlib.rcParamsDefault if k != "backend"}
        for k, v in (self.rc or rc_now()).items():
            if v != defaults.get(k):
                rc[k] = v
        to_rc = {"dpi": "savefig.dpi", "facecolor": "savefig.facecolor", "edgecolor": "savefig.edgecolor",
                 "pad_inches": "savefig.pad_inches", "transparent": "savefig.transparent"}
        for k, v in savefig_kwargs.items():
            if k == "pad_inches" and v == "layout":
                raise NotRecorded("savefig(pad_inches='layout')")
            if k in to_rc:
                rc[to_rc[k]] = plain(v, {})
            elif k == "bbox_inches" and v == "tight":
                rc["savefig.bbox"] = "tight"
            else:
                raise NotRecorded(f"savefig({k}=...)")
        calls = []
        for c in self.calls:
            c = dict(c)
            names = c.pop("_as", None)
            flat = names if isinstance(names, list) else [names]
            if names is not None and any(n in self.used for n in flat):
                c["as"] = names
            if not c["args"]:
                del c["args"]
            if not c["kwargs"]:
                del c["kwargs"]
            calls.append(c)
        return {"version": 1, "rc": rc, "calls": calls}


def recorded(func, receiver_first):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        rec = ACTIVE
        if rec is None or getattr(_depth, "n", 0):
            return func(*args, **kwargs)
        _depth.n = 1
        try:
            receiver, rest = (args[0], args[1:]) if receiver_first else (None, args)
            call = rec.before(func.__name__, receiver, rest, kwargs)
            result = func(*args, **kwargs)
            rec.after(call, result)
            return result
        finally:
            _depth.n = 0
    wrapper._calculate_plot_recorded = True
    return wrapper


def install_recorder():
    import inspect
    import matplotlib.pyplot as plt
    from matplotlib import artist, axis, collections as mcoll, figure, image, legend, lines, patches, spines, text
    from matplotlib.axes import Axes
    for name, f in list(vars(plt).items()):
        if not name.startswith("_") and inspect.isfunction(f) and f.__module__ == "matplotlib.pyplot" \
                and name not in ("show", "savefig", "close", "switch_backend", "isinteractive", "ion", "ioff"):
            setattr(plt, name, recorded(f, False))
    classes = set()
    pending = [artist.Artist, axis.Axis, figure.FigureBase, Axes]
    while pending:
        cls = pending.pop()
        if cls in classes:
            continue
        classes.add(cls)
        pending.extend(cls.__subclasses__())
    for cls in classes:
        for name, f in list(vars(cls).items()):
            if name.startswith("_") or not inspect.isfunction(f) or getattr(f, "_calculate_plot_recorded", False):
                continue
            if name in ("savefig", "draw", "draw_artist", "get_window_extent", "get_tightbbox", "stale_callback"):
                continue
            setattr(cls, name, recorded(f, True))
    del mcoll, image, legend, lines, patches, spines, text  # imported so that their classes are listed


class Plugin:
    """The pytest plugin: keeps the PNG image_comparison tests, records them, writes the cases."""

    def __init__(self, out, baselines):
        self.out = out
        self.baselines = baselines
        self.results = {}

    def pytest_configure(self, config):
        from matplotlib.testing import decorators
        install_recorder()
        work = self.out
        baselines = self.baselines

        def image_directories(func):
            module = pathlib.Path(sys.modules[func.__module__].__file__).stem
            result = work / "results" / module
            result.mkdir(parents=True, exist_ok=True)
            return baselines / module, result
        decorators._image_directories = image_directories
        original = decorators._ImageComparisonBase.compare
        plugin = self

        def compare(img, fig, baseline, extension, *, _lock=False):
            global ACTIVE
            rec, ACTIVE = ACTIVE, None
            node = plugin.current
            if plugin.figures is None:  # before the first of the test's figures is closed
                import matplotlib.pyplot as plt
                plugin.figures = len(set(plt.get_fignums()) - plugin.before)
            entry = {"test": node, "baseline": f"{pathlib.Path(img.baseline_dir).name}/{baseline}.{extension}"}
            entry["test"] = f"{pathlib.Path(img.baseline_dir).name}::{node.split('::')[-1]}"
            entry["actual"] = str((pathlib.Path(img.result_dir) / baseline).with_suffix(f".{extension}").relative_to(plugin.out))
            entry["tol"] = img.tol
            plugin.results.setdefault(node, []).append(entry)
            try:
                if rec is None:
                    entry["status"] = "not recorded"
                    entry["why"] = "no recorder"
                elif plugin.placeholders:
                    entry["status"] = "not recorded"
                    entry["why"] = "text drawn as placeholders (fixture text_placeholders)"
                elif plugin.figures > 1:
                    entry["status"] = "not recorded"
                    entry["why"] = "the test makes several figures"
                elif rec.problem:
                    entry["status"] = "not recorded"
                    entry["why"] = rec.problem
                else:
                    if img.remove_text:
                        entry["remove_text"] = True
                    try:
                        model = rec.figure_file(img.savefig_kwargs)
                        if img.remove_text:
                            model["calls"].extend(remove_text_calls(rec, fig))
                    except NotRecorded as e:
                        entry["status"] = "not recorded"
                        entry["why"] = str(e)
                    else:
                        case = plugin.out / "cases" / pathlib.Path(img.baseline_dir).name / baseline
                        case.mkdir(parents=True, exist_ok=True)
                        (case / "fig.json").write_text(json.dumps(model, ensure_ascii=False) + "\n")
                        entry["status"] = "recorded"
                        entry["case"] = str(case.relative_to(plugin.out))
                try:
                    original(img, fig, baseline, extension, _lock=_lock)
                    entry["matplotlib"] = "equal"
                except Exception as e:  # noqa: BLE001: matplotlib's own result against its baseline
                    entry["matplotlib"] = f"{type(e).__name__}: {str(e).splitlines()[0][:200]}"
            finally:
                ACTIVE = rec
        decorators._ImageComparisonBase.compare = compare

    def pytest_collection_modifyitems(self, config, items):
        keep = []
        for item in items:
            function = getattr(item, "function", None)
            spec = getattr(item, "callspec", None)
            if function is None or spec is None or spec.params.get("extension") != "png":
                continue
            if not is_image_comparison(function):
                continue
            keep.append(item)
        items[:] = keep

    @pytest.hookimpl(hookwrapper=True)
    def pytest_runtest_call(self, item):
        global ACTIVE
        import matplotlib.pyplot as plt
        self.current = item.nodeid
        self.figures = None
        self.placeholders = "text_placeholders" in getattr(item, "fixturenames", ())
        self.before = set(plt.get_fignums())
        ACTIVE = Recorder()
        try:
            yield
        finally:
            ACTIVE = None

    def pytest_runtest_logreport(self, report):
        if report.when == "call" and report.outcome != "passed":
            for entry in self.results.get(report.nodeid, []):
                entry.setdefault("pytest", report.outcome)
            if report.nodeid not in self.results:
                self.results[report.nodeid] = [{"test": report.nodeid, "status": "not recorded",
                                                "why": "the test did not reach the comparison (" + report.outcome + ")"}]


def is_image_comparison(function, seen=None):
    """Whether image_comparison's wrapper is among the functions this one closes over."""
    seen = set() if seen is None else seen
    if id(function) in seen or not callable(function):
        return False
    seen.add(id(function))
    code = getattr(function, "__code__", None)
    if code is not None and "baseline_images" in code.co_freevars:
        return True
    for cell in getattr(function, "__closure__", None) or ():
        try:
            inner = cell.cell_contents
        except ValueError:
            continue
        if callable(inner) and is_image_comparison(inner, seen):
            return True
        if hasattr(inner, "__dict__") and is_image_comparison(getattr(inner, "func", None), seen):
            return True
    return False


def remove_text_calls(rec, fig):
    """remove_ticks_and_titles, as calls on the figure's axes (all must be named)."""
    fig_name = rec.names.get(id(fig))
    calls = []
    if fig_name is None:
        calls.append({"call": "suptitle", "args": [""]})
    else:
        calls.append({"on": fig_name, "call": "suptitle", "args": [""]})
    for ax in fig.get_axes():
        name = rec.names.get(id(ax))
        if name is None:
            raise NotRecorded("remove_text on axes no recorded call made")
        if ax.child_axes:
            raise NotRecorded("remove_text on child axes")
        rec.used.add(name.split("[")[0])
        calls.append({"on": name, "call": "set_title", "args": [""]})
        for which in ("xaxis", "yaxis"):
            for major in ("major", "minor"):
                calls.append({"on": f"{name}.{which}", "call": f"set_{major}_formatter", "args": [{"$new": "NullFormatter"}]})
    return calls


def phase1(module, out, baselines):
    import pytest
    import matplotlib
    tests = pathlib.Path(matplotlib.__file__).parent / "tests"
    plugin = Plugin(out, baselines)
    pytest.main([str(tests / f"{module}.py"), "-q", "-p", "no:cacheprovider", "-W", "ignore",
                 "--rootdir", str(out), "-o", "addopts="], plugins=[plugin])
    results = [e for entries in plugin.results.values() for e in entries]
    (out / "phase1").mkdir(parents=True, exist_ok=True)
    (out / "phase1" / f"{module}.json").write_text(json.dumps(results, indent=1) + "\n")


# ---------------------------------------------------------------------------------------------------
# Phase 2: draw and compare

RUNNER = r"""
import runpy, sys
import matplotlib
matplotlib.use("agg")
import matplotlib.pyplot as plt
runpy.run_path(sys.argv[1], run_name="calculate_plot_conformance")
plt.gcf().savefig(sys.argv[2])
"""


def limit():
    resource.setrlimit(resource.RLIMIT_AS, (MEMORY, MEMORY))


def same(expected, actual):
    """compare_images at tolerance 0: "equal", "differs", or why the sizes differ."""
    from matplotlib.testing.compare import compare_images
    from matplotlib.testing.exceptions import ImageComparisonFailure
    try:
        return "equal" if compare_images(str(expected), str(actual), 0) is None else "differs"
    except ImageComparisonFailure as e:
        return "differs: " + str(e)[:200]
    except Exception as e:  # noqa: BLE001: an empty or unreadable image
        return "fails: " + str(e)[:200]


def draw(entry, cplot, baselines):
    """cplot's drawing and the exported script's, against matplotlib's own drawing of the test (the
    identity asked for) and against the baseline (which matplotlib may meet only within the test's
    tolerance)."""
    case = WORK / entry["case"]
    baseline = baselines / entry["baseline"]
    script = case / "fig.py"
    r = subprocess.run([cplot, "export-py", str(case / "fig.json"), str(script)], capture_output=True, text=True)
    if r.returncode:
        entry["cplot"] = "refused: " + (r.stderr.strip() or r.stdout.strip())
        return entry
    ours = case / "cplot.png"
    r = subprocess.run([cplot, "draw", str(case / "fig.json"), str(ours), "--fonts", str(ROOT / "fonts" / "ttf")],
                       capture_output=True, text=True, timeout=600)
    entry["cplot"] = "drawn" if r.returncode == 0 else "refused: " + (r.stderr.strip() or r.stdout.strip())[:300]
    theirs = case / "script.png"
    r = subprocess.run([sys.executable, "-I", "-c", RUNNER, str(script), str(theirs)], capture_output=True, text=True,
                       preexec_fn=limit, timeout=600, cwd=case)
    actual = WORK / entry["actual"]
    if r.returncode:
        entry["script"] = "fails: " + r.stderr.strip().splitlines()[-1][:300] if r.stderr.strip() else "fails"
    else:
        entry["script"] = same(actual, theirs)
    if entry["cplot"] == "drawn":
        entry["cplot"] = same(actual, ours)
        entry["cplot baseline"] = same(baseline, ours)
    return entry


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cplot")
    parser.add_argument("modules", nargs="*")
    parser.add_argument("--baselines", type=pathlib.Path, required=True,
                        help="lib/matplotlib/tests/baseline_images of matplotlib 3.11.2's source distribution")
    parser.add_argument("--phase1-module")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--phase2-only", action="store_true", help="draw what an earlier run recorded")
    a = parser.parse_args()
    if os.environ.get("MPL_IGNORE_SYSTEM_FONTS") != "1":
        sys.exit("set MPL_IGNORE_SYSTEM_FONTS=1")
    if a.phase1_module:
        limit()
        phase1(a.phase1_module, WORK, a.baselines)
        return
    import matplotlib
    assert matplotlib.__version__ == "3.11.2", matplotlib.__version__
    tests = pathlib.Path(matplotlib.__file__).parent / "tests"
    link = tests / "baseline_images"  # matplotlib.tests refuses to load without its test data
    if not link.exists():
        link.symlink_to(a.baselines.resolve(), target_is_directory=True)
    modules = a.modules or sorted(p.stem for p in tests.glob("test_*.py")
                                  if "image_comparison" in p.read_text(encoding="utf-8"))
    WORK.mkdir(parents=True, exist_ok=True)
    cplot = str(WORK / "cplot")  # a copy: a rebuild during the run does not take it away
    shutil.copy2(a.cplot, cplot)

    def run1(module):
        r = subprocess.run([sys.executable, "-I", __file__, cplot, "--phase1-module", module,
                            "--baselines", str(a.baselines)], capture_output=True, text=True, timeout=3600)
        out = WORK / "phase1" / f"{module}.json"
        if not out.exists():
            return [{"test": module, "status": "not recorded", "why": "pytest did not finish: " +
                     (r.stderr.strip().splitlines() or ["?"])[-1][:200]}]
        return json.loads(out.read_text())

    if a.phase2_only:
        entries = [e for m in modules if (WORK / "phase1" / f"{m}.json").exists()
                   for e in json.loads((WORK / "phase1" / f"{m}.json").read_text())]
    else:
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
            entries = [e for result in pool.map(run1, modules) for e in result]
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
        drawn = list(pool.map(lambda e: draw(e, cplot, a.baselines) if e.get("status") == "recorded" else e, entries))
    (WORK / "results.json").write_text(json.dumps(drawn, indent=1) + "\n")
    summarise(drawn)


def summarise(entries):
    total = len(entries)
    count = collections.Counter()
    why = collections.Counter()
    refusals = collections.Counter()
    for e in entries:
        if e.get("status") != "recorded":
            count["not recorded"] += 1
            why[e.get("why", "?").split(":")[0][:80]] += 1
            continue
        count["recorded"] += 1
        count["script " + e.get("script", "?").split(":")[0]] += 1
        c = e.get("cplot", "?")
        count["cplot " + c.split(":")[0]] += 1
        if c.startswith("refused"):
            reason = c[len("refused: "):]
            if reason.startswith("cplot: ") and ": " in reason[len("cplot: "):]:
                reason = reason[len("cplot: "):].split(": ", 1)[1]  # without the file's name
            refusals[reason[:100]] += 1
    print(f"{total} PNG baselines of image_comparison tests")
    for k, n in sorted(count.items()):
        print(f"  {k}: {n}")
    equal = sum(1 for e in entries if e.get("cplot") == "equal")
    print(f"conformance: {equal} of {total} test images drawn identically to matplotlib's "
          f"({100 * equal / max(total, 1):.1f} %)")
    baseline = sum(1 for e in entries if e.get("cplot baseline") == "equal")
    print(f"  of them identical to the baseline image too: {baseline}")
    modules = collections.defaultdict(collections.Counter)
    for e in entries:
        m = e.get("baseline", e["test"]).split("/")[0].split("::")[0]
        modules[m]["all"] += 1
        modules[m]["equal"] += e.get("cplot") == "equal"
    print("by module (equal of all):")
    for m, c in sorted(modules.items(), key=lambda kv: -kv[1]["all"]):
        print(f"  {m}: {c['equal']} of {c['all']}")
    print("why not recorded (top 25):")
    for k, n in why.most_common(25):
        print(f"  {n:5d}  {k}")
    print("why cplot refuses (top 25):")
    for k, n in refusals.most_common(25):
        print(f"  {n:5d}  {k}")


if __name__ == "__main__":
    main()
