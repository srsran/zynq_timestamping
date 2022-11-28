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

#include "srsran/phy/rf/rf.h"
#include <stdbool.h>

/* Define implementation for IIO */
#ifdef ENABLE_IIO
static srsran_rf_plugin_t plugin_iio = {"libsrsran_rf_iio.so", NULL, NULL};
#endif

/* Define implementation for RFdc */
#ifdef ENABLE_RFDC
static srsran_rf_plugin_t plugin_rfdc = {"libsrsran_rf_rfdc.so", NULL, NULL};
#endif

/**
 * Collection of all currently available RF plugins
 */
static srsran_rf_plugin_t* rf_plugins[] = {
#ifdef ENABLE_IIO
    &plugin_iio,
#endif
#ifdef ENABLE_RFDC
    &plugin_rfdc,
#endif
    NULL};