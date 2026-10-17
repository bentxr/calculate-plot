# Changes to FDLIBM code (Sun Microsystems' notice: use, copying, modification and distribution are
# freely granted provided the notice is preserved; it is, in the file and in LICENSES/fdlibm.txt)

src/support/log10.cpp is e_log10.c of FDLIBM 5.3 (https://www.netlib.org/fdlibm/e_log10.c), with these changes:
- it is compiled as C++, in the namespace calculate_plot::support, and the function is named log10
  (it was __ieee754_log10); the K&R alternative of the declarations is removed;
- the include of fdlibm.h is replaced: the words of the double are read and written through its bytes
  (std::memcpy) instead of the __HI and __LO macros, so that no aliasing rule is broken and the byte order
  does not matter; the integers are std::int32_t and std::uint32_t instead of int and unsigned;
- the natural logarithm is the C library's (std::log) instead of __ieee754_log;
- the zero that the special cases divide by is a volatile local instead of a static variable.
The method, the constants and the order of the operations are unchanged; the comments are the original ones.
