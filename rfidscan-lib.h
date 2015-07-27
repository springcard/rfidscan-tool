/**
 * blink(1) C library -- aka "rfidscan-lib" 
 *
 * Part of the blink(1) open source hardware project
 * See https://github.com/todbot/rfidscan for details
 *
 * 2012-2014, Tod E. Kurt, http://todbot.com/blog/ , http://thingm.com/
 *
 */


#ifndef __RFIDSCAN_LIB_H__
#define __RFIDSCAN_LIB_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define rfidscan_max_devices 16

#define cache_max 16  
#define serialstrmax (8 + 1) 
#define pathstrmax 128

#define rfidscan_report_id  0
#define rfidscan_report_size 64
#define rfidscan_buf_size (rfidscan_report_size+1)

struct rfidscan_device_;

#if USE_HIDAPI
typedef struct hid_device_ rfidscan_device; /**< opaque rfidscan structure */
#elif USE_HIDDATA
typedef struct usbDevice   rfidscan_device; /**< opaque rfidscan structure */
#else
typedef struct hid_device_ rfidscan_device; /**< opaque rfidscan structure */
#endif


//
// -------- BEGIN PUBLIC API ----------
//

/**
 * Scan USB for blink(1) devices.
 * @return number of devices found
 */
int          rfidscan_enumerate();

/**
 * Scan USB for devices by given VID,PID.
 * @param vid vendor ID
 * @param pid product ID
 * @return number of devices found
 */
int          rfidscan_enumerateByVidPid(int vid, int pid);

/**
 * Open first found blink(1) device.
 * @return pointer to opened rfidscan_device or NULL if no rfidscan found
 */
rfidscan_device* rfidscan_open(void);

/**
 * Open blink(1) by USB path.
 * note: this is platform-specific, and port-specific.
 * @param path string of platform-specific path to rfidscan
 * @return rfidscan_device or NULL if no rfidscan found
 */ 
rfidscan_device* rfidscan_openByPath(const char* path);

/**
 * Open blink(1) by 8-digit serial number.
 * @param serial 8-hex digit serial number
 * @return rfidscan_device or NULL if no rfidscan found
 */
rfidscan_device* rfidscan_openBySerial(const char* serial);

/**
 * Open by "id", which if from 0-rfidscan_max_devices is index
 *  or if >rfidscan_max_devices, is numerical representation of serial number
 * @param id ordinal 0-15 id of rfidscan or numerical rep of 8-hex digit serial
 * @return rfidscan_device or NULL if no rfidscan found
 */
rfidscan_device* rfidscan_openById( uint32_t id );

/**
 * Close opened rfidscan device.  
 * Safe to call rfidscan_close on already closed device.
 * @param dev rfidscan_device
 */
void rfidscan_close( rfidscan_device* dev );

/**
 * Low-level communication with rfidscan device.
 * Used internally by rfidscan-lib
 */
int rfidscan_exchange(rfidscan_device* dev, uint8_t *buf, int len);

int rfidscan_getVendorName(rfidscan_device *dev, char *data, size_t max_size);
int rfidscan_getProductName(rfidscan_device *dev, char *data, size_t max_size);
int rfidscan_getSerialNumber(rfidscan_device *dev, char *data, size_t max_size);

/**
 * Get rfidscan firmware version.
 * @param dev opened rfidscan device
 * @return version as scaled int number (e.g. "v1.1" = 101)
 */
int rfidscan_getVersion(rfidscan_device *dev, char *data, size_t max_size);

int rfidscan_setBuzzer(rfidscan_device *dev, uint16_t duration);
int rfidscan_setLedsP(rfidscan_device *dev, uint8_t r, uint8_t g, uint8_t b);
int rfidscan_setLedsT(rfidscan_device *dev, uint8_t r, uint8_t g, uint8_t b, uint16_t duration);

/** 
 * Read register from FEED
 */
int rfidscan_RegisterRead(rfidscan_device *dev, uint8_t addr, uint8_t *data, size_t max_size);

/** 
 * Write register into FEED
 */
int rfidscan_RegisterWrite(rfidscan_device *dev, uint8_t addr, uint8_t buffer[], size_t size);


int rfidscan_RegisterReset(rfidscan_device *dev);
int rfidscan_ApplyConfig(rfidscan_device *dev);


/**
 * Simple wrapper for cross-platform millisecond delay.
 * @param delayMillis number of milliseconds to wait
 */
void rfidscan_sleep(uint16_t delayMillis);

/**
 * Return platform-specific USB path for given cache index.
 * @param i cache index
 * @return path string
 */
const char*  rfidscan_getCachedPath(int i);
/**
 * Return bilnk1 serial number for given cache index.
 * @param i cache index
 * @return 8-hexdigit serial number as string
 */
const char*  rfidscan_getCachedSerial(int i);

int rfidscan_getCachedVid(int i);
int rfidscan_getCachedPid(int i);

/**
 * Return cache index for a given platform-specific USB path.
 * @param path platform-specific path string
 * @return cache index or -1 if not found
 */
int          rfidscan_getCacheIndexByPath( const char* path );
/**
 * Return cache index for a given rfidscan id (0-max or serial number as uint32)
 * @param i rfidscan id (0-rfidscan_max_devices or serial as uint32)
 * @return cache index or -1 if not found
 */
int          rfidscan_getCacheIndexById( uint32_t i );
/**
 * Return cache index for a given rfidscan serial number.
 * @param path platform-specific path string
 * @return cache index or -1 if not found
 */
int          rfidscan_getCacheIndexBySerial( const char* serial );
/**
 * Return cache index for a given rfidscan_device object.
 * @param dev rfidscan device to lookup
 * @return cache index or -1 if not found
 */
int          rfidscan_getCacheIndexByDev( rfidscan_device* dev );
/**
 * Clear the rfidscan device cache for a given device.
 * @param dev rfidscan device 
 * @return cache index that was cleared, or -1 if not found
 */
int          rfidscan_clearCacheDev( rfidscan_device* dev );

/**
 * Return serial number string for give rfidscan device.
 * @param dev blink device to lookup
 * @return 8-hexdigit serial number string
 */
const char*  rfidscan_getSerialForDev(rfidscan_device* dev);
/**
 * Return number of entries in rfidscan device cache.
 * @note This is the number of devices found with rfidscan_enumerate()
 * @return number of cache entries
 */
int          rfidscan_getCachedCount(void);


extern int rfidscan_verbose;

#ifdef __cplusplus
}
#endif

#endif
