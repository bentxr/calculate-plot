// Derived from dateutil 2.9.0.post0: dateutil/relativedelta.py (relativedelta between two datetimes, added to
// one) and dateutil/rrule.py (rrule.between for the rules matplotlib's date locators make).
// Copyright (c) 2003-2011 - Gustavo Niemeyer <gustavo@niemeyer.net>
// Licence: LICENSES/dateutil.txt (BSD-3-Clause). Changes: licensing/changes/dateutil.md
#pragma once

#include <optional>
#include <vector>

namespace calculate_plot::axes::dates {

// A naive datetime.datetime (UTC), year 1 to 9999.
struct DateTime {
    int year = 1970, month = 1, day = 1, hour = 0, minute = 0, second = 0, microsecond = 0;
};
bool operator<(const DateTime& a, const DateTime& b);
bool operator==(const DateTime& a, const DateTime& b);

// The microseconds since 1970-01-01 (proleptic Gregorian), and back (false outside years 1 to 9999)
long long toMicroseconds(const DateTime& d);
bool fromMicroseconds(long long us, DateTime* out);
long long daysFromCivil(long long y, int m, int d);
int monthDays(long long y, int m);
long long floorDiv(long long a, long long b);
constexpr long long kUsPerDay = 86400000000LL;

// datetime + timedelta(microseconds=us); false on OverflowError (outside years 1 to 9999)
bool addMicroseconds(const DateTime& d, long long us, DateTime* out);

// relativedelta(dt1, dt2): years and months, then the rest as time (normalised by _fix)
struct RelativeDelta {
    long long years = 0, months = 0, days = 0, hours = 0, minutes = 0, seconds = 0, microseconds = 0;
    RelativeDelta operator-() const { return {-years, -months, -days, -hours, -minutes, -seconds, -microseconds}; }
};
RelativeDelta relativeDelta(const DateTime& dt1, const DateTime& dt2);
// datetime + relativedelta; false where Python raises (the year leaves 1 to 9999)
bool addRelative(const DateTime& d, const RelativeDelta& r, DateTime* out);

// The rrule frequencies (dateutil's numbers) and AutoDateLocator's extra one
enum Freq { YEARLY = 0, MONTHLY = 1, WEEKLY = 2, DAILY = 3, HOURLY = 4, MINUTELY = 5, SECONDLY = 6, MICROSECONDLY = 7 };

// rrule(freq, interval, by...) from dtstart (microseconds dropped) to until: its occurrences in [after, before]
struct Rule {
    Freq freq = YEARLY;
    long long interval = 1;
    std::optional<std::vector<int>> bymonth, bymonthday, byhour, byminute, bysecond;
};
std::vector<DateTime> between(Rule rule, DateTime dtstart, const DateTime& until, const DateTime& after, const DateTime& before);

}  // namespace calculate_plot::axes::dates
