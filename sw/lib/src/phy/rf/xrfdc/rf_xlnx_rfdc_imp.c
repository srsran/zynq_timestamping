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

#include "../rf_helper.h"
#include "../rf_plugin.h"
#include "rf_xlnx_rfdc_imp.h"
#include "srsran/srsran.h"
#include "xrfdc.h"
#include "xrfdc_clk.h"

#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <pthread.h>

enum LMK04208_CLK_SRC {
  INTERNAL_CLK_REF = 0,
  EXTERNAL_CLK_REF
};

unsigned int LMK04208_CKin[2][26] = {
               {0x00160040,0x80140320,0x80140321,0x80140322,
                0xC0140023,0x40140024,0x80141E05,0x03300006,0x01300007,0x06010008,
                0x55555549,0x9102410A,0x0401100B,0x1B0C006C,0x2302886D,0x0200000E,
                0x8000800F,0xC1550410,0x00000058,0x02C9C419,0x8FA8001A,0x10001E1B,
                0x0021201C,0x0180033D,0x0200033E,0x003F001F },
               {0x00160040,0x00143200,0x00143201,0x00140322,
                0xC0140023,0x40140024,0x80141E05,0x01100006,0x01100007,0x06010008,
                0x55555549,0x9102410A,0x0401100B,0x1B0C006C,0x2302884D,0x0200000E,
                0x8000800F,0xC1550410,0x00000058,0x02C9C419,0x8FA8001A,0x10001F5B,
                0x0021801C,0x0180033D,0x0200033E,0x003F001F }};

const unsigned int RFDC_DEVICE_ID           = 0;
const unsigned int I2CBUS                   = 12;
const double       RFDC_REF_SAMPLE_FREQ     = 245.76f;
const double       RFDC_REF_SAMPLE_FREQ_KHZ = 245760;
const double       RFDC_PLL_FREQ            = 1966.08f;
const unsigned int MIN_DATA_BUFFER_SIZE     = 1000;
const unsigned int METADATA_NSAMPLES        = 8;  // 8 32bit samples
const double       DEFAULT_TXRX_SRATE       = 1920000.0f;

static cf_t zero_mem[64*1024]    = {0};
static int  lates                = 0;
static int  rx_data_buffer_size  = MIN_DATA_BUFFER_SIZE;
static int  tx_data_buffer_size  = MIN_DATA_BUFFER_SIZE;

#define DEVNAME_RFDC        "RFdc"
#define CONVERT_BUFFER_SIZE (1024*1024)
#define common_preamble1    0xbbbbaaaa
#define common_preamble2    0xddddcccc
#define common_preamble3    0xffffeeee
#define common_preamble3_short \
                            0x0000ffee
#define time_preamble1      0xabcddcba
#define time_preamble2      0xfedccdef
#define time_preamble3      0xdfcbaefd

#define PKT_HEADER_MAGIC    0x12345678
//#define PRINT_TIMESTAMPS  1

typedef enum srs_dma_dir {
  RX_DMA,
  TX_DMA
} srs_dma_direction;

typedef enum srs_dma_buffer_pool_size {
  SMALL_BUFF_POOL_SIZE   = 4,
  DEFAULT_BUFF_POOL_SIZE = 8
} srs_dma_pool_size_t;

/* structure holding buffers allocation request */
struct buffers_alloc_request {
  unsigned int num_of_buffers;
  unsigned int buffer_size;
};

typedef struct dma_buffer_pool_desc {
  unsigned num_of_buffers;
  unsigned buffer_size;
  unsigned long **addresses;
} dma_buffers_desc_t;

typedef struct user_dma_buf_pointer {
  int id;
  int tx_size;
} user_dma_buf_ptr;

struct dma_buffers {
  int                dma_device_fd;             // File descriptor for interfacing srs_dma device
  volatile uint32_t* ts_enabler_mem;            // Pointer to registers memory of 'adc_timestamp_enabler_packetizer' block
  dma_buffers_desc_t dma_buffer_pool_desc;      // Pool of DMA buffers
  srs_dma_direction  direction;                 // RX or TX operation
  size_t             sample_size;               // Specifies size of a sample (depends of number of channels used by the streamer)
  bool               dma_queue_enabled;         // Specifies whether buffer queue is enabled and DMA is active
  user_dma_buf_ptr   current_user_buffer;       // Descriptor of the buffer owned by the user
};
typedef struct dma_buffers dma_buffers_t;

#define PAGE_SHIFT         12
#define SRS_DMA_IOC_MAGIC  'V'

#define SRS_DMA_ALLOC_BUFFERS    _IOW(SRS_DMA_IOC_MAGIC,  0, struct buffers_alloc_request)
#define SRS_DMA_DESTROY_BUFFERS  _IO(SRS_DMA_IOC_MAGIC,   1)
// rx
#define SRS_DMA_GET_RX_BUFFER    _IOR(SRS_DMA_IOC_MAGIC,  2, struct user_dma_buf_pointer)
#define SRS_DMA_PUT_RX_BUFFER    _IOW(SRS_DMA_IOC_MAGIC,  3, struct user_dma_buf_pointer)
// tx
#define SRS_DMA_GET_TX_BUFFER    _IOR(SRS_DMA_IOC_MAGIC,  4, struct user_dma_buf_pointer)
#define SRS_DMA_SEND_TX_BUFFER   _IOWR(SRS_DMA_IOC_MAGIC, 5, struct user_dma_buf_pointer)
// common
#define SRS_DMA_ENABLE_QUEUE     _IO(SRS_DMA_IOC_MAGIC,   6)
#define SRS_DMA_DISABLE_QUEUE    _IO(SRS_DMA_IOC_MAGIC,   7)

static void *reader_thread(void *arg);
static void *writer_thread(void *arg);

typedef struct {
  uint64_t  magic;
  uint64_t  timestamp;
  uint32_t  nof_samples;
  bool      end_of_burst;
} tx_header_t;

typedef struct {
  void*      parent;
  long long  _fs_hz;
  int16_t    _conv_buffer[CONVERT_BUFFER_SIZE];
  ssize_t    buf_count;
  uint32_t   nof_channels;
  long       buffer_size;
  bool       stream_active;
  int        items_in_buffer;
  uint32_t   tx_segment_time_len;
  float      secs;
  float      frac_secs;
  int        metadata_samples;
  int        preamble_location;
  pthread_mutex_t     stream_mutex;
  pthread_cond_t      stream_cvar;
  pthread_t           thread;
  bool                thread_completed;
  tx_header_t         prev_header;
  srsran_ringbuffer_t ring_buffer;
  struct dma_buffers  _buf;
} xrfdc_streamer;

typedef struct {
  bool                      use_timestamps;
  xrfdc_streamer            tx_streamer;
  xrfdc_streamer            rx_streamer;
  srsran_rf_error_handler_t iio_error_handler;
  void*                     iio_error_handler_arg;
  volatile unsigned int*    memory_map_ptr;
  srsran_rf_info_t          info;
  XRFdc                     RFdcInst;      // RFdc driver instance
  struct metal_device*      phy_deviceptr; // libmetal device descriptor
} rf_xrfdc_handler_t;

static int allocate_buffer_pool(dma_buffers_t *_buf,
                                const srs_dma_pool_size_t num_of_buffers,
                                const uint32_t buffer_length)
{
  int i = 0, ret = 0;
  int fd = _buf->dma_device_fd;
  struct buffers_alloc_request alloc_req = {0};

  alloc_req.num_of_buffers = num_of_buffers;
  alloc_req.buffer_size    = buffer_length * _buf->sample_size; // must be specified in Bytes

  // allocate array holding DMA buffer addresses (position in array corresponds to buffer ID)
  _buf->dma_buffer_pool_desc.addresses = malloc(num_of_buffers * sizeof(unsigned long *));
  if (!_buf->dma_buffer_pool_desc.addresses) {
    ERROR("failed to allocate memory for dma_buffer_pool_desc");
    goto err_out;
  }

  // ask the driver to allocate memory suitable for DMA
  ret = ioctl(fd, SRS_DMA_ALLOC_BUFFERS, &alloc_req);
  if (ret < 0){
    ERROR("SRS_DMA_ALLOC_BUFFERS ioctl() failed, errno=%d", errno);
    return -1;
  }

  //request an address of each dma buffer from the kernel driver using mmap call
  for (i = 0; i < num_of_buffers; i++) {
    _buf->dma_buffer_pool_desc.addresses[i] =
        (unsigned long *) mmap(0, buffer_length * _buf->sample_size,
                               PROT_READ | PROT_WRITE, MAP_SHARED, fd, i << PAGE_SHIFT);
    if (!_buf->dma_buffer_pool_desc.addresses[i]) {
      ERROR("Error mapping dma buffer with id=%d", i);
      goto err_out;
    }
  }
  _buf->dma_buffer_pool_desc.buffer_size = buffer_length;
  _buf->current_user_buffer.id = -1;
  return 0;
err_out:
  ioctl(fd, SRS_DMA_DESTROY_BUFFERS);
  return -1;
}

static int open_srs_dma_device(xrfdc_streamer* streamer, bool is_rx_dma, uint32_t nof_channels)
{
  char dev_name[16] = {0};
  snprintf(dev_name, sizeof(dev_name), "/dev/srs_%cx_dma", is_rx_dma ? 'r' : 't');

  int fd = open(dev_name, O_RDWR);
  if (fd < 0) {
    ERROR("Error opening device '%s'", dev_name);
    return -1;
  }
  streamer->_buf.dma_device_fd = fd;

  uint32_t nof_hw_rx_channels = 0;
  // For ADC path map registers memory of the adc_timestamp_enabler_packetizer
  if (is_rx_dma) {
    int devmem;
    if ((devmem = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
      ERROR("Error accessing memory-maped registers in FPGA");
      return -1;
    }
    streamer->_buf.ts_enabler_mem =
        (uint32_t*)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, devmem, 0xA0050000);
    if (!streamer->_buf.ts_enabler_mem) {
      ERROR("Error mapping ADC timestamp enabler registers");
      return -1;
    }
    rf_xrfdc_handler_t* h = (rf_xrfdc_handler_t*)streamer->parent;
    nof_hw_rx_channels    = h->memory_map_ptr[264];
    if (!nof_hw_rx_channels) {
      INFO("Warning: nof RX DMA channels reported by FPGA is 0, automatically setting it to 1");
      nof_hw_rx_channels = 1;
    }
    if (nof_channels != nof_hw_rx_channels) {
      ERROR("Requested number of RX channels doesn't match FPGA implementation (supports %u channels)",
            nof_hw_rx_channels);
      return -1;
    }
  }
  streamer->nof_channels           = (!is_rx_dma) ? 1 : nof_channels;
  streamer->_buf.sample_size       = is_rx_dma ? sizeof(uint16_t) * 2 * nof_channels : sizeof(uint16_t) * 2;
  streamer->_buf.direction         = (is_rx_dma) ? RX_DMA : TX_DMA;
  streamer->_buf.dma_queue_enabled = false;
  return 0;
}

static void srs_dma_cleanup_resources(dma_buffers_t* buf)
{
  if (!buf) {
    ERROR("%s called with buffer object = NULL", __func__);
    return;
  }
  // unmap DMA buffers
  uint32_t buffer_size = buf->dma_buffer_pool_desc.buffer_size;
  if (buf->dma_buffer_pool_desc.addresses) {
    for (int i = 0; i < buf->dma_buffer_pool_desc.num_of_buffers; i++) {
      munmap((void*)buf->dma_buffer_pool_desc.addresses[i], buffer_size * 4);
    }
    free(buf->dma_buffer_pool_desc.addresses);
    buf->dma_buffer_pool_desc.addresses      = NULL;
    buf->dma_buffer_pool_desc.num_of_buffers = 0;
    buf->dma_buffer_pool_desc.buffer_size    = 0;
  }
  buf->dma_queue_enabled = false;
}

static void close_srs_dma_device(xrfdc_streamer *streamer)
{
  if (!streamer) {
    ERROR("%s called with streamer object = NULL", __func__);
    return;
  }
  srs_dma_cleanup_resources(&streamer->_buf);
  // close file descriptor
  close(streamer->_buf.dma_device_fd);

  // for ADC path, unmap registers memory
  if (streamer->_buf.direction == RX_DMA) {
    munmap((void *) streamer->_buf.ts_enabler_mem, 0x1000);
  }
}

static int srs_dma_destroy_buffers(dma_buffers_t *buf)
{
  int ret = 0;
  srs_dma_cleanup_resources(buf);
  ret = ioctl(buf->dma_device_fd, SRS_DMA_DESTROY_BUFFERS);
  if (ret < 0) {
    ERROR("SRS_DMA_DESTROY_BUFFERS ioctl() failed, errno=%d", errno);
  }
  return ret;
}

static int srs_dma_allocate_buffers(dma_buffers_t *buf, const unsigned buf_length)
{
  if (allocate_buffer_pool(buf, DEFAULT_BUFF_POOL_SIZE, buf_length) < 0) {
    return -1;
  }
  if (buf->direction == TX_DMA) {
    // get first free data buffer from the DMA pool
    struct user_dma_buf_pointer user_dma_buf_info = {};
    int ret = ioctl(buf->dma_device_fd, SRS_DMA_GET_TX_BUFFER, &user_dma_buf_info);
    if (ret < 0) {
      ERROR("SRS_DMA_GET_TX_BUFFER ioctl() failed, errno=%d", errno);
      return ret;
    }
    memcpy(&buf->current_user_buffer, &user_dma_buf_info, sizeof(user_dma_buf_info));
  }
  return 0;
}

static int srs_dma_start_streaming(dma_buffers_t *buf)
{
  int ret = 0;
  ret = ioctl(buf->dma_device_fd, SRS_DMA_ENABLE_QUEUE);
  if (ret < 0) {
    ERROR("SRS_DMA_ENABLE_QUEUE ioctl() failed, errno=%d", errno);
    return ret;
  }
  // for ADC path, enable "adc_timestamp_enabler_packetizer" IP
  if (buf->direction == RX_DMA) {
    buf->ts_enabler_mem[0] = buf->dma_buffer_pool_desc.buffer_size;
    buf->ts_enabler_mem[1] = 1; // enable TS insertion and packetizing
  }
  buf->dma_queue_enabled = true;
  return ret;
}

static int srs_dma_stop_streaming(dma_buffers_t* buf)
{
  int ret = 0;
  if (!buf->dma_queue_enabled)
    return 0;

  // disable TS insertion and packetizing
  if (buf->direction == RX_DMA) {
    buf->ts_enabler_mem[1] = 0;
  }
  ret = ioctl(buf->dma_device_fd, SRS_DMA_DISABLE_QUEUE);
  if (ret < 0) {
    ERROR("SRS_DMA_DISABLE_QUEUE ioctl() failed, errno=%d", errno);
  }
  // reset ADC fifo
  if (buf->direction == RX_DMA) {
    INFO("RF_RFdc: resetting RX FIFO");
    while (buf->ts_enabler_mem[1]) {
      usleep(100);
    }
    // reset only after packetizing logic was stopped
    buf->ts_enabler_mem[2] = 1;
  }
  buf->dma_queue_enabled = false;
  return ret;
}

static void* srs_dma_get_data_ptr(dma_buffers_t* buf)
{
  unsigned buf_id = buf->current_user_buffer.id;
  return buf->dma_buffer_pool_desc.addresses[buf_id];
}

static int srs_dma_receive_data(dma_buffers_t* buf)
{
  struct user_dma_buf_pointer user_dma_buf_info = {};

  // if the user owns valid buffer - return it to DMA device
  if (buf->current_user_buffer.id != -1) {
    int ret = ioctl(buf->dma_device_fd, SRS_DMA_PUT_RX_BUFFER, &buf->current_user_buffer);
    if (ret < 0) {
      INFO("SRS_DMA_PUT_RX_BUFFER ioctl() failed, errno=%d", errno);
      return ret;
    }
  }
  // get data buffer from DMA
  int ret = ioctl(buf->dma_device_fd, SRS_DMA_GET_RX_BUFFER, &user_dma_buf_info);
  if (ret < 0) {
    INFO("SRS_DMA_GET_RX_BUFFER ioctl() failed, errno=%d", errno);
    return ret;
  }
  memcpy(&buf->current_user_buffer, &user_dma_buf_info, sizeof(user_dma_buf_info));
  return buf->dma_buffer_pool_desc.buffer_size;
}

static int srs_dma_send_data(dma_buffers_t* buf, const int tx_size)
{
  struct user_dma_buf_pointer user_dma_buf_info = buf->current_user_buffer;
  user_dma_buf_info.tx_size                     = tx_size; // Bytes

  // send data buffer to DMA, obtain next buffer ID through the same parameter
  int ret = ioctl(buf->dma_device_fd, SRS_DMA_SEND_TX_BUFFER, &user_dma_buf_info);
  if (ret < 0) {
    INFO("SRS_DMA_SEND_TX_BUFFER ioctl() failed, errno=%d", errno);
    return ret;
  }
  memcpy(&buf->current_user_buffer, &user_dma_buf_info, sizeof(user_dma_buf_info));
  return tx_size;
}

int refill_buffer(xrfdc_streamer* streamer, ssize_t* items_in_buffer)
{
  ssize_t nbytes_rx = 0;
  int     nsamples  = srs_dma_receive_data(&streamer->_buf);
  if (nsamples < 0) {
    *items_in_buffer = 0;
    return nsamples;
  }
  // on success srs_dma_receive_data() returns number of received IQ samples in bytes
  nbytes_rx        = nsamples * streamer->_buf.sample_size;
  *items_in_buffer = nsamples;
  return nbytes_rx;
}

static void log_late(rf_xrfdc_handler_t *h, bool is_rx) {
  if (h->iio_error_handler) {
    srsran_rf_error_t error;
    bzero(&error, sizeof(srsran_rf_error_t));
    error.opt  = is_rx ? 1 : 0;
    error.type = SRSRAN_RF_ERROR_LATE;
    h->iio_error_handler(h->iio_error_handler_arg, error);
  }
}

void rf_xrfdc_suppress_stdout(void* h)
{
  // do nothing
}

void rf_xrfdc_register_error_handler(void *h, srsran_rf_error_handler_t new_handler, void* arg)
{
  rf_xrfdc_handler_t *handler    = (rf_xrfdc_handler_t*) h;
  handler->iio_error_handler     = new_handler;
  handler->iio_error_handler_arg = arg;
}

bool rf_xrfdc_has_rssi(void *h)
{
  return false;
}

float rf_xrfdc_get_rssi(void *h)
{
  return 0.0f;
}

const char* rf_xrfdc_devname(void* h)
{
  return DEVNAME_RFDC;
}

/*
static void RFdc_IRQ_callback(void *CallBackRef, u32 Type, int Tile, u32 Block, u32 Event)
{
  //rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) CallBackRef;

  // Check the type of interrupt event
  if (Type == XRFDC_DAC_TILE) {
    INFO("Interrupt occurred for DAC%d_%d:", Tile, Block);

    if (Event & (XRFDC_IXR_FIFOUSRDAT_OF_MASK | XRFDC_IXR_FIFOUSRDAT_UF_MASK)) {
      INFO("\tFIFO Actual Overflow");
    }
    if (Event & (XRFDC_IXR_FIFOMRGNIND_OF_MASK | XRFDC_IXR_FIFOMRGNIND_UF_MASK)){
      INFO("\tFIFO Marginal Overflow");
    }
    if (Event & XRFDC_DAC_IXR_INTP_STG_MASK) {
      INFO("\tInterpolation Stages Overflow");
    }
    if (Event & XRFDC_IXR_QMC_GAIN_PHASE_MASK) {
      INFO("\tQMC gain/phase correction has overflowed/saturated");
    }
    if (Event & XRFDC_IXR_QMC_OFFST_MASK) {
      INFO("\tQMC offset correction has overflowed/saturated");
    }
  } else {
    INFO("Interrupt occurred for ADC%d_%d:", Tile, Block);

    if(Event & (XRFDC_IXR_FIFOUSRDAT_OF_MASK | XRFDC_IXR_FIFOUSRDAT_UF_MASK)) {
      INFO("\tFIFO Actual Overflow");
    }
    if(Event & (XRFDC_IXR_FIFOMRGNIND_OF_MASK | XRFDC_IXR_FIFOMRGNIND_UF_MASK)){
      INFO("\tFIFO Marginal Overflow");
    }
    if(Event & XRFDC_ADC_IXR_DMON_STG_MASK){
      INFO("\tDecimation Stages Overflow");
    }
    if(Event & XRFDC_ADC_OVR_VOLTAGE_MASK){
      INFO("\tADC buffer over voltage event");
    }
    if(Event & XRFDC_ADC_OVR_RANGE_MASK){
      INFO("\tADC over range event");
    }
    if(Event & XRFDC_ADC_FIFO_OVR_MASK){
      INFO("\tRF-ADC/RF-DAC FIFO over/underflow");
    }
  }
}
*/
static int configure_rfdc_controller(rf_xrfdc_handler_t *handler, const char *clock_source)
{
  int Status = 0;

  XRFdc          *RFdcInstPtr            = &handler->RFdcInst;
  XRFdc_Config   *ConfigPtr              = NULL;
  XRFdc_IPStatus  IPStatusPtr            = {};
  XRFdc_Mixer_Settings *adcMixerSettings = NULL;

  /* Define our desired ADC mixer configuration (mimics what we set in Vivado) */
  XRFdc_Mixer_Settings adcMixerSettings_ch0 = {
          .CoarseMixFreq  = XRFDC_COARSE_MIX_OFF,   //we are not using a coarse mixer type
          .MixerType      = XRFDC_MIXER_TYPE_FINE,  //we are using a fine mixer type
          .MixerMode      = XRFDC_MIXER_MODE_R2C,   //we will receive a real signal and return an I/Q pai
          .Freq           = -491.52,                //we want our signal to be centered at 2.4576 GHz (NCO freq) -> 2457.6 (Fc) - 1966.08 MHz (Fc) = 491.52 MHz
          .PhaseOffset    = 0,                      //NCO phase = 0
          .FineMixerScale = XRFDC_MIXER_SCALE_AUTO, //the fine mixer scale will be auto updated
          .EventSource    = XRFDC_EVNT_SRC_TILE
  };

  XRFdc_Mixer_Settings adcMixerSettings_ch1 = {
          .CoarseMixFreq  = XRFDC_COARSE_MIX_OFF,   //we are not using a coarse mixer type
          .MixerType      = XRFDC_MIXER_TYPE_FINE,  //we are using a fine mixer type
          .MixerMode      = XRFDC_MIXER_MODE_R2C,   //we will receive a real signal and return an I/Q pai
          .Freq           = -433.92,                 //we want our signal to be centered at 2.400 GHz (NCO freq) -> 2400 (Fc) - 1966.08 MHz (Fc) = 433.92 MHz
          .PhaseOffset    = 0,                      //NCO phase = 0
          .FineMixerScale = XRFDC_MIXER_SCALE_AUTO, //the fine mixer scale will be auto updated
          .EventSource    = XRFDC_EVNT_SRC_TILE
  };

  /* Define our desired DAC mixer configuration (mimics what we set in Vivado) */
  XRFdc_Mixer_Settings dacMixerSettings = {
          .CoarseMixFreq  = XRFDC_COARSE_MIX_OFF,   //we are not using a coarse mixer type
          .MixerType      = XRFDC_MIXER_TYPE_FINE,  //we are using a fine mixer type
          .MixerMode      = XRFDC_MIXER_MODE_C2R,   //we will receive an I/Q pair and return a real signal
          .Freq           = 433.92,                 //we want our signal to be centered at 2.400 GHz (NCO freq) -> 2400 (Fc) - 1966.08 MHz (Fc) = 433.92 MHz
          .PhaseOffset    = 0,                      //NCO phase = 0
          .FineMixerScale = XRFDC_MIXER_SCALE_AUTO, //the fine mixer scale will be auto updated
          .EventSource    = XRFDC_EVNT_SRC_TILE
  };

  u32 DataWidth       = 0;
  u32 GetFabricRate   = 0;
  u32 NyquistZonePtr  = 0;
  u8  CalibrationMode = 0;
  int DataConnectedI  = 0;
  int DataConnectedQ  = 0;
  XRFdc_BlockStatus BlockStatus  = {};

  // look for 'clock source' parameter in the arguments list
  u32 ref_clock_source = INTERNAL_CLK_REF;

  if (clock_source != NULL) {
    if (!strcmp(clock_source, "external")) {
      ref_clock_source = EXTERNAL_CLK_REF;
    }
  }

  struct metal_init_params init_param = METAL_INIT_DEFAULTS;
  if (metal_init(&init_param)) {
    ERROR("ERROR: Failed to run libmetal initialization");
    return -1;
  }

  // Initialize the RFdc driver
  ConfigPtr = XRFdc_LookupConfig(RFDC_DEVICE_ID);
  if (ConfigPtr == NULL) {
    ERROR("ERROR: Couldn't look up RFdc configuration");
    return -1;
  }
  handler->phy_deviceptr = NULL;
  Status = XRFdc_RegisterMetal(RFdcInstPtr, RFDC_DEVICE_ID, &handler->phy_deviceptr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: Failed to register libmetal device");
    return -1;
  }
  INFO("RF_RFdc: RFdc driver successfully registered and mapped to Libmetal");

  /* Initializes the controller */
  Status = XRFdc_CfgInitialize(RFdcInstPtr, ConfigPtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: Failed to initialize RFdc controller");
    return -1;
  }
  INFO("RF_RFdc: RFdc controller successfully initialized");

  printf("Configuring LMK04208 to use %s clock source\n", ref_clock_source == EXTERNAL_CLK_REF ? "external" : "internal");
  // Configuring the clocks
  LMK04208ClockConfig(I2CBUS, &LMK04208_CKin[ref_clock_source]);
  // The ADCs expect a 245.76 MHz reference signal (as set in Vivado)
  LMX2594ClockConfig(I2CBUS, RFDC_REF_SAMPLE_FREQ_KHZ);

  INFO("RF_RFdc: Clock configuration successfully finished");

  u16 ADC_Tile = 0;
  //u16 ADC_Tile = 1;
  u16 DAC_Tile = 1;

  // Explicitly wake up ADC tile 0 (does not change Vivado-provided parameters)
  Status = XRFdc_StartUp(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: Failed to wake up ADC tile 1");
    return -1;
  }
  INFO("RF_RFdc: ADC tile %d succesfully started up", ADC_Tile);

  /*explicitly wake up DAC tile 1 (does not change Vivado-provided parameters)*/
  Status = XRFdc_StartUp(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: Failed to wake up DAC tile 1");
    return -1;
  }
  INFO("RF_RFdc: DAC tile %d succesfully started up", DAC_Tile);

  // Capture the RFdc IP status (to be printed later for a specific tile)
  Status = XRFdc_GetIPStatus(RFdcInstPtr, &IPStatusPtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: RFdc status reports FAILURE");
    return -1;
  }
  /** -----------------   ADC   ------------------------- **
   *                                                      **
   * We'll explicitly configure ADC tile 1,               **
   * for which we have enabled channels 0 and 1 in Vivado **
   ** --------------------------------------------------- **/

  /** ---------------------------------------*/
  /** === Common ADC tile configuration ===  */
  /** ---------------------------------------*/
  // Explicitly configure the PLL
  // (overriding the parameters provided through Vivado)
  Status = XRFdc_DynamicPLLConfig(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, XRFDC_INTERNAL_PLL_CLK, RFDC_REF_SAMPLE_FREQ, RFDC_PLL_FREQ);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: failed to set Dynamic PLL configuration (ADC)");
    return -1;
  }
  INFO("RF_RFdc: PLL succesfully configured for ADC tile %d", ADC_Tile);

  // Explicitly enable the ADC FIFO
  Status = XRFdc_SetupFIFO(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, 1);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: failed to enable the ADC FIFO");
    return -1;
  }
  INFO("RF_RFdc: ADC FIFO succesfully enabled for ADC tile %d",ADC_Tile);

  // Print out the previously captured IP status [related to that ADC tile only]
  INFO("RF_RFdc: ADC tile %u status:", ADC_Tile);
  INFO("\tRF_RFdc: Tile enabled: %u", IPStatusPtr.ADCTileStatus[ADC_Tile].IsEnabled);
  INFO("\tRF_RFdc: Tile state: %u",   IPStatusPtr.ADCTileStatus[ADC_Tile].TileState);
  INFO("\tRF_RFdc: Tile block status mask: %u", IPStatusPtr.ADCTileStatus[ADC_Tile].BlockStatusMask);
  INFO("\tRF_RFdc: Tile power-up state: %u",    IPStatusPtr.ADCTileStatus[ADC_Tile].PowerUpState);
  INFO("\tRF_RFdc: Tile PLL state: %u", IPStatusPtr.ADCTileStatus[ADC_Tile].PLLState);

  // Configure the clock divider for the PL as required
  Status = XRFdc_SetFabClkOutDiv(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, XRFDC_FAB_CLK_DIV2);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: Failed to configure ADC tile clock dividers");
    return -1;
  }
  INFO("RF_RFdc: Clock divider for the PL succesfully set to 0x%u (2) for ADC tile %d", XRFDC_FAB_CLK_DIV2, ADC_Tile);

  u32 ClockSource   = 0;
  u16 FabClkDivPtr  = 0;
  u32 LockStatusPtr = 0;
  u8  FIFOEnablePtr = 0;
  XRFdc_PLL_Settings PLLSettings = {};

  // Print out the clock divider for the PL
  Status = XRFdc_GetFabClkOutDiv(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, &FabClkDivPtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetFabClkOutDiv failed");
    return -1;
  }
  INFO("RF_RFdc: Clock divider for the PL: 0x%u", FabClkDivPtr);

  // Print out the clock source
  Status = XRFdc_GetClockSource(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, &ClockSource);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetClockSource failed");
    return -1;
  }
  INFO("RF_RFdc: ADC clock source: %d", ClockSource);

  // Print out the PLL configuration
  Status = XRFdc_GetPLLConfig(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, &PLLSettings);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetPLLConfig failed");
    return -1;
  }
  INFO("RF_RFdc: PLL configuration:");
  INFO("\tRF_RFdc: PLL enabled (%u)",                   PLLSettings.Enabled);
  INFO("\tRF_RFdc: PLL reference clock frequency (%f)", PLLSettings.RefClkFreq);
  INFO("\tRF_RFdc: PLL sample rate (%f)",               PLLSettings.SampleRate);
  INFO("\tRF_RFdc: PLL reference clock divider (%d)",   PLLSettings.RefClkDivider);
  INFO("\tRF_RFdc: PLL feedback divider (%d)",          PLLSettings.FeedbackDivider);
  INFO("\tRF_RFdc: PLL output divider (%d)",            PLLSettings.OutputDivider);

  // Print out the PLL lock status
  Status = XRFdc_GetPLLLockStatus(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, &LockStatusPtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetPLLLockStatus failed");
    return -1;
  }
  INFO("RF_RFdc: PLL lock status: %u", LockStatusPtr);

  // Print out the FIFO status
  Status = XRFdc_GetFIFOStatus(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, &FIFOEnablePtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetFIFOStatus failed");
    return -1;
  }
  INFO("RF_RFdc: ADC FIFO status: %u", FIFOEnablePtr);

  /** ---------------------------------------------*/
  /** === channel specific configuration (ADC) === */
  /** ---------------------------------------------*/

  for (u16 Block = 0; Block < 4; Block++) {
    if (!XRFdc_IsADCBlockEnabled(RFdcInstPtr, ADC_Tile, Block)) {
      continue;
    }
    INFO("RF_RFdc: ADC tile %u channel %u is enabled", ADC_Tile, Block);

    // Explicitly set the ADC decimation factor (overriding the parameters provided through Vivado)
    Status = XRFdc_SetDecimationFactor(RFdcInstPtr, ADC_Tile, Block, XRFDC_INTERP_DECIM_8X);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: Failed to set ADC decimation factor");
      return -1;
    }
    INFO("RF_RFdc: ADC decimation factor succesfully configured for ADC tile %d channel %d", ADC_Tile, Block);

    /** these function calls must be used at startup to initialize the phase of the fine mixer to a valid state */
    // Set our desired NCO configuration;
    // NOTE: for some reason the vivado-set configuration was not applied or rewritten at some point
    adcMixerSettings = (Block == 0) ? &adcMixerSettings_ch0 : &adcMixerSettings_ch1;
    Status = XRFdc_SetMixerSettings(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, adcMixerSettings);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: Failed to set ADC NCO settings");
      return -1;
    }
    // Reset NCO phase of the DDC
    XRFdc_ResetNCOPhase(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block);
    //Generate a Tile Event
    XRFdc_UpdateEvent(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, XRFDC_EVENT_MIXER);

    INFO("RF_RFdc: ADC mixer succesfully configured");
    /** end of mixer configuration */

    // Explicitly set the Nyquist zone
/*  Status = XRFdc_SetNyquistZone(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, XRFDC_EVEN_NYQUIST_ZONE);
    if (Status != XRFDC_SUCCESS) {
      fprintf(stderr, "ERROR: Failed to set ADC Nyquist Zone\n");
      return -1;
    }
    INFO("RF_RFdc: ADC Nyquist zone succesfully set to 2 (even) for ADC tile %d", ADC_Tile);
 */
    Status = XRFdc_SetNyquistZone(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, XRFDC_ODD_NYQUIST_ZONE);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: Failed to set ADC Nyquist Zone");
      return -1;
    }
    INFO("RF_RFdc: ADC Nyquist zone succesfully set to 1 (odd) for ADC tile %d", ADC_Tile);

    // Read the number of samples per axi4-stream cycle
    Status = XRFdc_GetFabRdVldWords(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, &GetFabricRate);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetFabRdVldWords failed");
      return -1;
    }
    INFO("RF_RFdc: ADC tile %u channel %u number of read samples per axi4-stream cycle: %u", ADC_Tile, Block, GetFabricRate);

    // Read the number of samples per axi4-stream cycle
    Status = XRFdc_GetFabWrVldWords(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, &GetFabricRate);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetFabWrVldWords failed");
      return -1;
    }
    INFO("RF_RFdc: ADC tile %u channel %u number of write samples per axi4-stream cycle: %u", ADC_Tile, Block, GetFabricRate);

    // Print out the block configuration
    Status = XRFdc_GetBlockStatus(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, &BlockStatus);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetBlockStatus failed");
      return -1;
    }

    INFO("RF_RFdc: ADC block configuration:");
    INFO("\tRF_RFdc: ADC Sampling Frequency: %.03f", BlockStatus.SamplingFreq);
    INFO("\tRF_RFdc: Analog datapath status: %u",  BlockStatus.AnalogDataPathStatus);
    INFO("\tRF_RFdc: Digital datapath status: %u", BlockStatus.DigitalDataPathStatus);
    INFO("\tRF_RFdc: Datapath clock status: %u",   BlockStatus.DataPathClocksStatus);
    INFO("\tRF_RFdc: FIFO flags enabled: %u",      BlockStatus.IsFIFOFlagsEnabled);
    INFO("\tRF_RFdc: FIFO flags asserted: %u",     BlockStatus.IsFIFOFlagsAsserted);

    // Print out the configured mixer frequency
    Status = XRFdc_GetMixerSettings(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, adcMixerSettings);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetMixerSettings failed");
      return -1;
    }
    INFO("RF_RFdc: ADC Mixer Frequency: %.03f", adcMixerSettings->Freq);

    // Print out the ADC input data type
    if (XRFdc_GetDataType(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block)) {
      INFO("RF_RFdc: ADC input data type: real");
    } else {
      INFO("RF_RFdc: ADC input data type: I/Q");
    }
    // Print out the data width
    DataWidth = XRFdc_GetDataWidth(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block);
    INFO("RF_RFdc: ADC data width: %d", DataWidth);

    // Print out the digital path status
    bool adc_dig_path_en = XRFdc_IsADCDigitalPathEnabled(RFdcInstPtr, ADC_Tile, Block);
    INFO("RF_RFdc: Digital path is %sabled", adc_dig_path_en ? "en" : "dis");

    // Print out the FIFO status
    bool adc_fifo_en = XRFdc_IsFifoEnabled(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block);
    INFO("RF_RFdc: ADC FIFO is %sabled", adc_fifo_en ? "en" : "dis");

    // Print out the connected I amd Q data
    DataConnectedI = XRFdc_GetConnectedIData(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block);
    DataConnectedQ = XRFdc_GetConnectedQData(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block);
    INFO("RF_RFdc: ADC connected I data: %d, ADC connected Q data: %d", DataConnectedI, DataConnectedQ);

    // Print out the Nyquist zone
    Status = XRFdc_GetNyquistZone(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, &NyquistZonePtr);
    if (Status != XRFDC_SUCCESS) {
      ERROR("RF_RFdc: GetNyquistZone failed");
      return -1;
    }
    INFO("RF_RFdc: ADC Nyquist zone: %u", NyquistZonePtr);
    Status = XRFdc_GetCalibrationMode(RFdcInstPtr, ADC_Tile, Block, &CalibrationMode);
    if (Status != XRFDC_SUCCESS) {
      ERROR("RF_RFdc: XRFdc_GetCalibrationMode failed");
      return -1;
    }
    INFO("RF_RFdc: ADC calibration mode: %u", CalibrationMode);
  }

  /** -----------------   DAC   ------------------ **
   *                                               **
   * We'll explicitly configure DAC tile 1,        **
   * for which we have enabled channel 0 in Vivado **
   ** -------------------------------------------- **/
  u32 DecoderModePtr = 0;
  u16 InvSincModePtr = 0;
  u32 DACMixedMode   = 0;

  /** ---------------------------------------*/
  /** === Common DAC tile configuration ===  */
  /** ---------------------------------------*/

  // Explicitly configure the PLL (overriding the parameters provided through Vivado)
  Status = XRFdc_DynamicPLLConfig(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, XRFDC_INTERNAL_PLL_CLK, RFDC_REF_SAMPLE_FREQ, RFDC_PLL_FREQ);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: failed to set Dynamic PLL configuration (DAC)");
    return -1;
  }
  INFO("RF_RFdc: PLL succesfully configured for DAC tile %d", DAC_Tile);

  // Explicitly enable the DAC FIFO
  Status = XRFdc_SetupFIFO(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, 1);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: failed to enable the DAC FIFO");
    return -1;
  }
  INFO("RF_RFdc: DAC FIFO succesfully enabled for DAC tile %d", DAC_Tile);

  // Print out the previously captured IP status [related to that DAC tile only]
  INFO("RF_RFdc: DAC tile %u status:", DAC_Tile);
  INFO("\tRF_RFdc: Tile enabled: %u",          IPStatusPtr.DACTileStatus[DAC_Tile].IsEnabled);
  INFO("\tRF_RFdc: Tile state: %u",            IPStatusPtr.DACTileStatus[DAC_Tile].TileState);
  INFO("\tRF_RFdc: Tile block status mask: %u",IPStatusPtr.DACTileStatus[DAC_Tile].BlockStatusMask);
  INFO("\tRF_RFdc: Tile power-up state: %u",   IPStatusPtr.DACTileStatus[DAC_Tile].PowerUpState);
  INFO("\tRF_RFdc: Tile PLL state: %u",        IPStatusPtr.DACTileStatus[DAC_Tile].PLLState);

  // Configure the clock divider for the PL as required
  Status = XRFdc_SetFabClkOutDiv(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, XRFDC_FAB_CLK_DIV1);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: Failed to configure DAC tile clock dividers");
    return -1;
  }
  INFO("RF_RFdc: Clock divider for the PL succesfully set to 0x%u (1) for DAC tile %d", XRFDC_FAB_CLK_DIV1, DAC_Tile);

  // Print out the clock divider for the PL
  Status = XRFdc_GetFabClkOutDiv(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, &FabClkDivPtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetFabClkOutDiv failed");
    return -1;
  }
  INFO("RF_RFdc: Clock divider for the PL: 0x%u", FabClkDivPtr);

  // Print out the clock source
  Status = XRFdc_GetClockSource(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, &ClockSource);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetClockSource failed");
    return -1;
  }
  INFO("RF_RFdc: DAC clock source: %d", ClockSource);

  // Print out the PLL configuration
  Status = XRFdc_GetPLLConfig(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, &PLLSettings);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetPLLConfig failed");
    return -1;
  }
  INFO("RF_RFdc: PLL configuration:");
  INFO("\tRF_RFdc: PLL enabled (%u)",                   PLLSettings.Enabled);
  INFO("\tRF_RFdc: PLL reference clock frequency (%f)", PLLSettings.RefClkFreq);
  INFO("\tRF_RFdc: PLL sample rate (%f)",               PLLSettings.SampleRate);
  INFO("\tRF_RFdc: PLL reference clock divider (%d)",   PLLSettings.RefClkDivider);
  INFO("\tRF_RFdc: PLL feedback divider (%d)",          PLLSettings.FeedbackDivider);
  INFO("\tRF_RFdc: PLL output divider (%d)",            PLLSettings.OutputDivider);

  // Print out the PLL lock status
  Status = XRFdc_GetPLLLockStatus(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, &LockStatusPtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetPLLLockStatus failed");
    return -1;
  }
  INFO("RF_RFdc: PLL lock status: %u", LockStatusPtr);

  // Print out the FIFO status
  Status = XRFdc_GetFIFOStatus(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, &FIFOEnablePtr);
  if (Status != XRFDC_SUCCESS) {
    ERROR("ERROR: GetFIFOStatus failed");
    return -1;
  }
  INFO("RF_RFdc: DAC FIFO status: %u", FIFOEnablePtr);

  /** ---------------------------------------------*/
  /** === channel specific configuration (DAC) === */
  /** ---------------------------------------------*/
  for (u16 Block = 0; Block < 4; Block++) {
    if (!XRFdc_IsDACBlockEnabled(RFdcInstPtr, DAC_Tile, Block)) {
      continue;
    }
    INFO("RF_RFdc: DAC tile %u channel %u is enabled", DAC_Tile, Block);

    // Explicitly set the DAC interpolation factor (overriding the parameters provided through Vivado)
    Status = XRFdc_SetInterpolationFactor(RFdcInstPtr, DAC_Tile, Block, XRFDC_INTERP_DECIM_8X);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: Failed to set DAC interpolation factor");
      return -1;
    }
    INFO("RF_RFdc: DAC interpolation factor succesfully configured for DAC tile %d channel %d", DAC_Tile, Block);

    /** these function calls must be used at startup to initialize the phase of the fine mixer to a valid state */
    // Set our desired NCO configuration;
    // NOTE: for some reason the vivado-set configuration was not applied or rewritten at some point
    Status = XRFdc_SetMixerSettings(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, &dacMixerSettings);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: Failed to set DAC NCO settings");
      return -1;
    }
    // Reset NCO phase of the DUC
    XRFdc_ResetNCOPhase(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block);
    //Generate a Tile Event
    XRFdc_UpdateEvent(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, XRFDC_EVENT_MIXER);

    INFO("RF_RFdc: DAC mixer succesfully configured");
    /** end of DAC channel mixer configuration */

    // Explicitly set the Nyquist zone
    // TODO:
    Status = XRFdc_SetNyquistZone(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, XRFDC_EVEN_NYQUIST_ZONE);
    if (Status != XRFDC_SUCCESS) {
      ERROR("RF_RFdc: Failed to set DAC Nyquist Zone");
      return -1;
    }
    INFO("RF_RFdc: DAC Nyquist zone succesfully set to 2 (even) for DAC tile %d", DAC_Tile);

/*  Status = XRFdc_SetNyquistZone(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, XRFDC_ODD_NYQUIST_ZONE);
    if (Status != XRFDC_SUCCESS) {
      ERROR("RF_RFdc: Failed to set DAC Nyquist Zone");
      return -1;
    }
    INFO("RF_RFdc: DAC Nyquist zone succesfully set to 1 (odd) for DAC tile %d", DAC_Tile);
*/
    // Explicitly set the decoder mode
    Status = XRFdc_SetDecoderMode(RFdcInstPtr, DAC_Tile, Block, XRFDC_DECODER_MAX_SNR_MODE);
    //Status = XRFdc_SetDecoderMode(RFdcInstPtr, Tile, Block, XRFDC_DECODER_MAX_LINEARITY_MODE);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: Failed to set DAC decoder mode");
      return -1;
    }
    INFO("RF_RFdc: DAC decoder mode succesfully set to %d (max SNR) for DAC tile %d", XRFDC_DECODER_MAX_SNR_MODE, DAC_Tile);

    // Explicitly disable the inverse sinc FIR (we're on the second Nyquist zone)
    Status = XRFdc_SetInvSincFIR(RFdcInstPtr, DAC_Tile, Block, 0);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: Failed to disable the inverse sinc FIR");
      return XRFDC_FAILURE;
    }
    INFO("RF_RFdc: DAC inverse sinc FIR disalbed for DAC tile %d", DAC_Tile);

    // Read the number of samples per axi4-stream cycle
    Status = XRFdc_GetFabRdVldWords(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, &GetFabricRate);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetFabRdVldWords failed");
      return -1;
    }
    INFO("RF_RFdc: DAC tile %u channel %u number of read samples per axi4-stream cycle: %u", DAC_Tile, Block,
         GetFabricRate);
    // Read the number of samples per axi4-stream cycle
    Status = XRFdc_GetFabWrVldWords(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, &GetFabricRate);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetFabWrVldWords failed");
      return -1;
    }
    INFO("RF_RFdc: DAC tile %u channel %u number of write samples per axi4-stream cycle: %u", DAC_Tile, Block,
         GetFabricRate);

    // Print out the block configuration
    Status = XRFdc_GetBlockStatus(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, &BlockStatus);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetBlockStatus failed");
      return -1;
    }

    INFO("RF_RFdc: DAC block configuration:");
    INFO("\tRF_RFdc: DAC Sampling Frequency: %.03f", BlockStatus.SamplingFreq);
    INFO("\tRF_RFdc: Analog datapath status: %u", BlockStatus.AnalogDataPathStatus);
    INFO("\tRF_RFdc: Digital datapath status: %u", BlockStatus.DigitalDataPathStatus);
    INFO("\tRF_RFdc: Datapath clock status: %u", BlockStatus.DataPathClocksStatus);
    INFO("\tRF_RFdc: FIFO flags enabled: %u", BlockStatus.IsFIFOFlagsEnabled);
    INFO("\tRF_RFdc: FIFO flags asserted: %u", BlockStatus.IsFIFOFlagsAsserted);

    // Print out the configured mixer frequency
    Status = XRFdc_GetMixerSettings(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, &dacMixerSettings);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetMixerSettings failed");
      return -1;
    }
    INFO("RF_RFdc: DAC Mixer Frequency: %.03f", dacMixerSettings.Freq);

    // Print out the DAC input data type*/
    if (XRFdc_GetDataType(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block)) {
      INFO("RF_RFdc: DAC input data type: real");
    } else {
      INFO("RF_RFdc: DAC input data type: I/Q");
    }

    // Print out the data width
    DataWidth = XRFdc_GetDataWidth(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block);
    INFO("RF_RFdc: DAC data width: %d", DataWidth);

    // Print out the digital path status
    bool dac_dig_path_en = XRFdc_IsDACDigitalPathEnabled(RFdcInstPtr, DAC_Tile, Block);
    INFO("RF_RFdc: Digital path is %sabled", dac_dig_path_en ? "en" : "dis");

    // Print out the FIFO status
    bool dac_fifo_en = XRFdc_IsFifoEnabled(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block);
    INFO("RF_RFdc: DAC FIFO is %sabled", dac_fifo_en ? "en" : "dis");

    // Print out the connected I and Q data
    DataConnectedI = XRFdc_GetConnectedIData(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block);
    DataConnectedQ = XRFdc_GetConnectedQData(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block);
    INFO("RF_RFdc: DAC connected I data: %d, DAC connected Q data: %d", DataConnectedI, DataConnectedQ);

    // Print out the Nyquist zone
    Status = XRFdc_GetNyquistZone(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, Block, &NyquistZonePtr);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetNyquistZone failed");
      return -1;
    }
    INFO("RF_RFdc: DAC Nyquist zone %u", NyquistZonePtr);

    // Print out the decoder mode
    Status = XRFdc_GetDecoderMode(RFdcInstPtr, DAC_Tile, Block, &DecoderModePtr);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetDecoderMode failed");
      return -1;
    }
    INFO("RF_RFdc: DAC decoder mode %u", DecoderModePtr);

    // Print out the inverse sinc FIR status
    Status = XRFdc_GetInvSincFIR(RFdcInstPtr, DAC_Tile, Block, &InvSincModePtr);
    if (Status != XRFDC_SUCCESS) {
      ERROR("ERROR: GetInvSincFIR failed");
      return -1;
    }
    INFO("RF_RFdc: DAC inverse sinc FIR status %u", InvSincModePtr);

    // Print out the mixed mode
    DACMixedMode = XRFdc_GetMixedMode(RFdcInstPtr, DAC_Tile, Block);
    INFO("RF_RFdc: DAC mixed mode: %d", DACMixedMode);
  }

/*
  // Register a callabck called from RFdc interrupt handler
  XRFdc_SetStatusHandler(RFdcInstPtr, handler, (XRFdc_StatusHandler)RFdc_IRQ_callback);

  // Enable those events that we want to log
  u32 DAC_tile_en_IRQ_mask = 0, ADC_tile_en_IRQ_mask = 0;

  DAC_tile_en_IRQ_mask    |= XRFDC_IXR_FIFOUSRDAT_MASK;     // Indicates that the FIFO interface is incorrectly setup, clocks/data throughput mismatch.
  DAC_tile_en_IRQ_mask    |= XRFDC_DAC_IXR_INTP_STG_MASK;   // Indicates that one of the interpolation stages has overflowed/saturated.
                                                            // Flags are per-stage and I/Q paths to indicate where the overflow has occurred. Data amplitude is too high.
  DAC_tile_en_IRQ_mask    |= XRFDC_IXR_QMC_GAIN_PHASE_MASK; // Indicates the QMC gain/phase correction has overflowed/saturated. Data amplitude, or correction factors are too high and should be reduced.
  DAC_tile_en_IRQ_mask    |= XRFDC_IXR_QMC_OFFST_MASK;      // Indicates the QMC offset correction has overflowed/saturated. Data amplitude, or correction factors are too high and should be reduced.

  ADC_tile_en_IRQ_mask    |= XRFDC_IXR_FIFOUSRDAT_MASK;     // Indicates that the FIFO interface is incorrectly setup, clocks/data throughput mismatch.
  ADC_tile_en_IRQ_mask    |= XRFDC_ADC_IXR_DMON_STG_MASK;   // Indicates one of the RF-ADC decimation stages has overflowed/saturated.
                                                            // Flags are per-stage and I/Q paths to indicate where the overflow has occurred. Data amplitude is too high.
  ADC_tile_en_IRQ_mask    |= XRFDC_ADC_OVR_VOLTAGE_MASK;    // Indicates the analog input is exceeding the safe input range of the RF-ADC input buffer and the buffer has been shut down.
                                                            // The input signal amplitude and common mode should be brought within range.
  ADC_tile_en_IRQ_mask    |= XRFDC_ADC_OVR_RANGE_MASK;      // Indicates the analog input is exceeding the full-scale range of the RF-ADC. The input signal amplitude should be reduced.
                                                            // To clear this interrupt, the sub-RF-ADC Over range interrupts must be cleared.
  ADC_tile_en_IRQ_mask    |= XRFDC_ADC_FIFO_OVR_MASK;       // RF-ADC/RF-DAC FIFO over/underflow

  for (u16 BlockId = 0; BlockId < 4; BlockId++) {
    if (!XRFdc_IsDACBlockEnabled(RFdcInstPtr, DAC_Tile, BlockId)) {
      continue;
    }
    XRFdc_IntrEnable(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, BlockId, DAC_tile_en_IRQ_mask);
    u32 IntrMask = 0;
    XRFdc_GetEnabledInterrupts(RFdcInstPtr, XRFDC_DAC_TILE, DAC_Tile, BlockId, &IntrMask);
    INFO("RF_RFdc: DAC Tile 1, block %d enabled IRQs mask: %x:", BlockId, IntrMask);
    INFO("RF_RFdc: DAC Tile 1, block %d Termination voltage: %f", BlockId, RFdcInstPtr->DAC_Tile[DAC_Tile].DACBlock_Analog_Datapath[BlockId].TerminationVoltage);
  }

  for (u16 BlockId = 0; BlockId < 2; BlockId++) {
    if (!XRFdc_IsADCBlockEnabled(RFdcInstPtr, ADC_Tile, BlockId)) {
      continue;
    }
    XRFdc_IntrEnable(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, BlockId, ADC_tile_en_IRQ_mask);
    u32 IntrMask = 0;
    XRFdc_GetEnabledInterrupts(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, BlockId, &IntrMask);
    INFO("RF_RFdc: ADC Tile 0, block %d enabled IRQs mask: %x:", BlockId, IntrMask);
  }
*/

  return 0;
}

int rf_xrfdc_start_rx_stream(void* h, bool now)
{
  rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)h;

  // If the sampling rate was not configured, ADC/DAC paths will be held in a reset state;
  // in this case, if radio wants to start rx stream, let's set the default sampling rate (1.92MHz)
  if (!handler->rx_streamer._fs_hz) {
    rf_xrfdc_set_rx_srate(h, DEFAULT_TXRX_SRATE);
    rf_xrfdc_set_tx_srate(h, DEFAULT_TXRX_SRATE);
    INFO("RF_RFdc: default srate has been configured");
  }

  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  handler->rx_streamer.items_in_buffer = 0;
  handler->rx_streamer.stream_active   = true;

  if (handler->rx_streamer.thread_completed) {
    // if rx thread was stopped before - restart it
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
  INFO("RF_RFdc: RX stream started");
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);
  return SRSRAN_SUCCESS;
}

static void stop_rx_stream(rf_xrfdc_handler_t* handler)
{
  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  handler->rx_streamer.stream_active = false;

  srs_dma_stop_streaming(&handler->rx_streamer._buf);

  while (!handler->rx_streamer.thread_completed) {
    pthread_cond_wait(&handler->rx_streamer.stream_cvar, &handler->rx_streamer.stream_mutex);
  }
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);
  pthread_join(handler->rx_streamer.thread, NULL);
  srs_dma_destroy_buffers(&handler->rx_streamer._buf);
}

int rf_xrfdc_stop_rx_stream(void* h)
{
  rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)h;
  if (!handler->rx_streamer.thread_completed) {
    stop_rx_stream(handler);
    srsran_ringbuffer_stop(&handler->rx_streamer.ring_buffer);
    INFO("RF_RFdc: RX stream stopped");
  }
  return 0;
}

int rf_xrfdc_start_tx_stream(void *h)
{
  rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) h;
  handler->tx_streamer.items_in_buffer = 0;
  pthread_mutex_lock(&handler->tx_streamer.stream_mutex);
  handler->tx_streamer.stream_active = true;
  pthread_cond_signal(&handler->tx_streamer.stream_cvar);
  pthread_mutex_unlock(&handler->tx_streamer.stream_mutex);
  return SRSRAN_SUCCESS;
}

static int rf_xrfdc_stop_tx_stream(void *h)
{
  rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) h;

  pthread_mutex_lock(&handler->tx_streamer.stream_mutex);
  handler->tx_streamer.stream_active = false;
  pthread_mutex_unlock(&handler->tx_streamer.stream_mutex);

  pthread_join(handler->tx_streamer.thread, NULL);

  srs_dma_stop_streaming(&handler->tx_streamer._buf);
  srs_dma_destroy_buffers(&handler->tx_streamer._buf);
  return SRSRAN_SUCCESS;
}


static bool buffer_initialized(xrfdc_streamer *streamer)
{
  return (streamer->_buf.dma_buffer_pool_desc.addresses != NULL);
}

static void configure_timestamping(void* h, uint32_t nof_prbs)
{
  bool                skip_rx_buf_reconfig  = false;
  bool                skip_tx_buf_reconfig  = false;
  rf_xrfdc_handler_t* handler               = (rf_xrfdc_handler_t*)h;
  handler->use_timestamps                   = true;

  handler->tx_streamer.metadata_samples = METADATA_NSAMPLES;
  handler->rx_streamer.metadata_samples = METADATA_NSAMPLES / handler->rx_streamer.nof_channels;

  uint32_t sf_len = SRSRAN_SF_LEN_PRB(nof_prbs);

  // determine rx_data_buffer_size
  if (nof_prbs <= 6) {
    rx_data_buffer_size = MIN_DATA_BUFFER_SIZE;
  } else if (nof_prbs > 6 && nof_prbs <= 15) {
    rx_data_buffer_size = MIN_DATA_BUFFER_SIZE * 2;
  } else if (nof_prbs <= 25) {
    rx_data_buffer_size = sf_len;
  } else {
    rx_data_buffer_size = sf_len / 2;
  }
  tx_data_buffer_size       = rx_data_buffer_size;
  long total_tx_buffer_size = tx_data_buffer_size + handler->tx_streamer.metadata_samples;

  if (handler->rx_streamer.buffer_size == rx_data_buffer_size) {
    INFO("RF_RFdc: RX buffer size is the same as the one being configured.");
    skip_rx_buf_reconfig = true;
  }
  if (handler->tx_streamer.buffer_size == tx_data_buffer_size) {
    INFO("RF_RFdc: TX buffer size is the same as the one being configured.");
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
    srsran_ringbuffer_stop(&handler->rx_streamer.ring_buffer);
    srsran_ringbuffer_reset(&handler->rx_streamer.ring_buffer);
    INFO("RF_RFdc: changing DMA buffer size, RX stream paused");
    // invalidate any partially read data packet
    handler->rx_streamer.prev_header.nof_samples = 0;
  }
  if (need_tx_stream_restart) {
    rf_xrfdc_stop_tx_stream(handler);
    INFO("RF_RFdc: changing DMA buffer size, TX stream paused");
    // invalidate any partially read data packet
    handler->tx_streamer.prev_header.nof_samples = 0;
  }

  handler->rx_streamer.buffer_size = rx_data_buffer_size;
  handler->tx_streamer.buffer_size = tx_data_buffer_size;

  if (!skip_tx_buf_reconfig) {
    if (buffer_initialized(&handler->tx_streamer)) {
      srs_dma_stop_streaming(&handler->tx_streamer._buf);
      srs_dma_destroy_buffers(&handler->tx_streamer._buf);
    }
    if (srs_dma_allocate_buffers(&handler->tx_streamer._buf, total_tx_buffer_size) < 0) {
      ERROR("RF_RFdc: Could not create TX buffer");
    }
    srs_dma_start_streaming(&handler->tx_streamer._buf);
  }
  if (need_rx_stream_restart) {
    rf_xrfdc_start_rx_stream(handler, true);
  }
  if (need_tx_stream_restart) {
    rf_xrfdc_start_tx_stream(handler);
  }
}

static int open_mem_register(void* h)
{
  rf_xrfdc_handler_t* handler  = (rf_xrfdc_handler_t*)h;
  unsigned int        reg_size = 0x1F40;
  off_t               reg_addr = 0x00A0040000;
  int                 mm_reg_d;
  // Map the MM-reg address into user space getting a virtual address for it
  if ((mm_reg_d = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
    ERROR("Error accessing the memory-mapped register");
    return -1;
  }
  handler->memory_map_ptr = (uint32_t*)mmap(NULL, reg_size, PROT_READ | PROT_WRITE, MAP_SHARED, mm_reg_d, reg_addr);
  return 0;
}

int rf_xrfdc_open(char *args, void **h)
{
  return rf_xrfdc_open_multi(args, h, 1);
}

int rf_xrfdc_open_multi(char* args, void** h, uint32_t nof_channels)
{
  rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)malloc(sizeof(rf_xrfdc_handler_t));

  if (!handler) {
    fprintf(stderr, "Error allocating memory for RF\n");
    return -1;
  }
  *h = handler;

  if (nof_channels == 0) {
    INFO("Warning: setting nof_channels to 1 by default (argument nof_channels=%u)\n", nof_channels);
    nof_channels = 1;
  }
  if (nof_channels > 2) {
    fprintf(stderr, "only 1 or 2 RF channels are supported (argument nof_channels=%u)\n", nof_channels);
    return -1;
  }

  /// Handle rf arguments.
  uint32_t n_prb = 0;
  if (!parse_uint32(args, "n_prb", 0, &n_prb)) {
    // set to 6 PRBs if not provided by the user
    n_prb = 6;
  }
  char clock_source[RF_PARAM_LEN] = "internal";
  parse_string(args, "clock", 0, clock_source);

  // Configure RFdc controller
  if (configure_rfdc_controller(handler, clock_source) < 0) {
    return -1;
  }
  // map register memory of the centralized_AXI_controller
  if (open_mem_register(handler) < 0) {
    return -1;
  }

  handler->rx_streamer.parent = handler;
  handler->tx_streamer.parent = handler;

  // open ADC DMA device descriptor
  if (open_srs_dma_device(&handler->rx_streamer, true, nof_channels) < 0) {
    return -1;
  }
  // open DAC DMA device descriptor
  if (open_srs_dma_device(&handler->tx_streamer, false, nof_channels) < 0) {
    return -1;
  }

  pthread_mutex_init(&handler->rx_streamer.stream_mutex, NULL);
  pthread_cond_init(&handler->rx_streamer.stream_cvar, NULL);
  srsran_ringbuffer_init(&handler->rx_streamer.ring_buffer, 50000 * 1920);
  handler->rx_streamer.thread_completed = false;
  pthread_create(&handler->rx_streamer.thread, NULL, reader_thread, handler);

  pthread_mutex_init(&handler->tx_streamer.stream_mutex, NULL);
  pthread_cond_init(&handler->tx_streamer.stream_cvar, NULL);
  srsran_ringbuffer_init(&handler->tx_streamer.ring_buffer, 200 * 1920);
  handler->tx_streamer.thread_completed = false;
  pthread_create(&handler->tx_streamer.thread, NULL, writer_thread, handler);

  handler->rx_streamer.buf_count = 0;
  handler->tx_streamer.buf_count = 0;

  handler->rx_streamer.preamble_location = 0;
  handler->tx_streamer.preamble_location = 0;

  configure_timestamping(handler, n_prb);

  return 0;
}

int rf_xrfdc_close(void *h)
{
  rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) h;

  if(handler->tx_streamer.thread && !handler->tx_streamer.thread_completed) {
    pthread_cancel(handler->tx_streamer.thread);
  }
  if(handler->rx_streamer.thread && !handler->rx_streamer.thread_completed) {
    pthread_cancel(handler->rx_streamer.thread);
  }
  srs_dma_stop_streaming(&handler->rx_streamer._buf);
  srs_dma_stop_streaming(&handler->tx_streamer._buf);
  close_srs_dma_device(&handler->rx_streamer);
  close_srs_dma_device(&handler->tx_streamer);
  return SRSRAN_SUCCESS;
}

uint64_t time_to_hw_tstamp(rf_xrfdc_handler_t* handler, time_t secs, double frac_secs)
{
  return (uint64_t)(handler->tx_streamer._fs_hz * ((double)secs)) +
         (uint64_t)(round((double)handler->tx_streamer._fs_hz * frac_secs));
}

void hw_tstamp_to_time(rf_xrfdc_handler_t* handler, uint64_t tstamp, time_t* secs, double* frac_secs)
{
  uint64_t srate_int = (uint64_t)handler->rx_streamer._fs_hz;
  uint64_t remainder = 0;
  if (secs && frac_secs) {
    *secs      = tstamp / srate_int;
    remainder  = tstamp % srate_int;
    *frac_secs = (double)remainder / srate_int;
  }
}

double rf_xrfdc_set_rx_srate(void *h, double rate)
{
  rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) h;
  bool stream_needs_restart = false;

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
  INFO("RF_RFdc: changing srate %s", stream_needs_restart ? "RX stream paused" : "");

  uint32_t req_sf_nsamples = (uint32_t) (rate / 1e3);
  uint32_t symbol_sz       = req_sf_nsamples / 15;
  if (srsran_symbol_sz_isvalid(symbol_sz)) {
    handler->memory_map_ptr[4] = symbol_sz;

    //read back and print current FPGA RFdc FFT size
    INFO("RF_RFdc: current RFdc NFFT = %u", handler->memory_map_ptr[4]);
    handler->rx_streamer._fs_hz = rate;

    // wait until MMCM generating baseband clock locks
    while(!handler->memory_map_ptr[263]) {
      usleep(100);
    }
    INFO("RF_RFdc: MMCM locked");
  } else {
    ERROR("RF_RFdc: invalid sampling rate requested");
  }

  if (stream_needs_restart) {
    //restart the RX stream
    rf_xrfdc_start_rx_stream(handler, true);
  }
  return rate;
}

double rf_xrfdc_set_tx_srate(void *h, double freq)
{
  rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) h;
  handler->tx_streamer._fs_hz = handler->rx_streamer._fs_hz;
  return freq;
}

int rf_xrfdc_set_rx_gain(void *h, double gain)
{
  // Not supported by RFSoC
  return 0;
}

int rf_xrfdc_set_tx_gain(void *h, double gain)
{
  // Not supported by RFSoC
  return 0;
}

double rf_xrfdc_get_rx_gain(void *h)
{
  // Not supported by RFSoC, return some default value for API compatibility
  return 50.0f;
}

double rf_xrfdc_get_tx_gain(void *h)
{
  // Not supported by RFSoC
  return 60.0f;
}

double rf_xrfdc_set_rx_freq(void* h, uint32_t ch, double freq)
{
  rf_xrfdc_handler_t* handler     = (rf_xrfdc_handler_t*)h;
  XRFdc*              RFdcInstPtr = &handler->RFdcInst;

  u16 ADC_Tile = 0;
  if (ch > 1) {
    printf("Warning! channel (%d) specifying in set_rx_freq is out of range (ADC [0 1] are supported)\n"
           "Using ADC0 by default\n",
           ch);
  }
  u16 Block = (ch > 1) ? 0 : ch; // if channel is out of range, let's use ADC0 by default

  if (!XRFdc_IsADCBlockEnabled(RFdcInstPtr, ADC_Tile, Block)) {
    INFO("RF_RFdc: ADC%d is not enabled, make sure the RFdc was initialized before", Block);
    // try another ADC channel
    Block = (Block + 1) % 2;
    if (!XRFdc_IsADCBlockEnabled(RFdcInstPtr, ADC_Tile, Block)) {
      ERROR("RF_RFdc: Couldn't find any enabled ADC channel! returning...");
      return -1;
    } else {
      INFO("RF_RFdc: Using ADC%d instead of requested channel", Block);
    }
  }
  // Define our desired ADC mixer configuration
  XRFdc_Mixer_Settings adcMixerSettings = {
      .CoarseMixFreq  = XRFDC_COARSE_MIX_OFF,   // we are not using a coarse mixer type
      .MixerType      = XRFDC_MIXER_TYPE_FINE,  // we are using a fine mixer type
      .MixerMode      = XRFDC_MIXER_MODE_R2C,   // we will receive a real signal and return an I/Q pair
      .PhaseOffset    = 0,                      // NCO phase = 0
      .FineMixerScale = XRFDC_MIXER_SCALE_AUTO, // the fine mixer scale will be auto updated
      .EventSource    = XRFDC_EVNT_SRC_TILE};
  double freq_in_MHz = freq / 1000000.0;

  // we want our signal to be centered at 2.4576 GHz (NCO freq) -> 2457.6 (Fc) - 1966.08 MHz (Fs) = 491.52 MHz
  if (freq_in_MHz < 2 * RFDC_PLL_FREQ) {
    // positive sign used for frequencies in [0; fs] range, negative in [fs; 2*fs]
    adcMixerSettings.Freq = RFDC_PLL_FREQ - freq_in_MHz;
    INFO("RF_RFdc: configuring ADC Mixer: requested = %f, NCO freq = %f", freq_in_MHz, adcMixerSettings.Freq);
  } else {
    adcMixerSettings.Freq = (2 * RFDC_PLL_FREQ) - freq_in_MHz; // 2xFs (3932.16MHz) - (Fc)
    INFO("RF_RFdc: configuring ADC Mixer: requested = %f, NCO freq = %f", freq_in_MHz, adcMixerSettings.Freq);
  }

  // Set our desired NCO configuration;
  u32 Status = XRFdc_SetMixerSettings(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, &adcMixerSettings);
  if (Status != XRFDC_SUCCESS) {
    ERROR("RFdc: Failed to set ADC NCO settings");
    return -1;
  }

  // Reset NCO phase of the DDC
  XRFdc_ResetNCOPhase(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block);
  // Generate a Tile Event
  XRFdc_UpdateEvent(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, XRFDC_EVENT_MIXER);

  // Print out the configured mixer frequency
  Status = XRFdc_GetMixerSettings(RFdcInstPtr, XRFDC_ADC_TILE, ADC_Tile, Block, &adcMixerSettings);
  if (Status != XRFDC_SUCCESS) {
    ERROR("RFdc: GetMixerSettings failed");
    return -1;
  }
  INFO("RF_RFdc: ADC%d Mixer Frequency set to %.03f", Block, adcMixerSettings.Freq);

  return freq;
}

double rf_xrfdc_set_tx_freq(void* h, uint32_t ch, double freq)
{
  rf_xrfdc_handler_t* handler     = (rf_xrfdc_handler_t*)h;
  XRFdc*              RFdcInstPtr = &handler->RFdcInst;

  // Define our desired DAC mixer configuration
  XRFdc_Mixer_Settings dacMixerSettings = {
      .CoarseMixFreq  = XRFDC_COARSE_MIX_OFF,   // we are not using a coarse mixer type
      .MixerType      = XRFDC_MIXER_TYPE_FINE,  // we are using a fine mixer type
      .MixerMode      = XRFDC_MIXER_MODE_C2R,   // we will send an I/Q pair and forward a real signal
      .PhaseOffset    = 0,                      // NCO phase = 0
      .FineMixerScale = XRFDC_MIXER_SCALE_AUTO, // the fine mixer scale will be auto updated
      .EventSource    = XRFDC_EVNT_SRC_TILE};

  double freq_in_MHz = freq / 1000000.0;
  if (freq_in_MHz < 2 * RFDC_PLL_FREQ) {
    // positive sign used for frequencies in [0; fs] range, negative in [fs; 2*fs]
    dacMixerSettings.Freq = RFDC_PLL_FREQ - freq_in_MHz;
    INFO("RF_RFdc: configuring DAC Mixer: requested = %f, NCO freq = %f", freq_in_MHz, dacMixerSettings.Freq);
  } else {
    dacMixerSettings.Freq = (2 * RFDC_PLL_FREQ) - freq_in_MHz; // 2xFs (3932.16MHz) - (Fc)
    INFO("RF_RFdc: configuring DAC Mixer: requested = %f, NCO freq = %f", freq_in_MHz, dacMixerSettings.Freq);
  }
  dacMixerSettings.Freq = (-1) * dacMixerSettings.Freq; // inverse

  u16 Tile = 1;
  for (u16 Block = 0; Block < 4; Block++) {
    if (!XRFdc_IsDACBlockEnabled(RFdcInstPtr, Tile, Block)) {
      continue;
    }

    // Set our desired NCO configuration;
    int Status = XRFdc_SetMixerSettings(RFdcInstPtr, XRFDC_DAC_TILE, Tile, Block, &dacMixerSettings);
    if (Status != XRFDC_SUCCESS) {
      ERROR("RFdc: Failed to set DAC NCO settings");
      return -1;
    }

    // Reset NCO phase of the DUC
    XRFdc_ResetNCOPhase(RFdcInstPtr, XRFDC_DAC_TILE, Tile, Block);
    // Generate a Tile Event
    XRFdc_UpdateEvent(RFdcInstPtr, XRFDC_DAC_TILE, Tile, Block, XRFDC_EVENT_MIXER);

    // Print out the configured mixer frequency
    XRFdc_Mixer_Settings set_dacMixerSettings = {};
    Status = XRFdc_GetMixerSettings(RFdcInstPtr, XRFDC_DAC_TILE, Tile, Block, &set_dacMixerSettings);
    if (Status != XRFDC_SUCCESS) {
      ERROR("RFdc: GetMixerSettings failed");
      return -1;
    }
    INFO("RF_RFdc: DAC%d Mixer Frequency set to %.03f", Block, set_dacMixerSettings.Freq);
  }
  return freq;
}

srsran_rf_info_t* rf_xrfdc_get_info(void* h)
{
  srsran_rf_info_t* info = NULL;
  if (h != NULL) {
    rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)h;
    info                        = &handler->info;
  }
  return info;
}

static inline bool match_preamble(uint32_t* input)
{
  if (input[0] == common_preamble1 && input[1] == common_preamble2 &&
      input[2] == common_preamble3 && input[3] == time_preamble1 &&
      input[4] == time_preamble2   && input[5] == time_preamble3) {
    return true;
  }
  return false;
}

static void *reader_thread(void *arg)
{
  uint32_t nof_timestamping_errors = 0;
  uint32_t nof_overflow_errors     = 0;
  rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) arg;
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  while(!handler->rx_streamer.stream_active) {
    pthread_cond_wait(&handler->rx_streamer.stream_cvar, &handler->rx_streamer.stream_mutex);
  }

  if(!buffer_initialized(&handler->rx_streamer)) {
    if (srs_dma_allocate_buffers(&handler->rx_streamer._buf,
                                 rx_data_buffer_size + handler->rx_streamer.metadata_samples) < 0) {
      ERROR("RF_RFdc: Failed to create DMA buffer of length %d. Can not start streaming\n",
            rx_data_buffer_size + handler->rx_streamer.metadata_samples);
      goto exit;
    }
    srs_dma_start_streaming(&handler->rx_streamer._buf);
    srsran_ringbuffer_reset(&handler->rx_streamer.ring_buffer);
  }

  handler->rx_streamer.thread_completed = false;
  pthread_cond_signal(&handler->rx_streamer.stream_cvar);
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);

  tx_header_t header = {};

  while (handler->rx_streamer.stream_active) {
    int buffer_ret = refill_buffer(&handler->rx_streamer, &handler->rx_streamer.buf_count);
    if (buffer_ret <= 0) {
      /* If stream is not active, no need to report an error,
       * as we are just cancelling the thread (probably because of changing sample rate, or switching to FPGA processing)
       */
      if (handler->rx_streamer.stream_active) {
        ERROR("RF_RFdc: Error refilling buf %d\n",(int) buffer_ret);
        usleep(1000);
      }
      continue;
    }
    uintptr_t src_ptr                = (uintptr_t) srs_dma_get_data_ptr(&handler->rx_streamer._buf);
    header.magic                     = PKT_HEADER_MAGIC;
    handler->rx_streamer.buf_count   = handler->rx_streamer.buf_count - handler->rx_streamer.metadata_samples;
    header.nof_samples               = handler->rx_streamer.buf_count;
    uint32_t *start_ptr              = (uint32_t*) src_ptr;

    if (handler->use_timestamps) {
      if (!match_preamble(&start_ptr[handler->rx_streamer.preamble_location])) {
        printf("misaligned packet received from the DMA\n");
        nof_timestamping_errors++;
        if (nof_timestamping_errors == 20) {
          break;
        }
        continue;
      }
      uint64_t* tstamp = (uint64_t*)&(start_ptr[handler->rx_streamer.preamble_location + 6]);
      header.timestamp = *tstamp;
#ifdef PRINT_TIMESTAMPS
      time_t secs;
      double frac_secs;
      tstamp_to_time_iio(handler, header.timestamp, &secs, &frac_secs);

      struct timeval time;
      gettimeofday(&time, NULL);
      if(firstGo < 5) {
        if(frac_secs && secs) {
          printf("rec sec %lu frac %f or %lu ticks  [%4d] [%d] \n", secs, frac_secs, header.timestamp, time.tv_usec,time.tv_sec);
        }
      }
#endif
    }
    srsran_ringbuffer_write(&handler->rx_streamer.ring_buffer, &header, sizeof(tx_header_t));

    uint16_t* buf_ptr_tmp = (uint16_t*) src_ptr;
    uint16_t* buf_ptr =
        &buf_ptr_tmp[handler->rx_streamer.metadata_samples * handler->rx_streamer._buf.sample_size / sizeof(uint16_t)];

    int ret = srsran_ringbuffer_write(
            &handler->rx_streamer.ring_buffer,
            buf_ptr, 2 * sizeof(uint16_t) * handler->rx_streamer.buf_count * handler->rx_streamer.nof_channels);

    if (ret < 2 * sizeof(uint16_t) * handler->rx_streamer.buf_count * handler->rx_streamer.nof_channels) {
      ERROR("RF_RFdc: Error writing to buffer in rx thread, ret is %d but should be %d", ret,
             (int)(2 * sizeof(uint16_t) * handler->rx_streamer.buf_count * handler->rx_streamer.nof_channels));
      nof_overflow_errors++;
      if (nof_overflow_errors == 20) {
        break;
      }
    }
  }
exit:
  pthread_mutex_lock(&handler->rx_streamer.stream_mutex);
  handler->rx_streamer.thread_completed = true;
  pthread_cond_signal(&handler->rx_streamer.stream_cvar);
  pthread_mutex_unlock(&handler->rx_streamer.stream_mutex);
  if (nof_timestamping_errors || nof_overflow_errors) {
    printf("stopping RF rx stream because of errors\n");
    stop_rx_stream(handler);
    srsran_ringbuffer_stop(&handler->rx_streamer.ring_buffer);
  }
  return NULL;
}

int rf_xrfdc_recv_with_time(void* h, void* data, uint32_t nsamples, bool blocking, time_t* secs, double* frac_secs)
{
  return rf_xrfdc_recv_with_time_multi(h, &data, nsamples, blocking, secs, frac_secs);
}

int rf_xrfdc_recv_with_time_multi(void*            h,
                                  void**           data,
                                  uint32_t         nsamples,
                                  bool             blocking,
                                  time_t*          secs,
                                  double*          frac_secs)
{
  rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)h;

  size_t rxd_samples_total = 0;
  int    trials            = 0;
  cf_t*  data_ptr          = data[0];

  while (rxd_samples_total < nsamples && trials < 100) {
    if (!handler->rx_streamer.prev_header.nof_samples) {
      int ret = srsran_ringbuffer_read_timed(
          &handler->rx_streamer.ring_buffer, &handler->rx_streamer.prev_header, sizeof(tx_header_t), 1000);
      if (ret <= 0) {
        ERROR("RF_RFdc: Error reading RX ringbuffer");
        if (!ret) {
          // sleep in case the ringbuffer is not active (it is probably being reconfigured)
          usleep(500);
        }
        return SRSRAN_ERROR;
      }
      if (handler->rx_streamer.prev_header.magic != PKT_HEADER_MAGIC) {
        ERROR("RF_RFdc: Error reading rx ringbuffer, invalid header (ret=%d)", ret);
        srsran_ringbuffer_reset(&handler->rx_streamer.ring_buffer);
        return SRSRAN_ERROR;
      }
    }

    uint32_t read_samples = SRSRAN_MIN(handler->rx_streamer.prev_header.nof_samples, nsamples - rxd_samples_total);

    int nof_read_samples = srsran_ringbuffer_read_timed(
        &handler->rx_streamer.ring_buffer,
        (void*) &handler->rx_streamer._conv_buffer[2 * rxd_samples_total * handler->rx_streamer.nof_channels],
        2 * sizeof(uint16_t) * read_samples * handler->rx_streamer.nof_channels,
        1000);

    if (nof_read_samples < 0) {
      ERROR("Error reading samples from ringbuffer");
      return SRSRAN_ERROR;
    }
    handler->rx_streamer.prev_header.nof_samples -= read_samples;

    if (read_samples != nsamples) {
      handler->rx_streamer.prev_header.timestamp -= rxd_samples_total;
    }

    rxd_samples_total += read_samples;
    trials++;
  }
  hw_tstamp_to_time(handler, handler->rx_streamer.prev_header.timestamp, secs, frac_secs);
#ifdef PRINT_TIMESTAMPS
  struct timeval time;
  gettimeofday(&time, NULL);
  if (frac_secs && secs) {
    // INFO("receive samples sec %lu frac %f or %lu ticks  [%4d] [%d] \n", *secs, *frac_secs,
    // handler->rx_streamer.prev_header.timestamp, time.tv_usec,time.tv_sec);
    INFO("receive timestamp = %.6lf secs, or %lu ticks\n",
         (double)*secs + *frac_secs,
         handler->rx_streamer.prev_header.timestamp);
  }
#endif
  srsran_vec_convert_if(&handler->rx_streamer._conv_buffer[0], 32768, (float*)data_ptr, 2 * rxd_samples_total);

  if (handler->rx_streamer.nof_channels > 1) {
    data_ptr = data[1];
    srsran_vec_convert_if(
        &handler->rx_streamer._conv_buffer[2 * rxd_samples_total], 32768, (float*)data_ptr, 2 * rxd_samples_total);
  }
  // INFO("RX timestamp = %lu \n", handler->rx_streamer.prev_header.timestamp);
  return (int)nsamples;
}

void check_late_register(void* h, uint32_t* late_reg_value)
{
  rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)h;
  // BA + 0x380
  *late_reg_value = handler->memory_map_ptr[224];
}
/*
static uint64_t get_current_hw_clock(void* h)
{
  rf_xrfdc_handler_t* handler  = (rf_xrfdc_handler_t*)h;
  uint64_t            low_reg  = handler->memory_map_ptr[229];
  uint64_t            high_reg = handler->memory_map_ptr[230];
  return ((high_reg << 32u) | low_reg);
}*/

static int send_buf(void *h, size_t sample_size)
{
  rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)h;

  int total_tx_size = handler->tx_streamer.items_in_buffer * sample_size +
                      handler->tx_streamer.metadata_samples * 4u;

  int ret = srs_dma_send_data(&handler->tx_streamer._buf, total_tx_size);

  handler->tx_streamer.items_in_buffer = 0;
  return ret;
}

static void *writer_thread(void *arg)
{
  rf_xrfdc_handler_t *handler = (rf_xrfdc_handler_t*) arg;

  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

  int      read_samples   = 0;
  uint64_t timestamp      = 0;
  bool     have_timestamp = false;
  size_t   sample_size    = sample_size = 2 * sizeof(uint16_t); // size of a quantized IQ pair

  pthread_mutex_lock(&handler->tx_streamer.stream_mutex);
  while(!handler->tx_streamer.stream_active) {
    pthread_cond_wait(&handler->tx_streamer.stream_cvar, &handler->tx_streamer.stream_mutex);
  }
  pthread_mutex_unlock(&handler->tx_streamer.stream_mutex);

  while (handler->tx_streamer.stream_active) {
    int n = 0;
    do {
      uint32_t* start_ptr  = (uint32_t*)srs_dma_get_data_ptr(&handler->tx_streamer._buf);
      uint64_t* tstamp_ptr = (uint64_t*)srs_dma_get_data_ptr(&handler->tx_streamer._buf);

      uintptr_t dst_ptr =
          (uintptr_t)srs_dma_get_data_ptr(&handler->tx_streamer._buf) +
          (handler->tx_streamer.metadata_samples + handler->tx_streamer.items_in_buffer) * 2 * sizeof(int16_t);

      if(!handler->tx_streamer.prev_header.nof_samples) {
        if (srsran_ringbuffer_read(
                &handler->tx_streamer.ring_buffer, &handler->tx_streamer.prev_header, sizeof(tx_header_t)) < 0) {
          fprintf(stderr,"Error reading buffer\n");
        }
        if (handler->tx_streamer.prev_header.magic != PKT_HEADER_MAGIC) {
          fprintf(stderr, "Error reading tx ringbuffer. Invalid header\n");
          srsran_ringbuffer_reset(&handler->tx_streamer.ring_buffer);
        }
        if (!have_timestamp) {
          timestamp = handler->tx_streamer.prev_header.timestamp;
          if (timestamp != 0) {
            timestamp -= handler->tx_streamer.items_in_buffer;
          }
          have_timestamp = true;
        }
      }

      unsigned space_left = (handler->tx_streamer.buffer_size - handler->tx_streamer.items_in_buffer);
      read_samples        = SRSRAN_MIN(handler->tx_streamer.prev_header.nof_samples, space_left);

      if (read_samples > 0) {
        if (srsran_ringbuffer_read(&handler->tx_streamer.ring_buffer, (void*)dst_ptr, sample_size * read_samples) < 0) {
          ERROR("Error reading samples from TX ringbuffer");
          return NULL;
        }
      }
      bool end_of_burst = (read_samples == 0);
      if (!handler->tx_streamer.items_in_buffer && !n && end_of_burst) {
        have_timestamp = false;
        continue;
      }

      handler->tx_streamer.items_in_buffer += read_samples;
      handler->tx_streamer.prev_header.nof_samples -= read_samples;
      n += read_samples;
      end_of_burst = handler->tx_streamer.prev_header.end_of_burst;
      //INFO("RF_RFdc: n=%d, read_samples=%d, end=%d", n, read_samples, end_of_burst);

      if ((handler->tx_streamer.items_in_buffer == handler->tx_streamer.buffer_size) || end_of_burst) {
        if (!have_timestamp) {
          if (timestamp != 0) {
            timestamp += handler->tx_streamer.items_in_buffer;
          }
        }
        have_timestamp = false;

        /// Add packet header
        unsigned dma_length_bytes =
            (handler->tx_streamer.items_in_buffer + handler->tx_streamer.metadata_samples) * 4u - 1u;
        start_ptr[0] = common_preamble1;
        start_ptr[1] = common_preamble2;
        start_ptr[2] = common_preamble3_short | (dma_length_bytes << 16u);
        start_ptr[3] = time_preamble1;
        start_ptr[4] = time_preamble2;
        start_ptr[5] = time_preamble3;
        // last words of packet header store the timestamp
        tstamp_ptr[3] = (handler->use_timestamps) ? timestamp : 0;
#if PRINT_TIMESTAMPS
        time_t secs;
        double frac_secs;
        struct timeval time;
        gettimeofday(&time, NULL);
        tstamp_to_time_iio(handler, *tstamp_ptr, &secs, &frac_secs);
        if(firstGo < 20) {
          printf("send sec %d frac %f or %d ticks  [%4d] [%d] \n",secs,frac_secs,timestamp, time.tv_usec,time.tv_sec);
          firstGo++;
        }
#endif
        /// Submit buffer to DMA engine
        int ret_buf = send_buf((void*)handler, sample_size);
        // uint64_t hw_time = get_current_hw_clock(handler);
        // INFO("RF_RFdc: pushed TS=%lu, current fpga time = %lu", timestamp, hw_time);

        if (end_of_burst) {
          n = handler->tx_streamer.buffer_size;
        }
        if (ret_buf) {
          handler->tx_streamer.items_in_buffer = 0;
        }
        uint32_t late_reg_value = 0;
        if(handler->memory_map_ptr) {
          check_late_register(handler, &late_reg_value);
        }
        if (late_reg_value) {
          lates++;
          INFO("FPGA: L");
          if (lates > 5) {
            log_late(handler, false);
            lates = 0;
          }
        }
      }
    } while(n < handler->tx_streamer.buffer_size);
  }
  handler->tx_streamer.thread_completed = true;
  return NULL;
}

int rf_xrfdc_send_timed(void*              h,
                        void*              data,
                        int                nsamples,
                        time_t             secs,
                        double             frac_secs,
                        bool               has_time_spec,
                        bool               blocking,
                        bool               is_start_of_burst,
                        bool               is_end_of_burst)
{
  void* _data[SRSRAN_MAX_PORTS] = {data, zero_mem, zero_mem, zero_mem};
  return rf_xrfdc_send_timed_multi(
      h, _data, nsamples, secs, frac_secs, has_time_spec, blocking, is_start_of_burst, is_end_of_burst);
}

int rf_xrfdc_send_timed_multi(void*              h,
                              void**             data,
                              int                nsamples,
                              time_t             secs,
                              double             frac_secs,
                              bool               has_time_spec,
                              bool               blocking,
                              bool               is_start_of_burst,
                              bool               is_end_of_burst)
{
  tx_header_t         header  = {};
  rf_xrfdc_handler_t* handler = (rf_xrfdc_handler_t*)h;

  if (!handler->tx_streamer.stream_active) {
    rf_xrfdc_start_tx_stream(h);
  }
#ifdef PRINT_TIMESTAMPS
  struct timeval time;
  gettimeofday(&time, NULL);
  if (firstGo < 5) {
    printf("init send sec %d frac %f [%4d] [%d] \n", secs, frac_secs, time.tv_usec, time.tv_sec);
    firstGo++;
  }
#endif
  int n      = 0;
  int trials = 0;

  do {
    float* samples_cf32 = (float*)&(((cf_t**)data)[0][n]);

    srsran_vec_convert_fi(samples_cf32, 32767.999f, handler->tx_streamer._conv_buffer, 2 * nsamples);

    header.magic        = PKT_HEADER_MAGIC;
    header.nof_samples  = nsamples;
    header.timestamp    = time_to_hw_tstamp(handler, secs, frac_secs);
    header.end_of_burst = is_end_of_burst;

    srsran_ringbuffer_write_block(&handler->tx_streamer.ring_buffer, &header, sizeof(tx_header_t));
    // Each sample is a pair of quantized 16bit values, i.e. I and Q
    srsran_ringbuffer_write_block(
        &handler->tx_streamer.ring_buffer, (void*)(handler->tx_streamer._conv_buffer), sizeof(uint16_t) * 2 * nsamples);

    n += nsamples;
    trials++;
  } while (n < nsamples && trials < 100);
  INFO("RF_RFdc: sent %d samples", nsamples);
  return n;
}

rf_dev_t srsran_rf_dev_rfdc = {
        "RFdc",
        rf_xrfdc_devname,
        rf_xrfdc_start_rx_stream,
        rf_xrfdc_stop_rx_stream,
        NULL,
        rf_xrfdc_has_rssi,
        rf_xrfdc_get_rssi,
        rf_xrfdc_suppress_stdout,
        rf_xrfdc_register_error_handler,
        rf_xrfdc_open,
        rf_xrfdc_open_multi,
        rf_xrfdc_close,
        rf_xrfdc_set_rx_srate,
        rf_xrfdc_set_rx_gain,
        NULL,
        rf_xrfdc_set_tx_gain,
        NULL,
        rf_xrfdc_get_rx_gain,
        rf_xrfdc_get_tx_gain,
        rf_xrfdc_get_info,
        rf_xrfdc_set_rx_freq,
        rf_xrfdc_set_tx_srate,
        rf_xrfdc_set_tx_freq,
        NULL,
        NULL,
        rf_xrfdc_recv_with_time,
        rf_xrfdc_recv_with_time_multi,
        rf_xrfdc_send_timed,
        .srsran_rf_send_timed_multi = rf_xrfdc_send_timed_multi
};

int register_plugin(rf_dev_t** rf_api)
{
  if (rf_api == NULL) {
    return SRSRAN_ERROR;
  }
  *rf_api = &srsran_rf_dev_rfdc;
  return SRSRAN_SUCCESS;
}