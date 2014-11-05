/**
 * blink(1) C library -- aka "rfidscan-lib" 
 *
 * Part of the blink(1) open source hardware project
 * See https://github.com/todbot/rfidscan for details
 *
 * 2012-2014, Tod E. Kurt, http://todbot.com/blog/ , http://thingm.com/
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  // for toupper()

#ifdef _WIN32
#include <windows.h>
#define   swprintf   _snwprintf
#else
#include <unistd.h>    // for usleep()
#endif

#include "rfidscan-lib.h"

static const uint8_t ACTION_GET_PROTOCOL_INFO = 0x01;
static const uint8_t ACTION_GET_VARIABLE = 0x02;
static const uint8_t GET_VARIABLE_ITEM_LAST_ERROR = 0x01;
static const uint8_t ACTION_GET_CONST_ASCII = 0x04;
static const uint8_t GET_CONST_ITEM_VENDOR_NAME = 0x01;
static const uint8_t GET_CONST_ITEM_PRODUCT_NAME = 0x02;
static const uint8_t GET_CONST_ITEM_SERIAL_NUMBER = 0x03;
static const uint8_t GET_CONST_ITEM_PID_VID = 0x04;
static const uint8_t GET_CONST_ITEM_PRODUCT_VERSION = 0x05;
static const uint8_t ACTION_GET_FEED = 0x20;
static const uint8_t ACTION_SET_BEHAVIOUR = 0x80;
static const uint8_t SET_BEHAVIOUR_ITEM_STOP_READER = 0x10;
static const uint8_t SET_BEHAVIOUR_ITEM_START_READER = 0x11;
static const uint8_t SET_BEHAVIOUR_ITEM_STOP_KEYBOARD = 0x20;
static const uint8_t SET_BEHAVIOUR_ITEM_START_KEYBOARD = 0x21;
static const uint8_t SET_BEHAVIOUR_ITEM_APPLY_CONFIG = 0xC0;
static const uint8_t SET_BEHAVIOUR_ITEM_RESET = 0xD0;
static const uint8_t ACTION_SET_LEDS = 0x88;
static const uint8_t ACTION_SET_BUZZER = 0x8A;
static const uint8_t ACTION_SET_FEED = 0xA0;
static const uint8_t ACTION_SET_MIFARE_KEY_E2 = 0xB0;


// rfidscan copy of some hid_device_info and other bits. 
// this seems kinda dumb, though. is there a better way?
typedef struct rfidscan_info_ {
    rfidscan_device* dev;  // device, if opened, NULL otherwise
    char path[pathstrmax];  // platform-specific device path
    char serial[serialstrmax];
    int vid;
    int pid;
} rfidscan_info;

static rfidscan_info rfidscan_infos[cache_max];
static int rfidscan_cached_count = 0;  // number of cached entities

static int rfidscan_enable_degamma = 1;

// set in Makefile to debug HIDAPI stuff
extern int verbose;
#define LOG(...) if (verbose) fprintf(stderr, __VA_ARGS__)

// addresses in EEPROM for mk1 blink(1) devices
#define rfidscan_eeaddr_osccal        0
#define rfidscan_eeaddr_bootmode      1
#define rfidscan_eeaddr_serialnum     2
#define rfidscan_serialnum_len        4
#define rfidscan_eeaddr_patternstart (rfidscan_eeaddr_serialnum + rfidscan_serialnum_len)

void rfidscan_sortCache(void);


//----------------------------------------------------------------------------
// implementation-varying code 

#if USE_HIDDATA
#include "rfidscan-lib-lowlevel-hiddata.h"
#else
//#if USE_HIDAPI
#include "rfidscan-lib-lowlevel-hidapi.h"
#endif
// default to USE_HIDAPI unless specifically told otherwise


// -------------------------------------------------------------------------
// everything below here doesn't need to know about USB details
// except for a "rfidscan_device*"
// -------------------------------------------------------------------------

//
int rfidscan_getCachedCount(void)
{
    return rfidscan_cached_count;
}

//
const char* rfidscan_getCachedPath(int i)
{
    if( i > rfidscan_getCachedCount()-1 ) return NULL;
    return rfidscan_infos[i].path;
}
//
const char* rfidscan_getCachedSerial(int i)
{
    if( i > rfidscan_getCachedCount()-1 ) return NULL;
    return rfidscan_infos[i].serial;
}

int rfidscan_getCachedVid(int i)
{
    if( i > rfidscan_getCachedCount()-1 ) return 0;
    return rfidscan_infos[i].vid;
}

int rfidscan_getCachedPid(int i)
{
    if( i > rfidscan_getCachedCount()-1 ) return 0;
    return rfidscan_infos[i].pid;
}

int rfidscan_getCacheIndexByPath( const char* path ) 
{
  int i;
    for( i=0; i< cache_max; i++ ) { 
        if( strcmp( rfidscan_infos[i].path, (const char*) path ) == 0 ) return i;
    }
    return -1;
}

int rfidscan_getCacheIndexById( uint32_t i )
{
    if( i > rfidscan_max_devices ) { // then i is a serial number not an array index
        char serialstr[serialstrmax];
        sprintf( serialstr, "%X", i);  // convert to wchar_t* 
        return rfidscan_getCacheIndexBySerial( serialstr );
    }
    return i;
}

int rfidscan_getCacheIndexBySerial( const char* serial ) 
{
  int i;
    for( i=0; i< cache_max; i++ ) { 
        if( strcmp( rfidscan_infos[i].serial, serial ) == 0 ) return i;
    }
    return -1;
}

int rfidscan_getCacheIndexByDev( rfidscan_device* dev ) 
{
  int i;
    for( i=0; i< cache_max; i++ ) { 
        if( rfidscan_infos[i].dev == dev ) return i;
    }
    return -1;
}

const char* rfidscan_getSerialForDev(rfidscan_device* dev)
{
    int i = rfidscan_getCacheIndexByDev( dev );
    if( i>=0 ) return rfidscan_infos[i].serial;
    return NULL;
}

int rfidscan_clearCacheDev( rfidscan_device* dev ) 
{
    int i = rfidscan_getCacheIndexByDev( dev );
    if( i>=0 ) rfidscan_infos[i].dev = NULL; // FIXME: hmmmm
    return i;
}

int rfidscan_get(rfidscan_device *dev, uint8_t action, uint8_t item, uint8_t data[], size_t max_size)
{
  uint8_t buf[rfidscan_buf_size];
  uint8_t i;
  int rc;
    
  memset(buf, 0x00, sizeof(buf));
  buf[0] = rfidscan_report_id;
  buf[1] = 3;
  buf[2] = 0; /* Sequence number, could be incremented after each call */
  buf[3] = (uint8_t) (action & 0x7F);
  buf[4] = item;
   
  rc = rfidscan_exchange(dev, buf, sizeof(buf)); 

  LOG("in get, exchange rc=%d\n", rc);

  if (rc < 0)
    return rc;

  if (buf[2] != 0)
  {
    rc = 0 - buf[2];
    LOG("error raised by the reader: %d\n", rc);
    return rc;
  }

  if (buf[1] > 3)
  {
    rc = buf[1] - 3;
  
    if (max_size > rc)
      max_size = rc;
      
    if (data != NULL)
      for (i=0; i<max_size; i++)    
        data[i] = buf[5+i];
  }

  return rc;
}

int rfidscan_get_string(rfidscan_device *dev, uint8_t action, uint8_t item, char *data, size_t max_size)
{
  int rc;

  rc = rfidscan_get(dev, action, item, (uint8_t *) data, max_size);

  if (rc <= 0)
  {
    if (data != NULL)
      data[0] = '\0';
  } else
  {
    if (rc > max_size)
      data[max_size-1] = '\0';
    else
      data[rc] = '\0';
  }
  return rc;
}

int rfidscan_set(rfidscan_device *dev, uint8_t action, uint8_t item, uint8_t data[], uint8_t datalen)
{
  uint8_t buf[rfidscan_buf_size];
  uint8_t i;
  int rc;

  if ((data == NULL) && (datalen != 0))  
    return -1;
  if ((data != NULL) && (datalen > 60))
    return -1;
    
  buf[0] = rfidscan_report_id;
  buf[1] = 3 + datalen;
  buf[2] = 0; /* Sequence number, could be incremented after each call */
  buf[3] = (uint8_t) (0x80 | action);
  buf[4] = item;
  
  for (i=0; i<datalen; i++)
    buf[5+i] = data[i];
  
  rc = rfidscan_exchange(dev, buf, sizeof(buf));
  if (rc > 0)
      rc = 0 - buf[3];
      
  return rc;
}

int rfidscan_Reset(rfidscan_device *dev)
{
  uint8_t buf[1];
  
  buf[0] = SET_BEHAVIOUR_ITEM_RESET;

  return rfidscan_set(dev, ACTION_SET_BEHAVIOUR, 0, buf, sizeof(buf));
}

int rfidscan_ApplyConfig(rfidscan_device *dev)
{
  uint8_t buf[1];
  
  buf[0] = SET_BEHAVIOUR_ITEM_APPLY_CONFIG;

  return rfidscan_set(dev, ACTION_SET_BEHAVIOUR, 0, buf, sizeof(buf));
}

int rfidscan_setBuzzer(rfidscan_device *dev, uint16_t duration)
{
  uint8_t buf[2];
  
  buf[0] = (uint8_t) (duration / 0x0100);
  buf[1] = (uint8_t) (duration);

  return rfidscan_set(dev, ACTION_SET_BUZZER, 0, buf, sizeof(buf));
}

int rfidscan_setLedsP(rfidscan_device *dev, uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t buf[4];
  
  buf[0] = r;
  buf[1] = g;
  buf[2] = b;
  buf[3] = 0;

  return rfidscan_set(dev, ACTION_SET_LEDS, 0, buf, sizeof(buf));
}

int rfidscan_setLedsT(rfidscan_device *dev, uint8_t r, uint8_t g, uint8_t b, uint16_t duration)
{
  uint8_t buf[6];
  
  buf[0] = r;
  buf[1] = g;
  buf[2] = b;
  buf[3] = 0;

  buf[4] = (uint8_t) (duration / 0x0100);
  buf[5] = (uint8_t) (duration);

  return rfidscan_set(dev, ACTION_SET_LEDS, 0, buf, sizeof(buf));
}

int rfidscan_getVendorName(rfidscan_device *dev, char *data, size_t max_size)
{
  return rfidscan_get_string(dev, ACTION_GET_CONST_ASCII, GET_CONST_ITEM_VENDOR_NAME, data, max_size);
}

int rfidscan_getProductName(rfidscan_device *dev, char *data, size_t max_size)
{
  return rfidscan_get_string(dev, ACTION_GET_CONST_ASCII, GET_CONST_ITEM_PRODUCT_NAME, data, max_size);
}

int rfidscan_getSerialNumber(rfidscan_device *dev, char *data, size_t max_size)
{
  return rfidscan_get_string(dev, ACTION_GET_CONST_ASCII, GET_CONST_ITEM_SERIAL_NUMBER, data, max_size);
}

int rfidscan_getVersion(rfidscan_device *dev, char *data, size_t max_size)
{
  return rfidscan_get_string(dev, ACTION_GET_CONST_ASCII, GET_CONST_ITEM_PRODUCT_VERSION, data, max_size);
}

int rfidscan_RegisterWrite(rfidscan_device *dev, uint8_t addr, uint8_t buffer[], size_t size)
{
  return rfidscan_set(dev, ACTION_SET_FEED, addr, buffer, size);
}

int rfidscan_RegisterErase(rfidscan_device *dev, uint8_t addr)
{
  return rfidscan_set(dev, ACTION_SET_FEED, addr, NULL, 0);
}

int rfidscan_RegisterRead(rfidscan_device *dev, uint8_t addr, uint8_t buffer[], size_t max_size)
{
  return rfidscan_get(dev, ACTION_GET_FEED, addr, buffer, max_size);
}


// qsort char* string comparison function 
int cmp_rfidscan_info_serial(const void *a, const void *b) 
{ 
    rfidscan_info* bia = (rfidscan_info*) a;
    rfidscan_info* bib = (rfidscan_info*) b;

    return strncmp( bia->serial, 
                    bib->serial, 
                    serialstrmax);
} 

void rfidscan_sortCache(void)
{
    size_t elemsize = sizeof( rfidscan_info ); //  
    
    qsort( rfidscan_infos, 
           rfidscan_cached_count, 
           elemsize, 
           cmp_rfidscan_info_serial);
}


// simple cross-platform millis sleep func
void rfidscan_sleep(uint16_t millis)
{
#ifdef WIN32
            Sleep(millis);
#else 
            usleep( millis * 1000);
#endif
}


