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

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "rf_helper.h"
#include "rf_iio_imp.h"
#include "rf_plugin.h"
#include "srsran/srsran.h"
#include <ad9361.h>
#include <fcntl.h>
#include <iio.h>
#include <sys/mman.h>

#define common_preamble1 0xbbbbaaaa
#define common_preamble2 0xddddcccc
#define common_preamble3 0xffffeeee
#define time_preamble1   0xabcddcba
#define time_preamble2   0xfedccdef
#define time_preamble3   0xdfcbaefd

#define IIO_MIN_DATA_BUFFER_SIZE 1920
#define METADATA_NSAMPLES        8
#define CONVERT_BUFFER_SIZE      1048576
#define PKT_HEADER_MAGIC         0x12345678
#define DEVNAME_IIO              "iio"
//#define PRINT_TIMESTAMPS         1

cf_t zero_mem[64 * 1024] = {0};
int  rx_data_buffer_size = IIO_MIN_DATA_BUFFER_SIZE;
int  tx_data_buffer_size = IIO_MIN_DATA_BUFFER_SIZE;
int  lates               = 0;
int  firstGo             = 0;

typedef struct {
  uint64_t magic;
  uint64_t timestamp;
  uint32_t nof_samples;
  bool     end_of_burst;
} tx_header_t;

typedef struct {
  long long           _bw_hz; // Analog banwidth in Hz
  long long           _fs_hz; // Baseband sample rate in Hz
  int16_t             _conv_buffer[CONVERT_BUFFER_SIZE];
  ssize_t             _buf_count;
  long                buffer_size;
  int                 byte_offset;
  bool                stream_active;
  pthread_mutex_t     stream_mutex;
  pthread_cond_t      stream_cvar;
  tx_header_t         prev_header;
  srsran_ringbuffer_t ring_buffer;
  struct iio_device*  _device;
  struct iio_channel* _channel;
  struct iio_buffer*  _buf;
  int                 items_in_buffer;
  pthread_t           thread;
  bool                thread_completed;
  uint64_t            current_tstamp;
  float               secs;
  float               frac_secs;
  int                 metadata_samples;
  int                 preamble_location;
} rf_iio_streamer;

typedef struct {
  struct iio_device*        dev;
  struct iio_context*       ctx;
  bool                      use_timestamps;
  rf_iio_streamer           tx_streamer;
  rf_iio_streamer           rx_streamer;
  srsran_rf_error_handler_t iio_error_handler;
  void*                     iio_error_handler_arg;
  volatile unsigned int*    memory_map_ptr;
  srsran_rf_info_t          info;
} rf_iio_handler_t;

static char tmpstr[64];

/* helper function generating channel names */
static char* get_ch_name(const char* type, int id)
{
  snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
  return tmpstr;
}

int refill_buffer(rf_iio_streamer* streamer, ssize_t* items_in_buffer, int* byte_offset)
{
  ssize_t nbytes_rx = iio_buffer_refill(streamer->_buf);
  if (nbytes_rx < 0) {
    return nbytes_rx;
  }
  *items_in_buffer = (unsigned long)nbytes_rx / iio_buffer_step(streamer->_buf);
  *byte_offset     = 0;
  return nbytes_rx;
}

/*memory map interface functions*/
int open_mem_register(void* h)
{
  rf_iio_handler_t* handler  = (rf_iio_handler_t*)h;
  unsigned int      reg_size = 0x1000;
  off_t             reg_addr = 0x0050000000;
  int               mm_reg_d;
  // Map the MM-reg address into user space getting a virtual address for it
  if ((mm_reg_d = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
    fprintf(stderr, "Error accessing the memory-maped register\n");
    return SRSRAN_ERROR;
  } else {
    handler->memory_map_ptr = (uint32_t*)mmap(NULL, reg_size, PROT_READ | PROT_WRITE, MAP_SHARED, mm_reg_d, reg_addr);
  }
  return SRSRAN_SUCCESS;
}

void check_late_register(void* h, uint32_t* late_reg_value)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  if (handler->memory_map_ptr) {
    // BA + 0x10
    *late_reg_value = handler->memory_map_ptr[4];
  } else {
    *late_reg_value = 0;
  }
}

static void* reader_thread(void* arg);
static void* writer_thread(void* arg);

static void log_overflow(rf_iio_handler_t* h)
{
  if (h->iio_error_handler) {
    srsran_rf_error_t error;
    bzero(&error, sizeof(srsran_rf_error_t));
    error.type = SRSRAN_RF_ERROR_OVERFLOW;
    h->iio_error_handler(h->iio_error_handler_arg, error);
  }
}

static void log_late(rf_iio_handler_t* h, bool is_rx)
{
  if (h->iio_error_handler) {
    srsran_rf_error_t error;
    bzero(&error, sizeof(srsran_rf_error_t));
    error.opt  = is_rx ? 1 : 0;
    error.type = SRSRAN_RF_ERROR_LATE;
    h->iio_error_handler(h->iio_error_handler_arg, error);
  }
}

void rf_iio_suppress_stdout(void* h)
{
  // not supported
}

void rf_iio_register_error_handler(void* h, srsran_rf_error_handler_t new_handler, void* arg)
{
  rf_iio_handler_t* handler      = (rf_iio_handler_t*)h;
  handler->iio_error_handler     = new_handler;
  handler->iio_error_handler_arg = arg;
}

const char* rf_iio_devname(void* h)
{
  return DEVNAME_IIO;
}

int rf_iio_start_rx_stream(void* h, bool now)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;

  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  handler->rx_streamer.items_in_buffer = 0;
  handler->rx_streamer.stream_active   = true;

  if (handler->rx_streamer.thread_completed) {
    // if rx thread was stopped before - restart it
    // srsran_ringbuffer_reset(&handler->rx_streamer.ring_buffer);
    srsran_ringbuffer_start(&handler->rx_streamer.ring_buffer);
    pthread_create(&handler->rx_streamer.thread, NULL, reader_thread, handler);
  }
  pthread_cond_signal(&handler->rx_streamer.stream_cvar);
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);

  // make sure thread has been started
  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  while (handler->rx_streamer.thread_completed) {
    pthread_cond_wait(&handler->rx_streamer.stream_cvar, &handler->rx_streamer.stream_mutex);
  }
  // INFO("RF_IIO: RX stream started");
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);
  return SRSRAN_SUCCESS;
}

static void stop_rx_stream(rf_iio_handler_t* handler)
{
  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  handler->rx_streamer.stream_active = false;

  if (handler->rx_streamer._buf) {
    iio_buffer_cancel(handler->rx_streamer._buf);
  }
  while (!handler->rx_streamer.thread_completed) {
    pthread_cond_wait(&handler->rx_streamer.stream_cvar, &handler->rx_streamer.stream_mutex);
  }
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);
  pthread_join(handler->rx_streamer.thread, NULL);

  if (handler->rx_streamer._buf) {
    iio_buffer_destroy(handler->rx_streamer._buf);
    handler->rx_streamer._buf = NULL;
  }
}

int rf_iio_stop_rx_stream(void* h)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  if (!handler->rx_streamer.thread_completed) {
    stop_rx_stream(handler);
    srsran_ringbuffer_stop(&handler->rx_streamer.ring_buffer);
    INFO("RF_IIO: RX stream stopped\n");
  }
  return 0;
}

uint64_t time_to_tstamp_iio(rf_iio_handler_t* handler, time_t secs, double frac_secs)
{
  return (uint64_t)(handler->tx_streamer._fs_hz * ((double)secs)) +
         (uint64_t)(round((double)handler->tx_streamer._fs_hz * frac_secs));
}

void tstamp_to_time_iio(rf_iio_handler_t* handler, uint64_t tstamp, time_t* secs, double* frac_secs)
{
  uint64_t srate_int = (uint64_t)handler->rx_streamer._fs_hz;
  if (secs && frac_secs) {
    *secs              = tstamp / srate_int;
    uint64_t remainder = tstamp % srate_int;
    *frac_secs         = (double)remainder / srate_int;
  }
}

int rf_iio_start_tx_stream(void* h)
{
  rf_iio_handler_t* handler            = (rf_iio_handler_t*)h;
  handler->tx_streamer.items_in_buffer = 0;
  pthread_mutex_lock(&handler->tx_streamer.stream_mutex);
  handler->tx_streamer.stream_active = true;
  pthread_cond_signal(&handler->tx_streamer.stream_cvar);
  pthread_mutex_unlock(&handler->tx_streamer.stream_mutex);
  return SRSRAN_SUCCESS;
}

int rf_iio_stop_tx_stream(void* h)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  pthread_mutex_lock(&handler->tx_streamer.stream_mutex);
  handler->tx_streamer.stream_active = false;
  pthread_mutex_unlock(&handler->tx_streamer.stream_mutex);

  pthread_join(handler->tx_streamer.thread, NULL);

  if (handler->tx_streamer._buf) {
    iio_buffer_cancel(handler->tx_streamer._buf);
  }

  if (handler->tx_streamer._buf) {
    iio_buffer_destroy(handler->tx_streamer._buf);
    handler->tx_streamer._buf = NULL;
  }
  return SRSRAN_SUCCESS;
}

void rf_iio_flush_buffer(void* h)
{
  // noop
}

bool rf_iio_has_rssi(void* h)
{
  // noop
  return false;
}

float rf_iio_get_rssi(void* h)
{
  return 0.0;
}

void rf_iio_set_master_clock_rate(void* h, double rate)
{
  // noop
}

bool rf_iio_is_master_clock_dynamic(void* h)
{
  return false;
}

double rf_iio_set_rx_srate(void* h, double rate)
{
  rf_iio_handler_t* handler              = (rf_iio_handler_t*)h;
  bool              stream_needs_restart = false;

  if (rate == (double)handler->rx_streamer._fs_hz) {
    return rate;
  }

  if (handler->rx_streamer.stream_active) {
    stream_needs_restart = true;
    // stop receiving samples while reconfiguring RF frontend
    stop_rx_stream(handler);
    // clear ringbuffers and invalidate any partially read data packet
    srsran_ringbuffer_stop(&handler->rx_streamer.ring_buffer);
    srsran_ringbuffer_reset(&handler->rx_streamer.ring_buffer);
    handler->rx_streamer.prev_header.nof_samples = 0;
    srsran_ringbuffer_start(&handler->rx_streamer.ring_buffer);
  }
  INFO("RF_IIO: changing srate, RX stream paused\n");

  long long samplerate        = (long long)rate;
  handler->rx_streamer._fs_hz = rate;
  handler->tx_streamer._fs_hz = rate;

  bool decimation = false;
  if (samplerate < (25e6 / 48)) {
    if (samplerate * 8 < (25e6 / 48)) {
      printf("sample rate %f is not supported.\n", rate);
    }
    decimation = true;
    samplerate = samplerate * 8;
  }

  if (iio_channel_attr_write_longlong(
          iio_device_find_channel(handler->dev, "voltage0", false), "sampling_frequency", samplerate) < 0) {
    INFO("RF_IIO: error writing ad9361 \"sampling frequency\" attribute  \n");
  }
  if (iio_channel_attr_write_longlong(iio_device_find_channel(handler->rx_streamer._device, "voltage0", false),
                                      "sampling_frequency",
                                      decimation ? samplerate / 8 : samplerate) < 0) {
    INFO("RF_IIO: error writing cf-ad9361-lpc \"sampling frequency\" attribute  \n");
  }
  if (iio_channel_attr_write_longlong(iio_device_find_channel(handler->tx_streamer._device, "voltage0", true),
                                      "sampling_frequency",
                                      decimation ? samplerate / 8 : samplerate) < 0) {
    INFO("RF_IIO: error writing cf-ad9361-dds-core-lpc \"sampling frequency\" attribute  \n");
  }

#ifdef HAS_AD9361_IIO
  if (ad9361_set_bb_rate(handler->dev, samplerate)) {
    INFO("RF_IIO: Unable to set BB rate");
  }
#endif
  if (stream_needs_restart) {
    // restart the RX stream
    rf_iio_start_rx_stream(handler, true);
  }
  return rate;
}

double rf_iio_set_tx_srate(void* h, double rate)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  if (rate != (double)handler->rx_streamer._fs_hz) {
    INFO("RF_IIO: changing TX sample rate ...\n");
    rf_iio_set_rx_srate(h, rate);
  }
  INFO("RF_IIO: TX sample rate is configured\n");
  return rate;
}

int rf_iio_set_rx_gain(void* h, double gain)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  long long         gain1   = (long long)gain;
  iio_channel_attr_write_longlong(iio_device_find_channel(handler->dev, "voltage0", false), "hardwaregain", gain1);
  return SRSRAN_SUCCESS;
}

int rf_iio_set_tx_gain(void* h, double gain)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  long long         gain1   = (long long)gain;
  gain1                     = gain1 - 89;
  iio_channel_attr_write_longlong(iio_device_find_channel(handler->dev, "voltage0", true), "hardwaregain", gain1);

  return SRSRAN_SUCCESS;
}

double rf_iio_get_rx_gain(void* h)
{
  long long         gain;
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  gain                      = 0;
  if (iio_channel_attr_read_longlong(iio_device_find_channel(handler->dev, "voltage0", false), "hardwaregain", &gain) !=
      0) {
    return 0;
  }
  return (double)gain;
}

double rf_iio_get_tx_gain(void* h)
{
  long long         gain;
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  gain                      = 0;
  if (iio_channel_attr_read_longlong(iio_device_find_channel(handler->dev, "voltage0", true), "hardwaregain", &gain) !=
      0) {
    return 0;
  }
  gain = gain + 89;
  return (double)gain;
}

srsran_rf_info_t* rf_iio_get_info(void* h)
{
  srsran_rf_info_t* info = NULL;
  if (h != NULL) {
    rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
    info                      = &handler->info;
  }
  return info;
}

size_t rf_iio_set_rx_buffer_size(void* h, size_t buffer_size)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  if (buffer_size != handler->rx_streamer.buffer_size) {
    if (handler->rx_streamer._buf) {
      iio_buffer_destroy(handler->rx_streamer._buf);
    }
    handler->rx_streamer.buffer_size = buffer_size;
    handler->rx_streamer._buf        = iio_device_create_buffer(
        handler->rx_streamer._device, handler->rx_streamer.buffer_size + handler->rx_streamer.metadata_samples, false);
  }
  printf("(TODO)set Rx buffer size to %d\n", (int)buffer_size);
  return buffer_size;
}

double rf_iio_set_rx_freq(void* h, uint32_t ch, double frequency)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  long long         freq    = (long long)frequency;
  iio_channel_attr_write_longlong(iio_device_find_channel(handler->dev, "altvoltage0", true), "frequency", freq);
  return frequency;
}

double rf_iio_set_tx_freq(void* h, uint32_t ch, double frequency)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  long long         freq    = (long long)frequency;
  iio_channel_attr_write_longlong(iio_device_find_channel(handler->dev, "altvoltage1", true), "frequency", freq);
  return frequency;
}

void rf_iio_get_time(void* h, time_t* secs, double* frac_secs)
{
  // noop
}

static void rf_iio_use_timestamping(void* h, int nof_prbs)
{
  bool              skip_rx_buf_reconfig = false;
  bool              skip_tx_buf_reconfig = false;
  rf_iio_handler_t* handler              = (rf_iio_handler_t*)h;
  handler->use_timestamps                = true;

  handler->rx_streamer.metadata_samples = (handler->use_timestamps) ? (METADATA_NSAMPLES) : 0;
  handler->tx_streamer.metadata_samples = (handler->use_timestamps) ? (METADATA_NSAMPLES) : 0;

  if (nof_prbs <= 6) {
    rx_data_buffer_size = IIO_MIN_DATA_BUFFER_SIZE;
  } else if (nof_prbs > 6 && nof_prbs <= 15) {
    rx_data_buffer_size = IIO_MIN_DATA_BUFFER_SIZE * 2;
  } else {
    // 25 prbs and higher
    rx_data_buffer_size = 7680;
  }
  tx_data_buffer_size = rx_data_buffer_size;

  long total_tx_buffer_size = tx_data_buffer_size + handler->tx_streamer.metadata_samples;

  if (handler->rx_streamer.buffer_size == rx_data_buffer_size) {
    INFO("RF_IIO: RX IIO buffer size is the same as the one being configured.");
    skip_rx_buf_reconfig = true;
  }
  if (handler->tx_streamer.buffer_size == tx_data_buffer_size) {
    INFO("RF_IIO: TX IIO buffer size is the same as the one being configured.");
    skip_tx_buf_reconfig = true;
  }

  bool need_rx_stream_restart = false;
  bool need_tx_stream_restart = false;
  if (!skip_rx_buf_reconfig) {
    pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
    need_rx_stream_restart = handler->rx_streamer.stream_active;
    pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);
  }
  if (!skip_tx_buf_reconfig) {
    pthread_mutex_lock(&handler->tx_streamer.stream_mutex);
    need_tx_stream_restart = handler->tx_streamer.stream_active;
    pthread_mutex_unlock(&handler->tx_streamer.stream_mutex);
  }

  if (need_rx_stream_restart) {
    stop_rx_stream(handler);
    DEBUG("RF_IIO: changing IIO buffer size, RX stream paused");
    // invalidate any partially read data packet
    handler->rx_streamer.prev_header.nof_samples = 0;
  }
  if (need_tx_stream_restart) {
    rf_iio_stop_tx_stream(handler);
    DEBUG("RF_IIO: changing IIO buffer size, TX stream paused");
    // invalidate any partially read data packet
    handler->tx_streamer.prev_header.nof_samples = 0;
  }

  handler->rx_streamer.buffer_size = rx_data_buffer_size;
  handler->tx_streamer.buffer_size = tx_data_buffer_size;

  if (!skip_tx_buf_reconfig) {
    if (handler->tx_streamer._buf) {
      iio_buffer_cancel(handler->tx_streamer._buf);
      iio_buffer_destroy(handler->tx_streamer._buf);
    }
    handler->tx_streamer._buf = iio_device_create_buffer(handler->tx_streamer._device, total_tx_buffer_size, false);
    if (!handler->tx_streamer._buf) {
      ERROR("Could not create TX buffer");
    }
  }
  if (need_rx_stream_restart) {
    rf_iio_start_rx_stream(handler, true);
  }
  if (need_tx_stream_restart) {
    rf_iio_start_tx_stream(handler);
  }
}

int rf_iio_open_multi(char* args, void** h, uint32_t nof_rx_antennas)
{
  *h = NULL;

  rf_iio_handler_t* handler = (rf_iio_handler_t*)malloc(sizeof(rf_iio_handler_t));
  if (!handler) {
    perror("malloc");
    return -1;
  }
  *h = handler;

  /// handle rf args
  uint32_t n_prb = 0;
  if (!parse_uint32(args, "n_prb", 0, &n_prb)) {
    // set to 6PRBs if not provided by the user
    n_prb = 6;
  }

  char ctx_addr[RF_PARAM_LEN] = "default";
  bool is_lowspeed_context    = false;
  parse_string(args, "context", 0, ctx_addr);
  if (strcmp(ctx_addr, "default") == 0) {
    handler->ctx = iio_create_default_context();
  } else {
    handler->ctx        = iio_create_context_from_uri(ctx_addr);
    is_lowspeed_context = true;
  }
  if (!handler->ctx) {
    fprintf(stderr, "failed to create iio device context\n");
    return -1;
  }
  if (iio_context_get_devices_count(handler->ctx) <= 0) {
    fprintf(stderr, "Could not find iio devices in context\n");
    goto out_error;
  }

  // Acquire PHY device descriptor
  handler->dev = iio_context_find_device(handler->ctx, "ad9361-phy");
  if (!handler->dev) {
    fprintf(stderr, "No ad9361-phy found\n");
    goto out_error;
  }

  // Acquire rx- and tx-streamer device descriptors
  handler->rx_streamer._device = iio_context_find_device(handler->ctx, "cf-ad9361-lpc");
  if (!handler->rx_streamer._device) {
    fprintf(stderr, "could not find iio rx device\n");
    goto out_error;
  }
  handler->tx_streamer._device = iio_context_find_device(handler->ctx, "cf-ad9361-dds-core-lpc");
  if (!handler->tx_streamer._device) {
    fprintf(stderr, "could not find iio tx device\n");
    goto out_error;
  }

  // Get pointers to PHY device channels responsible for RF parameters configuration
  handler->rx_streamer._channel = iio_device_find_channel(handler->dev, get_ch_name("voltage", 0), false);
  if (!handler->rx_streamer._channel) {
    fprintf(stderr, "could not set rx phy channel\n");
    goto out_error;
  }
  handler->tx_streamer._channel = iio_device_find_channel(handler->dev, get_ch_name("voltage", 0), true);
  if (!handler->rx_streamer._channel) {
    fprintf(stderr, "could not set tx phy channel\n");
    goto out_error;
  }

  if (iio_channel_attr_write(handler->rx_streamer._channel, "rf_port_select", "A_BALANCED")) {
    fprintf(stderr, "failed to create the rf_port with A_BALENCED\n");
  }

  if (iio_channel_attr_write(handler->tx_streamer._channel, "rf_port_select", "A")) {
    fprintf(stderr, "failed to create the rf_port with A\n");
  }

  // Find and enable streaming channels
  struct iio_channel* tmp_chn[4];
  for (int ii = 0; ii < 4; ++ii) {
    struct iio_device* device;
    device      = (ii < 2) ? handler->rx_streamer._device : handler->tx_streamer._device;
    tmp_chn[ii] = iio_device_find_channel(device, get_ch_name("voltage", ii % 2), (ii < 2) ? false : true);
    if (!tmp_chn[ii]) {
      tmp_chn[ii] = iio_device_find_channel(device, get_ch_name("altvoltage", ii % 2), (ii < 2) ? false : true);
    }
    iio_channel_enable(tmp_chn[ii]);
  }

  if (is_lowspeed_context) {
    // if USB/Network context is being created, increase number of allocated IIO buffers
    iio_device_set_kernel_buffers_count(handler->rx_streamer._device, 32);
  }

  // in fully embedded setup, we can access registers storing some rx/tx statistics
  if (!is_lowspeed_context) {
    if (open_mem_register(handler) < SRSRAN_SUCCESS) {
      goto out_error;
    }
  } else {
    handler->memory_map_ptr = NULL;
  }

  // get the sampling rate being used by the device
  iio_channel_attr_read_longlong(iio_device_find_channel(handler->rx_streamer._device, "voltage0", false),
                                 "sampling_frequency",
                                 &handler->rx_streamer._fs_hz);
  handler->tx_streamer._fs_hz = handler->rx_streamer._fs_hz;

  pthread_mutex_init(&handler->rx_streamer.stream_mutex, NULL);
  pthread_cond_init(&handler->rx_streamer.stream_cvar, NULL);
  srsran_ringbuffer_init(&handler->rx_streamer.ring_buffer, 1500 * 1920);
  handler->rx_streamer.thread_completed = false;
  pthread_create(&handler->rx_streamer.thread, NULL, reader_thread, handler);

  pthread_mutex_init(&handler->tx_streamer.stream_mutex, NULL);
  pthread_cond_init(&handler->tx_streamer.stream_cvar, NULL);
  srsran_ringbuffer_init(&handler->tx_streamer.ring_buffer, 200 * 1920);
  handler->tx_streamer.thread_completed = false;
  pthread_create(&handler->tx_streamer.thread, NULL, writer_thread, handler);

  handler->rx_streamer._buf_count = 0;
  handler->tx_streamer._buf_count = 0;

  handler->rx_streamer.preamble_location = 0;
  handler->tx_streamer.preamble_location = 0;

  rf_iio_use_timestamping(handler, n_prb);

  return 0;

out_error:
  iio_context_destroy(handler->ctx);
  return -1;
}

static bool buffer_initialized(rf_iio_streamer* streamer)
{
  return (streamer->_buf != NULL);
}

int rf_iio_open(char* args, void** h)
{
  return rf_iio_open_multi(args, h, 1);
}

int rf_iio_close(void* h)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;

  if (handler->tx_streamer.thread && !handler->tx_streamer.thread_completed) {
    pthread_cancel(handler->tx_streamer.thread);
  }
  if (handler->rx_streamer.thread && !handler->rx_streamer.thread_completed) {
    pthread_cancel(handler->rx_streamer.thread);
  }
  // print statistics
  // if (handler->num_lates) printf("#lates=%d\n", handler->num_lates);
  // if (handler->num_overflows) printf("#overflows=%d\n", handler->num_overflows);
  // if (handler->num_underflows) printf("#underflows=%d\n", handler->num_underflows);
  // if (handler->num_time_errors) printf("#time_errors=%d\n", handler->num_time_errors);
  // if (handler->num_other_errors) printf("#other_errors=%d\n", handler->num_other_errors);
  // iio_context_destroy(handler->ctx);

  return SRSRAN_SUCCESS;
}

void check_overflow(void* h)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  if (handler->memory_map_ptr) {
    uint32_t val = handler->memory_map_ptr[2];
    if (val) {
      INFO("[IIO] Overflow detected");
      log_overflow(handler);
    }
  }
}

enum { COMMON = 0, TIME_DOMAIN = 1, TIMESTAMP = 2 };

int preamble_fsm(void* h, uint32_t* input)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;
  int               state   = COMMON;
  for (int i = 0; i < METADATA_NSAMPLES; i++) {
    switch (state) {
      case COMMON:
        if (input[0] == common_preamble1 && input[1] == common_preamble2 && input[2] == common_preamble3) {
          state = TIME_DOMAIN;
        } else {
          return 0;
        }
        break;
      case TIME_DOMAIN:
        if (input[3] == time_preamble1 && input[4] == time_preamble2 && input[5] == time_preamble3) {
          state = TIMESTAMP;
        } else {
          return 0;
        }
        break;
      case TIMESTAMP:
        handler->rx_streamer.current_tstamp = *((uint64_t*)(&input[6]));
        return 1;
    }
  }
  return 0;
}

static void* reader_thread(void* arg)
{
  rf_iio_handler_t*  handler = (rf_iio_handler_t*)arg;
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  while (!handler->rx_streamer.stream_active) {
    pthread_cond_wait(&handler->rx_streamer.stream_cvar, &handler->rx_streamer.stream_mutex);
  }
  handler->rx_streamer.thread_completed = false;
  pthread_cond_signal(&handler->rx_streamer.stream_cvar);
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);

  if (!buffer_initialized(&handler->rx_streamer)) {
    handler->rx_streamer._buf = iio_device_create_buffer(
        handler->rx_streamer._device, rx_data_buffer_size + handler->rx_streamer.metadata_samples, false);
    if (!handler->rx_streamer._buf) {
      INFO("RF_IIO: Failed to create an IIO buffer\n");
      goto exit;
    }
    iio_buffer_set_blocking_mode(handler->rx_streamer._buf, true);
    srsran_ringbuffer_reset(&handler->rx_streamer.ring_buffer);
  }

  tx_header_t header = {};

  while (handler->rx_streamer.stream_active) {
    int buffer_ret =
        refill_buffer(&handler->rx_streamer, &handler->rx_streamer._buf_count, &handler->rx_streamer.byte_offset);
    if (buffer_ret <= 0) {
      /* If stream is not active, no need to report an error,
       * as we are just cancelling the thread (probably because of changing sample rate, or switching to FPGA
       * processing)
       */
      if (handler->rx_streamer.stream_active) {
        ERROR("Error refilling buf %d\n", (int)buffer_ret);
        usleep(1000);
      }
      continue;
    }
    uintptr_t src_ptr = (uintptr_t)iio_buffer_start(handler->rx_streamer._buf) + handler->rx_streamer.byte_offset;

    header.magic                    = PKT_HEADER_MAGIC;
    handler->rx_streamer._buf_count = handler->rx_streamer._buf_count - handler->tx_streamer.metadata_samples;
    header.nof_samples              = handler->rx_streamer._buf_count;
    uint32_t* start_ptr             = (uint32_t*)src_ptr;

    if (handler->use_timestamps) {
      if (!preamble_fsm(handler, &start_ptr[handler->rx_streamer.preamble_location])) {
        printf("misaligned packet received from the DMA\n");
        // break;
        for (int i = 0; i < (rx_data_buffer_size - (METADATA_NSAMPLES - 1)); i++) {
          if (preamble_fsm(handler, &start_ptr[i])) {
            printf("realigning at index  %d\n", i);
            handler->rx_streamer.preamble_location = i;
          }
        }
      }
      header.timestamp = handler->rx_streamer.current_tstamp;
      // printf("RX timestamp = %lu \n", header.timestamp);
#ifdef PRINT_TIMESTAMPS
      time_t secs;
      double frac_secs;
      tstamp_to_time_iio(handler, header.timestamp, &secs, &frac_secs);

      struct timeval time;
      gettimeofday(&time, NULL);
      if (firstGo < 5) {
        if (frac_secs && secs) {
          printf("rec sec %lu frac %f or %lu ticks  [%4d] [%d] \n",
                 secs,
                 frac_secs,
                 header.timestamp,
                 time.tv_usec,
                 time.tv_sec);
        }
      }
#endif
    }

    check_overflow(handler);
    srsran_ringbuffer_write(&handler->rx_streamer.ring_buffer, &header, sizeof(tx_header_t));

    uint16_t* buf_ptr_tmp = (uint16_t*)src_ptr;
    uint16_t* buf_ptr;

    int ret1 = 0;
    if (handler->rx_streamer.preamble_location == 0) {
      buf_ptr = &buf_ptr_tmp[handler->rx_streamer.metadata_samples * 2];
      ret1    = srsran_ringbuffer_write(
          &handler->rx_streamer.ring_buffer, buf_ptr, 2 * sizeof(uint16_t) * handler->rx_streamer._buf_count);
    } else {
      buf_ptr = &buf_ptr_tmp[0];
      ret1    = srsran_ringbuffer_write(
          &handler->rx_streamer.ring_buffer, buf_ptr, 2 * sizeof(uint16_t) * handler->rx_streamer.preamble_location);

      buf_ptr = &buf_ptr_tmp[(handler->rx_streamer.preamble_location + 8) * 2];

      ret1 += srsran_ringbuffer_write(&handler->rx_streamer.ring_buffer,
                                      buf_ptr, 2 * sizeof(uint16_t) *
                                      (handler->rx_streamer._buf_count - (handler->rx_streamer.preamble_location)));
    }
    if (ret1 < 2 * sizeof(uint16_t) * handler->rx_streamer._buf_count) {
      ERROR("Error writing to buffer in rx thread, ret is %d but should be %d\n",
            ret1, (int)(2 * sizeof(uint16_t) * handler->rx_streamer._buf_count));
    }
  }

exit:
  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  handler->rx_streamer.thread_completed = true;
  pthread_cond_signal(&handler->rx_streamer.stream_cvar);
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);
  return NULL;
}

int rf_iio_recv_with_time_multi(void*    h,
                                void*    data[SRSRAN_MAX_PORTS],
                                uint32_t nsamples,
                                bool     blocking,
                                time_t*  secs,
                                double*  frac_secs)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;

  size_t rxd_samples_total = 0;
  int    trials            = 0;

  cf_t* data_ptr = data[0];
  while (rxd_samples_total < nsamples && trials < 100) {
    if (!handler->rx_streamer.prev_header.nof_samples) {
      if (srsran_ringbuffer_read(
              &handler->rx_streamer.ring_buffer, &handler->rx_streamer.prev_header, sizeof(tx_header_t)) <= 0) {
        INFO("Error reading RX ringbuffer\n");
        return -1;
      }
      if (handler->rx_streamer.prev_header.magic != PKT_HEADER_MAGIC) {
        fprintf(stderr, "Error reading rx ringbuffer. Invalid header\n");
        srsran_ringbuffer_reset(&handler->rx_streamer.ring_buffer);
        return 0;
      }
    }

    uint32_t read_samples = SRSRAN_MIN(handler->rx_streamer.prev_header.nof_samples, nsamples - rxd_samples_total);
    if (srsran_ringbuffer_read(&handler->rx_streamer.ring_buffer,
                               (void*)&handler->rx_streamer._conv_buffer[2 * rxd_samples_total],
                               2 * sizeof(uint16_t) * read_samples) < 0) {
      printf("Error reading buffer\n");
      return -1;
    }
    handler->rx_streamer.prev_header.nof_samples -= read_samples;

    if (read_samples != nsamples) {
      handler->rx_streamer.prev_header.timestamp -= rxd_samples_total;
    }
    rxd_samples_total += read_samples;
    trials++;
  }

  tstamp_to_time_iio(handler, handler->rx_streamer.prev_header.timestamp, secs, frac_secs);
#ifdef PRINT_TIMESTAMPS
  struct timeval time;
  gettimeofday(&time, NULL);
  if (frac_secs && secs) {
    // INFO("receive samples sec %lu frac %f or %lu ticks  [%4d] [%d] \n", *secs, *frac_secs,
    // handler->rx_streamer.prev_header.timestamp, time.tv_usec,time.tv_sec);
    INFO("receive timestamp = %.6lf secs, or %lu ticks",
         (double)*secs + *frac_secs,
         handler->rx_streamer.prev_header.timestamp);
  }
#endif

  srsran_vec_convert_if(&handler->rx_streamer._conv_buffer[0], 32768, (float*)data_ptr, 2 * rxd_samples_total);
  /*printf("receive timestamp = %.6lf secs, or %lu ticks\n", (double)*secs + *frac_secs,
            handler->rx_streamer.prev_header.timestamp);*/
  return (int)nsamples;
}

int rf_iio_recv_with_time(void* h, void* data, uint32_t nsamples, bool blocking, time_t* secs, double* frac_secs)
{
  return rf_iio_recv_with_time_multi(h, &data, nsamples, blocking, secs, frac_secs);
}

int send_buf(void* h)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;

  if (handler->tx_streamer.items_in_buffer < handler->tx_streamer.buffer_size) {
    ptrdiff_t buf_step = iio_buffer_step(handler->tx_streamer._buf);
    uintptr_t buf_ptr =
        (uintptr_t)iio_buffer_start(handler->tx_streamer._buf) + handler->tx_streamer.items_in_buffer * buf_step;
    uintptr_t buf_end = (uintptr_t)iio_buffer_end(handler->tx_streamer._buf);

    memset((void*)buf_ptr, 0, buf_end - buf_ptr);
  }

  ssize_t ret                          = iio_buffer_push(handler->tx_streamer._buf);
  handler->tx_streamer.items_in_buffer = 0;

  if (ret < 0) {
    return ret;
  }
  // uint64_t hw_time = get_current_hw_clock(h);
  // DEBUG("pushed, current fpga time = %lu\n", hw_time);
  return (int)(ret / iio_buffer_step(handler->tx_streamer._buf));
}

static void* writer_thread(void* arg)
{
  rf_iio_handler_t* handler = (rf_iio_handler_t*)arg;

  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
  int      read_samples   = 0;
  uint64_t timestamp      = 0;
  bool     have_timestamp = false;

  pthread_mutex_lock(&handler->tx_streamer.stream_mutex);
  while (!handler->tx_streamer.stream_active) {
    pthread_cond_wait(&handler->tx_streamer.stream_cvar, &handler->tx_streamer.stream_mutex);
  }
  pthread_mutex_unlock(&handler->tx_streamer.stream_mutex);

  while (handler->tx_streamer.stream_active) {
    int n = 0;
    do {
      uintptr_t dst_ptr;
      uint32_t* start_ptr;
      uint64_t* tstamp_ptr;
      start_ptr  = (uint32_t*)iio_buffer_start(handler->tx_streamer._buf);
      tstamp_ptr = (uint64_t*)iio_buffer_start(handler->tx_streamer._buf);
      dst_ptr    = (uintptr_t)iio_buffer_start(handler->tx_streamer._buf) +
                (handler->tx_streamer.metadata_samples + handler->tx_streamer.items_in_buffer) * 2 * sizeof(int16_t);

      if (!handler->tx_streamer.prev_header.nof_samples) {
        if (srsran_ringbuffer_read(
                &handler->tx_streamer.ring_buffer, &handler->tx_streamer.prev_header, sizeof(tx_header_t)) < 0) {
          fprintf(stderr, "Error reading buffer\n");
        }

        if (handler->tx_streamer.prev_header.magic != PKT_HEADER_MAGIC) {
          fprintf(stderr, "Error reading tx ringbuffer. Invalid header\n");
          srsran_ringbuffer_reset(&handler->tx_streamer.ring_buffer);
        }
        if (!have_timestamp) {
          timestamp = handler->tx_streamer.prev_header.timestamp;
          if (timestamp != 0) {
            timestamp -= handler->tx_streamer.items_in_buffer; // for freq-domain packet items_in_buffer is always 0
          }
          have_timestamp = true;
        }
      }

      read_samples = SRSRAN_MIN(handler->tx_streamer.prev_header.nof_samples,
                                (handler->tx_streamer.buffer_size - handler->tx_streamer.items_in_buffer));
      if (read_samples > 0) {
        if (srsran_ringbuffer_read(
                &handler->tx_streamer.ring_buffer, (void*)dst_ptr, 2 * sizeof(uint16_t) * read_samples) < 0) {
          printf("Error reading TX buffer\n");
          return NULL;
        }
      }
      bool end_of_burst = (read_samples == 0);
      // in case of freq-domain data, the length of subframe is a multiple of IIO packets size,
      // thus there is no pending data to be sent when (end_of_burst = true)
      if (!n && end_of_burst) {
        have_timestamp = false;
        continue;
      }

      handler->tx_streamer.items_in_buffer += read_samples;
      handler->tx_streamer.prev_header.nof_samples -= read_samples;
      n += read_samples;
      end_of_burst = handler->tx_streamer.prev_header.end_of_burst;
      // INFO("RF_IIO: n=%d, read_samples=%d, end=%d\n", n, read_samples, end_of_burst);

      if ((handler->tx_streamer.items_in_buffer == handler->tx_streamer.buffer_size) || end_of_burst) {
        if (!have_timestamp) {
          if (timestamp != 0) {
            timestamp += handler->tx_streamer.buffer_size;
          }
        }
        have_timestamp = false;

        // Add packet header
        start_ptr[0] = common_preamble1;
        start_ptr[1] = common_preamble2;
        start_ptr[2] = common_preamble3;
        // time domain sync words
        start_ptr[3]  = time_preamble1;
        start_ptr[4]  = time_preamble2;
        start_ptr[5]  = time_preamble3;
        tstamp_ptr[3] = (handler->use_timestamps) ? timestamp : 0;

#if PRINT_TIMESTAMPS
        time_t         secs;
        double         frac_secs;
        struct timeval time;
        gettimeofday(&time, NULL);
        tstamp_to_time_iio(handler, *tstamp_ptr, &secs, &frac_secs);
        if (firstGo < 20) {
          printf(
              "send sec %d frac %f or %d ticks  [%4d] [%d] \n", secs, frac_secs, timestamp, time.tv_usec, time.tv_sec);
          firstGo++;
        }
#endif
        // submit buffer to DMA engine managed by libiio
        // INFO("RF_IIO: items_in_buffer = %d\n", handler->tx_streamer.items_in_buffer);
        int ret_buf = send_buf((void*)handler);
        // INFO("RF_IIO: pushed TS=%lu\n", timestamp);

        if (end_of_burst) {
          n = handler->tx_streamer.buffer_size;
        }

        uint32_t late_reg_value = 0;
        check_late_register(handler, &late_reg_value);
        if (late_reg_value) {
          lates++;
          INFO("RF_IIO: L");
          if (lates > 5) {
            log_late(handler, false);
            lates = 0;
          }
        }
        if (ret_buf) {
          handler->tx_streamer.items_in_buffer = 0;
        }
      }
    } while (n < handler->tx_streamer.buffer_size);
  }
  handler->tx_streamer.thread_completed = true;
  return NULL;
}

int rf_iio_send_timed(void*  h,
                      void*  data,
                      int    nsamples,
                      time_t secs,
                      double frac_secs,
                      bool   has_time_spec,
                      bool   blocking,
                      bool   is_start_of_burst,
                      bool   is_end_of_burst)
{
  void* _data[SRSRAN_MAX_PORTS] = {data, zero_mem, zero_mem, zero_mem};
  return rf_iio_send_timed_multi(
      h, _data, nsamples, secs, frac_secs, has_time_spec, blocking, is_start_of_burst, is_end_of_burst);
}

int rf_iio_send_timed_multi(void*  h,
                            void*  data[SRSRAN_MAX_PORTS],
                            int    nsamples,
                            time_t secs,
                            double frac_secs,
                            bool   has_time_spec,
                            bool   blocking,
                            bool   is_start_of_burst,
                            bool   is_end_of_burst)
{
  int  n       = 0;
  int  trials  = 0;
  long towrite = 0;

  tx_header_t       header  = {};
  rf_iio_handler_t* handler = (rf_iio_handler_t*)h;

  if (!handler->tx_streamer.stream_active) {
    rf_iio_start_tx_stream(h);
  }
  struct timeval time;
  gettimeofday(&time, NULL);
#ifdef PRINT_TIMESTAMPS
  if (firstGo < 5) {
    printf("init send sec %d frac %f [%4d] [%d] \n", secs, frac_secs, time.tv_usec, time.tv_sec);
    firstGo++;
  }
#endif
  do {
    towrite             = nsamples;
    float* samples_cf32 = (float*)&(((cf_t**)data)[0][n]);
    srsran_vec_convert_fi(samples_cf32, 32767.999f, handler->tx_streamer._conv_buffer, 2 * towrite);

    header.magic        = PKT_HEADER_MAGIC;
    header.nof_samples  = towrite;
    header.timestamp    = time_to_tstamp_iio(handler, secs, frac_secs);
    header.end_of_burst = is_end_of_burst;

    srsran_ringbuffer_write_block(&handler->tx_streamer.ring_buffer, &header, sizeof(tx_header_t));
    srsran_ringbuffer_write_block(
        &handler->tx_streamer.ring_buffer, (void*)(handler->tx_streamer._conv_buffer), sizeof(uint16_t) * 2 * towrite);
    n += towrite;
    trials++;
  } while (n < nsamples && trials < 100);
  // INFO("sent %d samples, time = %ld\n", nsamples, header.timestamp);
  return n;
}

rf_dev_t srsran_rf_dev_iio = {"iio",
                              rf_iio_devname,
                              rf_iio_start_rx_stream,
                              rf_iio_stop_rx_stream,
                              rf_iio_flush_buffer,
                              rf_iio_has_rssi,
                              rf_iio_get_rssi,
                              rf_iio_suppress_stdout,
                              rf_iio_register_error_handler,
                              rf_iio_open,
                              rf_iio_open_multi,
                              rf_iio_close,
                              rf_iio_set_rx_srate,
                              rf_iio_set_rx_gain,
                              NULL,
                              rf_iio_set_tx_gain,
                              NULL,
                              rf_iio_get_rx_gain,
                              rf_iio_get_tx_gain,
                              rf_iio_get_info,
                              rf_iio_set_rx_freq,
                              rf_iio_set_tx_srate,
                              rf_iio_set_tx_freq,
                              rf_iio_get_time,
                              NULL,
                              rf_iio_recv_with_time,
                              rf_iio_recv_with_time_multi,
                              rf_iio_send_timed,
                              .srsran_rf_send_timed_multi = rf_iio_send_timed_multi};

int register_plugin(rf_dev_t** rf_api)
{
  if (rf_api == NULL) {
    return SRSRAN_ERROR;
  }
  *rf_api = &srsran_rf_dev_iio;
  return SRSRAN_SUCCESS;
}
