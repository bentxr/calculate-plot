# Changes to kiwisolver 1.5.1

The files in extern/kiwi/kiwi are the C++ headers of kiwisolver 1.5.1 (kiwi/ in its repository), the
constraint solver Matplotlib's constrained layout uses through kiwisolver's Python module. calculate-plot
copies them unmodified, except:

- kiwi/variable.h: variables are ordered by the order they were made in (a counter kept in each
  VariableData), where kiwisolver compares the addresses of their data. A constraint's terms are sorted by
  that order (Constraint's reduce, and the solver's variable map), so with addresses the order changed with
  the memory allocator from run to run and from target to target; Matplotlib's layouts come out the same
  whatever that order is (checked by drawing the same figures with different allocation patterns).
- kiwi/solverimpl.h: each `throw error` is `KIWI_RAISE(error)` followed by a return from the function;
  KIWI_RAISE is `throw error` unless it is defined before the header is included. calculate-plot is built
  without C++ exceptions and defines it to note the error (src/engine/constrained_layout.cpp), which then
  stops the layout with kiwisolver's message.
- Only the C++ headers are copied; the Python module (py/), the benchmarks, the documentation and the tests
  are not.
