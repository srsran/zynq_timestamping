/**
*
* \section COPYRIGHT
*
* Copyright 2013-2022 Software Radio Systems Limited
*
* By using this file, you agree to the terms and conditions set
* forth in the LICENSE file which can be found at the top level of
* the distribution.
*
*/

#ifndef SRSRAN_RF_XLNX_RFDC_IMP_H
#define SRSRAN_RF_XLNX_RFDC_IMP_H

#include <stdbool.h>
#include <stdint.h>

#include "srsran/config.h"
#include "srsran/phy/rf/rf.h"

extern rf_dev_t srsran_rf_dev_rfdc;

SRSRAN_API const char* rf_xrfdc_devname(void *h);
SRSRAN_API int    rf_xrfdc_start_rx_stream(void *h, bool now);
SRSRAN_API int    rf_xrfdc_stop_rx_stream(void *h);
SRSRAN_API int    rf_xrfdc_start_tx_stream(void *h);
SRSRAN_API void   rf_xrfdc_suppress_stdout(void* h);
SRSRAN_API void   rf_xrfdc_register_error_handler(void *h, srsran_rf_error_handler_t new_handler, void* arg);
SRSRAN_API int    rf_xrfdc_open(char *args, void **h);
SRSRAN_API int    rf_xrfdc_open_multi(char *args, void **h, uint32_t nof_channels);
SRSRAN_API int    rf_xrfdc_close(void *h);
SRSRAN_API double rf_xrfdc_set_rx_srate(void *h, double freq);
SRSRAN_API int    rf_xrfdc_set_rx_gain(void *h, double gain);
SRSRAN_API int    rf_xrfdc_set_tx_gain(void *h, double gain);
SRSRAN_API double rf_xrfdc_get_rx_gain(void *h);
SRSRAN_API double rf_xrfdc_get_tx_gain(void *h);
SRSRAN_API double rf_xrfdc_set_rx_freq(void* h, uint32_t ch, double freq);
SRSRAN_API double rf_xrfdc_set_tx_srate(void *h, double freq);
SRSRAN_API double rf_xrfdc_set_tx_freq(void* h, uint32_t ch, double freq);
SRSRAN_API bool   rf_xrfdc_has_rssi(void *h);
SRSRAN_API float  rf_xrfdc_get_rssi(void *h);

srsran_rf_info_t *rf_xrfdc_get_info(void *h);

int  rf_xrfdc_recv_with_time(void*    h,
                             void*    data,
                             uint32_t nsamples,
                             bool     blocking,
                             time_t*  secs,
                             double*  frac_secs);

int rf_xrfdc_recv_with_time_multi(void*            h,
                                  void**           data,
                                  uint32_t         nsamples,
                                  bool             blocking,
                                  time_t*          secs,
                                  double*          frac_secs);

int rf_xrfdc_send_timed(void*              h,
                        void*              data,
                        int                nsamples,
                        time_t             secs,
                        double             frac_secs,
                        bool               has_time_spec,
                        bool               blocking,
                        bool               is_start_of_burst,
                        bool               is_end_of_burst);

int rf_xrfdc_send_timed_multi(void*              h,
                              void**             data,
                              int                nsamples,
                              time_t             secs,
                              double             frac_secs,
                              bool               has_time_spec,
                              bool               blocking,
                              bool               is_start_of_burst,
                              bool               is_end_of_burst);

#endif //SRSRAN_RF_XLNX_RFDC_IMP_H
