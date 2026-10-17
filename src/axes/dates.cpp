// Derived from Matplotlib 3.11.2: lib/matplotlib/dates.py and lib/matplotlib/ticker.py (MultipleLocator,
// _Edge_integer).
// Copyright (c) 2012- Matplotlib Development Team; All Rights Reserved
// Licence: LICENSES/Matplotlib.txt. Changes: licensing/changes/matplotlib.md

#include "axes/dates.hpp"

#include "axes/ticker.hpp"
#include "support/pyformat.hpp"
#include "support/pynum.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <tuple>

namespace calculate_plot::axes::dates {

namespace {

constexpr double kMusecondsPerDay = 1e6 * 86400.0;  // MUSECONDS_PER_DAY
DateTime utcStart() { return DateTime{1, 1, 1, 0, 0, 0, 0}; }
DateTime utcStop() { return DateTime{9999, 12, 31, 23, 59, 59, 0}; }

// RRuleLocator.tick_values: the rule from vmin - delta to vmax + delta, its dates in [vmin, vmax] (or the two ends)
std::vector<double> rruleTicks(const Rule& rule, const DateTime& vmin, const DateTime& vmax) {
    const RelativeDelta delta = relativeDelta(vmax, vmin);
    DateTime start, stop;
    if (!addRelative(vmin, -delta, &start)) start = utcStart();
    if (!addRelative(vmax, delta, &stop)) stop = utcStop();
    const auto dates = between(rule, start, stop, vmin, vmax);
    std::vector<double> out;
    if (dates.empty()) return {date2num(vmin), date2num(vmax)};
    for (const auto& d : dates) out.push_back(date2num(d));
    return out;
}

// _Edge_integer(step, 0): le and ge of a float
double edgeLe(double x, double step) {
    const auto [d, m] = support::divmod(x, step);
    if (std::fabs(m / step - 1) < 1e-10) return d + 1;
    return d;
}
double edgeGe(double x, double step) {
    const auto [d, m] = support::divmod(x, step);
    if (std::fabs(m / step - 0) < 1e-10) return d;
    return d + 1;
}

}  // namespace

bool parseIso(const std::string& s, DateTime* out) {
    DateTime d;
    const auto digits = [&](std::size_t at, std::size_t n, int* v) {
        if (at + n > s.size()) return false;
        int x = 0;
        for (std::size_t i = at; i < at + n; ++i) {
            if (s[i] < '0' || s[i] > '9') return false;
            x = x * 10 + (s[i] - '0');
        }
        *v = x;
        return true;
    };
    if (!digits(0, 4, &d.year) || s.size() < 10 || s[4] != '-' || !digits(5, 2, &d.month) || s[7] != '-' || !digits(8, 2, &d.day))
        return false;
    std::size_t at = 10;
    if (at < s.size()) {
        if (s[at] != 'T' && s[at] != ' ') return false;
        if (!digits(at + 1, 2, &d.hour) || at + 3 >= s.size() || s[at + 3] != ':' || !digits(at + 4, 2, &d.minute)) return false;
        at += 6;
        if (at < s.size()) {
            if (s[at] != ':' || !digits(at + 1, 2, &d.second)) return false;
            at += 3;
            if (at < s.size()) {
                if (s[at] != '.' || (s.size() - at - 1 != 3 && s.size() - at - 1 != 6)) return false;
                int f = 0;
                if (!digits(at + 1, s.size() - at - 1, &f)) return false;
                d.microsecond = s.size() - at - 1 == 3 ? f * 1000 : f;
                at = s.size();
            }
        }
    }
    if (d.year < 1 || d.month < 1 || d.month > 12 || d.day < 1 || d.day > monthDays(d.year, d.month) || d.hour > 23 ||
        d.minute > 59 || d.second > 59)
        return false;
    *out = d;
    return true;
}

std::string isoFormat(const DateTime& d) {
    char text[64];
    std::snprintf(text, sizeof text, "%04d-%02d-%02dT%02d:%02d:%02d", d.year, d.month, d.day, d.hour, d.minute, d.second);
    std::string out = text;
    if (d.microsecond) {
        std::snprintf(text, sizeof text, ".%06d", d.microsecond);
        out += text;
    }
    return out;
}

double date2num(const DateTime& d) {
    // datetime64[us] -> seconds (floored) and the rest in ns; (seconds - epoch) + ns / 1e9, over SEC_PER_DAY
    const long long us = toMicroseconds(d);
    const long long seconds = floorDiv(us, 1000000);
    const long long extraNs = (us - seconds * 1000000) * 1000;
    double dt = double(seconds);
    dt += double(extraNs) / 1.0e9;
    return dt / 86400.0;
}

bool num2date(double x, DateTime* out, std::string* error) {
    const double us = std::nearbyint(x * kMusecondsPerDay);  // int(np.round(x * MUSECONDS_PER_DAY))
    DateTime d;
    if (!(std::fabs(us) < 4e17) || !fromMicroseconds((long long)us, &d)) {
        if (error) *error = "Date ordinal " + support::pythonRepr(x) + " converts to a date outside years 0001 to 9999 (using epoch "
                            "1970-01-01T00:00:00), but Matplotlib dates must be between year 0001 and 9999.";
        return false;
    }
    if (std::fabs(x) > 70 * 365) {  // far from the epoch: to 20 microseconds
        const long long ms = (long long)std::nearbyint(d.microsecond / 20.0) * 20;
        if (ms == 1000000) {
            d.microsecond = 0;
            if (!addMicroseconds(d, 1000000, &d)) return false;
        } else {
            d.microsecond = int(ms);
        }
    }
    *out = d;
    return true;
}

bool strftime(const DateTime& d, const std::string& fmt, std::string* out) {
    static const char* const monthNames[] = {"January", "February", "March", "April", "May", "June", "July",
                                             "August", "September", "October", "November", "December"};
    static const char* const dayNames[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    std::string s;
    char buf[32];
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%' || i + 1 == fmt.size()) {
            s += fmt[i];
            continue;
        }
        const char c = fmt[++i];
        const long long days = daysFromCivil(d.year, d.month, d.day);
        const int weekday = int(((days % 7) + 7 + 3) % 7);  // 1970-01-01 was a Thursday (Monday = 0)
        switch (c) {
        case 'Y': std::snprintf(buf, sizeof buf, "%04d", d.year); break;
        case 'y': std::snprintf(buf, sizeof buf, "%02d", d.year % 100); break;
        case 'm': std::snprintf(buf, sizeof buf, "%02d", d.month); break;
        case 'd': std::snprintf(buf, sizeof buf, "%02d", d.day); break;
        case 'e': std::snprintf(buf, sizeof buf, "%2d", d.day); break;
        case 'H': std::snprintf(buf, sizeof buf, "%02d", d.hour); break;
        case 'I': std::snprintf(buf, sizeof buf, "%02d", d.hour % 12 == 0 ? 12 : d.hour % 12); break;
        case 'M': std::snprintf(buf, sizeof buf, "%02d", d.minute); break;
        case 'S': std::snprintf(buf, sizeof buf, "%02d", d.second); break;
        case 'f': std::snprintf(buf, sizeof buf, "%06d", d.microsecond); break;
        case 'p': std::snprintf(buf, sizeof buf, "%s", d.hour < 12 ? "AM" : "PM"); break;
        case 'j': std::snprintf(buf, sizeof buf, "%03lld", days - daysFromCivil(d.year, 1, 1) + 1); break;
        case 'a': std::snprintf(buf, sizeof buf, "%.3s", dayNames[weekday]); break;
        case 'A': std::snprintf(buf, sizeof buf, "%s", dayNames[weekday]); break;
        case 'b': std::snprintf(buf, sizeof buf, "%.3s", monthNames[d.month - 1]); break;
        case 'B': std::snprintf(buf, sizeof buf, "%s", monthNames[d.month - 1]); break;
        case '%': std::snprintf(buf, sizeof buf, "%%"); break;
        default: return false;
        }
        s += buf;
    }
    *out = s;
    return true;
}

std::pair<double, double> AutoDateLocator::nonsingular(double vmin, double vmax) const {
    if (!std::isfinite(vmin) || !std::isfinite(vmax)) return {0.0, 1.0};  // (1970-01-01, 1970-01-02)
    if (vmax < vmin) std::swap(vmin, vmax);
    if (vmin == vmax) vmin = vmin - 365.0 * 2, vmax = vmax + 365.0 * 2;
    return {vmin, vmax};
}

double AutoDateLocator::unit() const {
    switch (freq) {
    case YEARLY: return 365.0;
    case MONTHLY: return 30.0;
    case WEEKLY: return 7.0;
    case DAILY: return 1.0;
    case HOURLY: return 1.0 / 24.0;
    case MINUTELY: return 1.0 / (60.0 * 24.0);
    case SECONDLY: return 1.0 / 86400.0;
    case MICROSECONDLY: return 1.0 / kMusecondsPerDay;
    }
    return -1;
}

bool AutoDateLocator::ticks(double vmin, double vmax, std::vector<double>* out, std::string* error) {
    // viewlim_to_dt
    if (vmin > vmax) std::swap(vmin, vmax);
    DateTime dmin, dmax;
    if (!num2date(vmin, &dmin, error) || !num2date(vmax, &dmax, error)) return false;
    // get_locator: the span in each unit
    const RelativeDelta delta = relativeDelta(dmax, dmin);
    const long long tdeltaUs = toMicroseconds(dmax) - toMicroseconds(dmin);
    const long long tdays = floorDiv(tdeltaUs, kUsPerDay);
    const double totalSeconds = double(tdeltaUs) / 1e6;  // timedelta.total_seconds()
    const double numYears = double(delta.years);
    const double numMonths = numYears * 12.0 + double(delta.months);
    const double numDays = double(tdays);
    const double numHours = numDays * 24.0 + double(delta.hours);
    const double numMinutes = numHours * 60.0 + double(delta.minutes);
    const double nums[7] = {numYears, numMonths, numDays, numHours, numMinutes, std::floor(totalSeconds), std::floor(totalSeconds * 1000000.0)};
    static const Freq freqs[7] = {YEARLY, MONTHLY, DAILY, HOURLY, MINUTELY, SECONDLY, MICROSECONDLY};
    static const int maxticks[7] = {11, 12, 11, 12, 11, 11, 8};
    std::vector<std::vector<long long>> intervald = {
        {1, 2, 4, 5, 10, 20, 40, 50, 100, 200, 400, 500, 1000, 2000, 4000, 5000, 10000},
        {1, 2, 3, 4, 6},
        {1, 2, 3, 7, 14, 21},
        {1, 2, 3, 4, 6, 12},
        {1, 5, 10, 15, 30},
        {1, 5, 10, 15, 30},
        {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000}};
    if (intervalMultiples) intervald[2] = {1, 2, 4, 7, 14};
    const int fullLo[7] = {0, 1, 1, 0, 0, 0, 0}, fullHi[7] = {-1, 12, 31, 23, 59, 59, -1};  // self._byranges
    // byranges: the by-rule of each unit; None (unset optional with `none` true) or a list
    struct By {
        bool none = false;
        std::vector<int> values;
    };
    By byranges[7] = {{true, {}}, {false, {1}}, {false, {1}}, {false, {0}}, {false, {0}}, {false, {0}}, {true, {}}};
    std::size_t i = 0;
    long long interval = 1;
    bool found = false;
    for (; i < 7; ++i) {
        if (nums[i] < minticks) {
            byranges[i].none = true;
            continue;
        }
        interval = intervald[i].back();
        for (long long candidate : intervald[i])
            if (nums[i] <= double(candidate) * double(maxticks[i] - 1)) {
                interval = candidate;
                break;
            }
        freq = freqs[i];
        const bool hasRange = fullHi[i] >= 0;
        if (hasRange && intervalMultiples) {
            std::vector<int> values;
            for (int v = fullLo[i]; v <= fullHi[i]; v += int(interval)) values.push_back(v);  // range[::interval]
            byranges[i] = {false, values};
            if (i == DAILY || i == WEEKLY) {  // (the index compared with the frequencies' numbers, as matplotlib does)
                if (interval == 14) byranges[i] = {false, {1, 15}};
                else if (interval == 7) byranges[i] = {false, {1, 8, 15, 22}};
            }
            interval = 1;
        } else {
            if (hasRange) {
                std::vector<int> values;
                for (int v = fullLo[i]; v <= fullHi[i]; ++v) values.push_back(v);
                byranges[i] = {false, values};
            } else {
                byranges[i] = {true, {}};
            }
        }
        found = true;
        break;
    }
    if (!found) interval = 1, i = 6;
    const Freq chosen = found ? freqs[i] : MICROSECONDLY;
    if (chosen == YEARLY && intervalMultiples) {
        // YearLocator(interval): Jan 1 of the multiples of the interval, from the year at or below vmin
        const double step = double(interval);
        const double ymin = std::max(edgeLe(double(dmin.year), step) * step, 1.0);
        const double ymax = std::min(edgeGe(double(dmax.year), step) * step, 9999.0);
        DateTime start = dmin;
        start.year = int(ymin), start.month = 1, start.day = 1, start.hour = start.minute = start.second = 0;
        DateTime stop = start;
        stop.year = int(ymax);
        Rule rule;
        rule.freq = YEARLY, rule.interval = interval;
        rule.bymonth = std::vector<int>{1}, rule.bymonthday = std::vector<int>{1};
        rule.byhour = std::vector<int>{0}, rule.byminute = std::vector<int>{0}, rule.bysecond = std::vector<int>{0};
        const auto dates = between(rule, start, stop, start, stop);
        out->clear();
        if (dates.empty()) {
            *out = {date2num(start), date2num(stop)};
        } else {
            for (const auto& d : dates) out->push_back(date2num(d));
        }
        return true;
    }
    if (chosen != MICROSECONDLY) {
        Rule rule;
        rule.freq = chosen, rule.interval = interval;
        const auto set = [](const By& b, std::optional<std::vector<int>>* target) {
            if (b.none) target->reset();
            else *target = b.values;
        };
        set(byranges[1], &rule.bymonth);
        set(byranges[2], &rule.bymonthday);
        set(byranges[3], &rule.byhour);
        set(byranges[4], &rule.byminute);
        set(byranges[5], &rule.bysecond);
        *out = rruleTicks(rule, dmin, dmax);
        return true;
    }
    // MicrosecondLocator(interval): a MultipleLocator over the microseconds of the day of vmin
    const double nmin0 = date2num(dmin), nmax0 = date2num(dmax);
    const double t0 = std::floor(nmin0);
    double nmax = nmax0 - t0, nmin = nmin0 - t0;
    nmin *= kMusecondsPerDay;
    nmax *= kMusecondsPerDay;
    const double step = double(interval);
    if (nmax < nmin) std::swap(nmin, nmax);
    const double lo = edgeGe(nmin, step) * step;
    const double n = support::divmod(nmax - lo + 0.001 * step, step).first;
    out->clear();
    for (double k = 0; k < n + 3; ++k) out->push_back(((lo - step + k * step) + 0.0) / kMusecondsPerDay + t0);
    return true;
}

bool AutoDateFormatter::format(double x, double unit, std::string* out, std::string* error) const {
    // the scales in order: microsecond, second, minute, hour, day, month, year
    const double scales[7] = {1.0 / kMusecondsPerDay, 1.0 / 86400.0, 1.0 / (60.0 * 24.0), 1.0 / 24.0, 1.0, 30.0, 365.0};
    const std::string* fmt = &defaultFormat;
    for (int k = 0; k < 7; ++k)
        if (scales[k] >= unit) {
            fmt = &formats[std::size_t(6 - k)];
            break;
        }
    DateTime d;
    if (!num2date(x, &d, error)) return false;
    if (!strftime(d, *fmt, out)) return *error = "the date format " + *fmt + " is not supported yet", false;
    return true;
}

}  // namespace calculate_plot::axes::dates
