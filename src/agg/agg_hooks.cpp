#include "agg/agg_hooks.h"

namespace calculate_plot::agg_hooks {

bool& cellBlockLimitExceeded() {
    thread_local bool exceeded = false;
    return exceeded;
}

}  // namespace calculate_plot::agg_hooks
