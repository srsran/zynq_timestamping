/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srsran/phy/common/phy_common.h"

#ifdef FORCE_STANDARD_RATE
static bool use_standard_rates = true;
#else
static bool use_standard_rates = false;
#endif

bool srsran_nofprb_isvalid(uint32_t nof_prb)
{
  if (nof_prb == 1 || (nof_prb >= 6 && nof_prb <= SRSRAN_MAX_PRB)) {
    return true;
  } else {
    return false;
  }
}

void srsran_use_standard_symbol_size(bool enabled)
{
  use_standard_rates = enabled;
}

bool srsran_symbol_size_is_standard()
{
  return use_standard_rates;
}

int srsran_sampling_freq_hz(uint32_t nof_prb)
{
  int n = srsran_symbol_sz(nof_prb);
  if (n == -1) {
    return SRSRAN_ERROR;
  } else {
    return 15000 * n;
  }
}

int srsran_symbol_sz_power2(uint32_t nof_prb)
{
  if (nof_prb <= 6) {
    return 128;
  } else if (nof_prb <= 15) {
    return 256;
  } else if (nof_prb <= 25) {
    return 512;
  } else if (nof_prb <= 52) {
    return 1024;
  } else if (nof_prb <= 79) {
    return 1536;
  } else if (nof_prb <= 110) {
    return 2048;
  } else {
    return -1;
  }
}

int srsran_symbol_sz(uint32_t nof_prb)
{
  if (nof_prb <= 0) {
    return SRSRAN_ERROR;
  }
  if (!use_standard_rates) {
    if (nof_prb <= 6) {
      return 128;
    } else if (nof_prb <= 15) {
      return 256;
    } else if (nof_prb <= 25) {
      return 384;
    } else if (nof_prb <= 52) {
      return 768;
    } else if (nof_prb <= 79) {
      return 1024;
    } else if (nof_prb <= 110) {
      return 1536;
    } else {
      return SRSRAN_ERROR;
    }
  } else {
    return srsran_symbol_sz_power2(nof_prb);
  }
}

bool srsran_symbol_sz_isvalid(uint32_t symbol_sz)
{
  if (!use_standard_rates) {
    if (symbol_sz == 128 || symbol_sz == 256 || symbol_sz == 384 || symbol_sz == 768 || symbol_sz == 1024 ||
        symbol_sz == 1536) {
      return true;
    } else {
      return false;
    }
  } else {
    if (symbol_sz == 128 || symbol_sz == 256 || symbol_sz == 512 || symbol_sz == 1024 || symbol_sz == 1536 ||
        symbol_sz == 2048) {
      return true;
    } else {
      return false;
    }
  }
}