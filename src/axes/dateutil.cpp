// Derived from dateutil 2.9.0.post0: dateutil/relativedelta.py (relativedelta between two datetimes, added to
// one) and dateutil/rrule.py (rrule.between for the rules matplotlib's date locators make).
// Copyright (c) 2003-2011 - Gustavo Niemeyer <gustavo@niemeyer.net>
// Licence: LICENSES/dateutil.txt (BSD-3-Clause). Changes: licensing/changes/dateutil.md

#include "axes/dateutil.hpp"

#include <algorithm>
#include <cstdlib>
#include <tuple>

namespace calculate_plot::axes::dates {


// days_from_civil / civil_from_days: the proleptic Gregorian calendar, day 0 = 1970-01-01
long long daysFromCivil(long long y, int m, int d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civilFromDays(long long z, long long* y, int* m, int* d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    *d = int(doy - (153 * mp + 2) / 5 + 1);
    *m = int(mp < 10 ? mp + 3 : mp - 9);
    *y = yoe + era * 400 + (*m <= 2);
}

static bool isLeap(long long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
int monthDays(long long y, int m) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && isLeap(y) ? 29 : days[m - 1];
}

long long floorDiv(long long a, long long b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

// datetime + timedelta(microseconds=us); false on OverflowError (outside years 1 to 9999)
bool addMicroseconds(const DateTime& d, long long us, DateTime* out) { return fromMicroseconds(toMicroseconds(d) + us, out); }


static int sign(long long x) { return x > 0 ? 1 : x < 0 ? -1 : 0; }  // _sign: int(copysign(1, x)) (only nonzero x reach it)

static void setMonths(RelativeDelta* r, long long months) {
    r->months = months;
    if (std::llabs(months) > 11) {
        const long long s = months >= 0 ? 1 : -1;
        const long long v = r->months * s;
        r->months = (v % 12) * s;
        r->years = (v / 12) * s;
    } else {
        r->years = 0;
    }
}

// relativedelta.__add__(datetime): the years and months (the day kept within the month), then the time
bool addRelative(const DateTime& d, const RelativeDelta& r, DateTime* out) {
    long long year = d.year + r.years;
    long long month = d.month;
    if (r.months) {
        month += r.months;
        if (month > 12) year += 1, month -= 12;
        else if (month < 1) year -= 1, month += 12;
    }
    if (year < 1 || year > 9999) return false;  // (replace raises ValueError)
    DateTime moved = d;
    moved.year = int(year), moved.month = int(month);
    moved.day = std::min(monthDays(year, int(month)), d.day);
    const long long us = ((((r.days * 24 + r.hours) * 60 + r.minutes) * 60) + r.seconds) * 1000000 + r.microseconds;
    return addMicroseconds(moved, us, out);
}

// relativedelta._fix
static void fix(RelativeDelta* r) {
    const auto carry = [](long long* small, long long* big, long long limit) {
        if (std::llabs(*small) > limit - 1) {
            const long long s = sign(*small);
            const long long v = *small * s;
            *small = (v % limit) * s;
            *big += (v / limit) * s;
        }
    };
    carry(&r->microseconds, &r->seconds, 1000000);
    carry(&r->seconds, &r->minutes, 60);
    carry(&r->minutes, &r->hours, 60);
    carry(&r->hours, &r->days, 24);
    carry(&r->months, &r->years, 12);
}

RelativeDelta relativeDelta(const DateTime& dt1, const DateTime& dt2) {
    RelativeDelta r;
    long long months = (long long)(dt1.year - dt2.year) * 12 + (dt1.month - dt2.month);
    setMonths(&r, months);
    DateTime dtm;
    addRelative(dt2, r, &dtm);
    const bool before = dt1 < dt2;
    const long long increment = before ? 1 : -1;
    while (before ? dtm < dt1 : dt1 < dtm) {  // compare(dt1, dtm): gt when dt1 < dt2, else lt
        months += increment;
        setMonths(&r, months);
        addRelative(dt2, r, &dtm);
    }
    // the timedelta from the months-adjusted date: seconds and microseconds
    const long long delta = toMicroseconds(dt1) - toMicroseconds(dtm);
    const long long days = floorDiv(delta, kUsPerDay), rest = delta - days * kUsPerDay;
    r.seconds = rest / 1000000 + days * 86400;
    r.microseconds = rest % 1000000;
    fix(&r);
    return r;
}


std::vector<DateTime> between(Rule rule, DateTime dtstart, const DateTime& until, const DateTime& after, const DateTime& before) {
    dtstart.microsecond = 0;
    // the defaults: the start's time of day for what is finer than the frequency, and its day for YEARLY/MONTHLY
    if (!rule.byhour && rule.freq < HOURLY) rule.byhour = std::vector<int>{dtstart.hour};
    if (!rule.byminute && rule.freq < MINUTELY) rule.byminute = std::vector<int>{dtstart.minute};
    if (!rule.bysecond && rule.freq < SECONDLY) rule.bysecond = std::vector<int>{dtstart.second};
    if (!rule.bymonthday) {
        if (rule.freq == YEARLY) {
            if (!rule.bymonth) rule.bymonth = std::vector<int>{dtstart.month};
            rule.bymonthday = std::vector<int>{dtstart.day};
        } else if (rule.freq == MONTHLY) {
            rule.bymonthday = std::vector<int>{dtstart.day};
        }
    }
    const auto sorted = [](std::optional<std::vector<int>> v, int lo, int hi) {
        std::vector<int> out;
        if (v) out = *v;
        else
            for (int i = lo; i <= hi; ++i) out.push_back(i);
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    };
    const auto in = [](const std::optional<std::vector<int>>& v, int x) { return !v || std::find(v->begin(), v->end(), x) != v->end(); };
    const std::vector<int> months = sorted(rule.bymonth, 1, 12), monthdays = sorted(rule.bymonthday, 1, 31);
    const std::vector<int> hours = sorted(rule.byhour, 0, 23), minutes = sorted(rule.byminute, 0, 59), seconds = sorted(rule.bysecond, 0, 59);
    std::vector<DateTime> out;
    bool done = false;
    // one occurrence: kept when from dtstart to until and within [after, before]
    const auto emit = [&](const DateTime& d) {
        if (d < dtstart) return;
        if (until < d || before < d) {
            done = true;
            return;
        }
        if (!(d < after)) out.push_back(d);
    };
    const auto times = [&](DateTime d, std::size_t from) {  // the hours, minutes and seconds of a day (from a level)
        const std::vector<int> one0{d.hour}, one1{d.minute}, one2{d.second};
        for (int h : from > 0 ? one0 : hours)
            for (int mi : from > 1 ? one1 : minutes)
                for (int s : from > 2 ? one2 : seconds) {
                    if (done) return;
                    d.hour = h, d.minute = mi, d.second = s, d.microsecond = 0;
                    emit(d);
                }
    };
    if (rule.freq == YEARLY || rule.freq == MONTHLY) {
        long long index = (long long)dtstart.year * 12 + (dtstart.month - 1);
        const long long step = rule.freq == YEARLY ? 12 * rule.interval : rule.interval;
        for (; !done; index += step) {
            const long long year = floorDiv(index, 12);
            if (year > 9999) break;
            for (int m : rule.freq == YEARLY ? months : std::vector<int>{int(index - year * 12) + 1}) {
                if (!in(rule.bymonth, m)) continue;
                for (int md : monthdays) {
                    if (md > monthDays(year, m)) continue;
                    DateTime d{int(year), m, md, 0, 0, 0, 0};
                    times(d, 0);
                    if (done) break;
                }
                if (done) break;
            }
        }
        return out;
    }
    // DAILY and finer: periods of interval days, hours, minutes or seconds from the start's
    const long long unit = rule.freq == DAILY ? kUsPerDay : rule.freq == HOURLY ? 3600000000LL : rule.freq == MINUTELY ? 60000000LL : 1000000LL;
    DateTime period = dtstart;
    if (rule.freq <= DAILY) period.hour = 0;
    if (rule.freq <= HOURLY) period.minute = 0;
    if (rule.freq <= MINUTELY) period.second = 0;
    const std::size_t level = rule.freq == DAILY ? 0 : rule.freq == HOURLY ? 1 : rule.freq == MINUTELY ? 2 : 3;
    while (!done) {
        if (in(rule.bymonth, period.month) && in(rule.bymonthday, period.day) && (level < 1 || in(rule.byhour, period.hour)) &&
            (level < 2 || in(rule.byminute, period.minute)) && (level < 3 || in(rule.bysecond, period.second)))
            times(period, level);
        if (!addMicroseconds(period, unit * rule.interval, &period)) break;
        if (until < period || before < period) break;  // (a later period's occurrences come after it)
    }
    return out;
}


bool operator<(const DateTime& a, const DateTime& b) {
    return std::tie(a.year, a.month, a.day, a.hour, a.minute, a.second, a.microsecond) <
           std::tie(b.year, b.month, b.day, b.hour, b.minute, b.second, b.microsecond);
}
bool operator==(const DateTime& a, const DateTime& b) { return !(a < b) && !(b < a); }

long long toMicroseconds(const DateTime& d) {
    const long long days = daysFromCivil(d.year, d.month, d.day);
    return ((days * 24 + d.hour) * 60 + d.minute) * 60 * 1000000LL + d.second * 1000000LL + d.microsecond;
}

bool fromMicroseconds(long long us, DateTime* out) {
    const long long days = floorDiv(us, kUsPerDay);
    long long rest = us - days * kUsPerDay;
    long long y = 0;
    int m = 0, d = 0;
    civilFromDays(days, &y, &m, &d);
    if (y < 1 || y > 9999) return false;
    DateTime t;
    t.year = int(y), t.month = m, t.day = d;
    t.hour = int(rest / 3600000000LL), rest %= 3600000000LL;
    t.minute = int(rest / 60000000LL), rest %= 60000000LL;
    t.second = int(rest / 1000000LL);
    t.microsecond = int(rest % 1000000LL);
    *out = t;
    return true;
}


}  // namespace calculate_plot::axes::dates
