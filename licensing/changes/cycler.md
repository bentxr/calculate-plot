# Changes to cycler code (BSD-3-Clause: redistribution keeps the notice)

src/rc/cycler.hpp and src/rc/cycler.cpp contain, translated to C++ from cycler/__init__.py (cycler 0.12.1):
- Cycler's sum (`__add__`: two cycles of one length, entry by entry), product (`__mul__` with a Cycler:
  every entry of the second for each entry of the first; with an integer: the cycle that many times over),
  slices (`__getitem__`) and `concat`, with the checks of `_process_keys` (no property on both sides) and the
  conditions under which each of them raises;
- the cycle is held as `Cycler.by_key()` gives it (the list of values of each property) instead of as a tree
  of operands that is iterated when needed, and the properties are kept in alphabetical order;
- where the package fails with a StopIteration on an empty cycle (it looks at the first entry of each
  side), the reason given here is "an empty cycle cannot be composed"; the other messages are the package's.

Not translated: iteration itself, `simplify`, `change_key`, the in-place operators and the representations.
