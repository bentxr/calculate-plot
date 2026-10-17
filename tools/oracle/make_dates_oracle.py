#!/usr/bin/env python3
"""Write tests/oracle/dates.json: matplotlib 3.11.2's date2num and num2date (UTC, the default epoch), and its
AutoDateLocator (with interval_multiples True and False) and AutoDateFormatter over many view intervals: the
ticks __call__ gives for an axis with that view, and the labels of each, as float.hex and text. dateutil 2.9.0
makes the rrules.

  tools/oracle/.venv/bin/python -I tools/oracle/make_dates_oracle.py
"""
import datetime
import json
import pathlib
import warnings

import matplotlib
import numpy as np
from matplotlib import dates as mdates

ROOT = pathlib.Path(__file__).resolve().parents[2]
assert matplotlib.__version__ == "3.11.2"
warnings.simplefilter("ignore")


def h(v):
    return float.hex(float(v))


def iso(dt):
    return dt.replace(tzinfo=None).isoformat()


rng = np.random.default_rng(1019)
DATES = [datetime.datetime(1970, 1, 1), datetime.datetime(2000, 2, 29, 12, 30, 15, 250000), datetime.datetime(1, 1, 1),
         datetime.datetime(9999, 12, 31, 23, 59, 59, 999999), datetime.datetime(1969, 12, 31, 23, 59, 59, 999999),
         datetime.datetime(1900, 3, 1), datetime.datetime(2024, 7, 4, 0, 0, 0, 1)]
for _ in range(40):
    us = int(rng.integers(-62135596800000000, 253402300799999999))
    DATES.append(datetime.datetime(1970, 1, 1) + datetime.timedelta(microseconds=us))
conversions = [{"datetime": iso(d), "num": h(mdates.date2num(d))} for d in DATES]
NUMS = [0.0, 0.5, -0.5, 1e-6, 19000.123456789, -719162.0, 2932896.999, 25567.000000001, 30000.5, 10000.25, -1.0e-12]
NUMS += [float(v) for v in rng.uniform(-700000, 2900000, 30)]
backs = [{"num": h(x), "datetime": iso(mdates.num2date(x))} for x in NUMS]


class FakeAxis:
    def __init__(self, vmin, vmax):
        self.v = (vmin, vmax)

    def get_view_interval(self):
        return self.v

    def get_data_interval(self):
        return self.v


SPANS = [1e-9, 3e-8, 1e-6, 5e-6, 1e-5, 4e-5, 2e-4, 1e-3, 5e-3, 0.02, 0.1, 0.3, 0.8, 1.5, 3, 7, 12, 20, 45, 60, 100, 200,
         400, 800, 1500, 3000, 7000, 15000, 40000, 100000, 400000, 1500000]
STARTS = [0.0, 19723.5, -3650.25, 10957.0, 7305.123456, 18262.999]
locators = []
for multiples in (True, False):
    for start in STARTS:
        for span in SPANS:
            vmin, vmax = start, start + span
            if vmax > 2932896 or vmin < -719162:
                continue
            loc = mdates.AutoDateLocator(interval_multiples=multiples)
            loc.set_axis(FakeAxis(vmin, vmax))
            case = {"multiples": multiples, "view": [h(vmin), h(vmax)]}
            try:
                ticks = loc()
                case["ticks"] = [h(t) for t in ticks]
                fmt = mdates.AutoDateFormatter(loc)
                case["labels"] = [fmt(t) for t in ticks]
                case["unit"] = h(loc._get_unit())
            except Exception as e:
                case["error"] = f"{type(e).__name__}: {e}"
            case["nonsingular"] = [h(v) for v in loc.nonsingular(vmin, vmax)]
            locators.append(case)
for vmin, vmax in ((5.0, 5.0), (float("nan"), 1.0), (3.0, 1.0)):
    loc = mdates.AutoDateLocator()
    locators.append({"multiples": True, "view": None, "nonsingular": [h(v) for v in loc.nonsingular(vmin, vmax)],
                     "nonsingular_of": [h(vmin), h(vmax)]})

out = {"_note": "matplotlib 3.11.2, dateutil 2.9.0.post0, tools/oracle/make_dates_oracle.py", "conversions": conversions,
       "backs": backs, "locators": locators}
(ROOT / "tests" / "oracle" / "dates.json").write_text(json.dumps(out) + "\n")
print(len(conversions), "conversions,", len(backs), "backs,", len(locators), "locator cases;",
      sum("error" in c for c in locators), "errors")
