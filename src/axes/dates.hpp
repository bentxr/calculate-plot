// Derived from Matplotlib 3.11.2: lib/matplotlib/dates.py (date2num, num2date, DateLocator, RRuleLocator,
// AutoDateLocator, YearLocator, MicrosecondLocator, DateFormatter, AutoDateFormatter) and lib/matplotlib/ticker.py
// (MultipleLocator, _Edge_integer, as they use them).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md
#pragma once

#include "axes/dateutil.hpp"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace calculate_plot::axes::dates {


// datetime.fromisoformat of 'YYYY-MM-DD[THH:MM[:SS[.ffffff]]]' (no time zone); false when it is not one
bool parseIso(const std::string& text, DateTime* out);
std::string isoFormat(const DateTime& d);  // datetime.isoformat()

// date2num: days since the epoch 1970-01-01T00:00:00 (through datetime64[us], as NumPy computes it)
double date2num(const DateTime& d);
// num2date: the datetime of a day number (np.round of the microseconds; within 20 microseconds far from the
// epoch); false and matplotlib's message outside years 1 to 9999
bool num2date(double x, DateTime* out, std::string* error);

// datetime.strftime for the directives date formats use (%Y %y %m %d %e %H %I %M %S %f %p %j %a %A %b %B %%);
// false for another one
bool strftime(const DateTime& d, const std::string& fmt, std::string* out);


// AutoDateLocator(minticks=5, maxticks=None, interval_multiples): the ticks of a view (days), from the locator
// get_locator picks for its span (YearLocator, an RRuleLocator or a MicrosecondLocator); the frequency it picked
// stays for the formatter (AutoDateFormatter reads _get_unit()).
struct AutoDateLocator {
    bool intervalMultiples = true;
    int minticks = 5;
    Freq freq = YEARLY;  // self._freq: the last one picked
    // __call__ for the view (vmin, vmax); false and why where matplotlib raises
    bool ticks(double vmin, double vmax, std::vector<double>* out, std::string* error);
    std::pair<double, double> nonsingular(double vmin, double vmax) const;
    double unit() const;  // _get_unit()
};

// AutoDateFormatter(locator) with the rcParams' formats (date.autoformatter.*): the format of the first scale
// that is at least the locator's unit
struct AutoDateFormatter {
    // year, month, day, hour, minute, second, microsecond
    std::array<std::string, 7> formats{"%Y", "%Y-%m", "%Y-%m-%d", "%m-%d %H", "%d %H:%M", "%H:%M:%S", "%M:%S.%f"};
    std::string defaultFormat = "%Y-%m-%d";
    bool format(double x, double unit, std::string* out, std::string* error) const;
};

}  // namespace calculate_plot::axes::dates
