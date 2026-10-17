#pragma once

#include <locale.h>

namespace calculate_plot::support {

// The "C" locale on the calling thread while a public call runs: figure files, style sheets, SVG and Python have
// numbers with a decimal point whatever locale the application set (a Qt application sets the environment's, which
// may have a decimal comma), as Python reads and writes them. The thread's own locale comes back at the end.
class CLocale {
public:
    CLocale() : previous_(uselocale(c())) {}
    ~CLocale() { uselocale(previous_); }
    CLocale(const CLocale&) = delete;
    CLocale& operator=(const CLocale&) = delete;

private:
    static locale_t c() {
        static const locale_t locale = newlocale(LC_ALL_MASK, "C", locale_t(0));
        return locale;
    }
    locale_t previous_;
};

}  // namespace calculate_plot::support
