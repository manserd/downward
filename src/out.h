#ifndef OUT_H
#define OUT_H

#include "search/utils/logging.h"

inline auto log_ = utils::get_log_for_verbosity(utils::Verbosity::DEBUG);
#define outl(...) do { log_ << OUT_STYLE OUT_PREFIX << __VA_ARGS__ << "\x1b[0m" << std::endl; } while (false)
// #define outl(...) do {} while (false)

#endif
