# Changes to Unicode data (Unicode License V3: the data may be used, copied, modified and distributed
# provided the copyright and permission notice goes with it; it does, in LICENSES/Unicode-3.0.txt)

data/unicode_letters.json is derived from the Unicode Character Database 16.0.0, read through the
unicodedata module of the reference Python by tools/oracle/make_unicode_letters.py:
- only three properties are kept, each as a list of inclusive code point ranges: the characters of the
  Basic Multilingual Plane whose general category begins with L ("letters"), those whose name begins
  with GREEK CAPITAL ("greek_capital"), and the characters of every plane that Python's str.isdigit
  accepts, that is Numeric_Type Digit or Decimal ("digits");
- everything else of the database (names, the other categories and properties) is left out;
- the format is JSON instead of the database's text files.
