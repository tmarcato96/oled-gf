#pragma once

#define MAKE_SLICE_PREFIX(prefix) #prefix "_perp", #prefix "_para_p", #prefix "_para_s"

#define MAKE_SLICE_SUFFIX(prefix, suffix) \
  #prefix "_perp_" #suffix, #prefix "_para_p_" #suffix, #prefix "_para_s_" #suffix

#define GET_MACRO(_1, _2, NAME, ...) NAME
#define MAKE_SLICE(...) GET_MACRO(__VA_ARGS__, MAKE_SLICE_SUFFIX, MAKE_SLICE_PREFIX)(__VA_ARGS__)

#define POWER_DIPOLES MAKE_SLICE(P, u), MAKE_SLICE(P, uf)
#define POWER_DIPOLES_U MAKE_SLICE(P, u)
#define POWER_DIPOLES_U_FRAC MAKE_SLICE(P, uf)
#define POWER_DIPOLES_SUB MAKE_SLICE(P, sub)