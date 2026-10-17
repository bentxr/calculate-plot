# Changes to dateutil code (BSD-3-Clause: redistribution keeps the notice)

src/axes/dateutil.hpp and src/axes/dateutil.cpp contain, translated to C++ from dateutil 2.9.0.post0 (with the
datetime arithmetic they need, our own):
- relativedelta(dt1, dt2) from dateutil/relativedelta.py: the months between two datetimes, adjusted until
  they do not overshoot, the rest as seconds and microseconds, _fix's normalisation, and relativedelta added to
  a datetime (years, months with the day kept within the month, then the time), with Python's datetime limits
  (years 1 to 9999) as the failures matplotlib catches;
- rrule.between(after, before, inc=True) from dateutil/rrule.py, for the rules matplotlib's date locators make
  (YEARLY to SECONDLY, an interval, bymonth, bymonthday, byhour, byminute and bysecond as lists or one value,
  and rrule's defaults from dtstart, whose microseconds it drops): occurrences generated period by period in
  order, from dtstart up to until. Weekly rules, bysetpos, byweekday, byyearday, byweekno, byeaster, counts
  and time zones other than UTC are not included.
