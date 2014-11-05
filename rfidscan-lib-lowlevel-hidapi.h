#include "hidapi.h"

int rfidscan_enumerate(void)
{
  LOG("rfidscan_enumerate!\n");
  rfidscan_cached_count = 0;
  rfidscan_enumerateByVidPid(0x1C34, 0x7241); /* Prox'N'Roll RFID Scanner */
  rfidscan_enumerateByVidPid(0x1C34, 0xB241); /* Prox'N'Roll RFID Scanner HSP */    
  return rfidscan_cached_count;
}

// get all matching devices by VID/PID pair
int rfidscan_enumerateByVidPid(int vid, int pid)
{
    struct hid_device_info *devs, *cur_dev;

    int i, count=0; 
    devs = hid_enumerate(vid, pid);
    cur_dev = devs;    
    while (cur_dev) {
        if( (cur_dev->vendor_id != 0 && cur_dev->product_id != 0) &&  
            (cur_dev->vendor_id == vid && cur_dev->product_id == pid) ) { 
            if( cur_dev->serial_number != NULL ) { // can happen if not root
                uint32_t serialnum;
                strcpy( rfidscan_infos[rfidscan_cached_count+count].path,   cur_dev->path );
                sprintf( rfidscan_infos[rfidscan_cached_count+count].serial, "%ls", cur_dev->serial_number);
                //wcscpy( rfidscan_infos[rfidscan_cached_count+count].serial, cur_dev->serial_number );
                //uint32_t sn = wcstol( cur_dev->serial_number, NULL, 16);
                serialnum = strtol( rfidscan_infos[rfidscan_cached_count+count].serial, NULL, 16);
                rfidscan_infos[rfidscan_cached_count+count].vid = vid;
                rfidscan_infos[rfidscan_cached_count+count].pid = pid;

                count++;
            }
        }
        cur_dev = cur_dev->next;
    }
    hid_free_enumeration(devs);

    LOG("rfidscan_enumerateByVidPid: done, %d devices found\n", count);
    for( i=0; i<count; i++ ) { 
        LOG("rfidscan_enumerateByVidPid: rfidscan_infos[%d].serial=%s\n",
            i, rfidscan_infos[rfidscan_cached_count+i].serial);
    }
    rfidscan_cached_count += count;
    rfidscan_sortCache();

    return count;
}

//
rfidscan_device* rfidscan_openByPath(const char* path)
{
  int i;
  rfidscan_device* handle;

    if( path == NULL || strlen(path) == 0 ) return NULL;

    LOG("rfidscan_openByPath: %s\n", path);

    handle = hid_open_path( path ); 

    i = rfidscan_getCacheIndexByPath( path );
    if( i >= 0 ) {  // good
        rfidscan_infos[i].dev = handle;
    }
    else { // uh oh, not in cache, now what?
    }
    
    return handle;
}

//
rfidscan_device* rfidscan_openBySerial(const char* serial)
{
    wchar_t wserialstr[serialstrmax] = {L'\0'};
    rfidscan_device* handle;
    int i;

    if( serial == NULL || strlen(serial) == 0 ) return NULL;

    i = rfidscan_getCacheIndexBySerial(serial);

    LOG("rfidscan_openBySerial: %s\n", serial);
    LOG("rfidscan_openBySerial: id=%d\n", i );

#ifdef _WIN32   // omg windows you suck
    swprintf( wserialstr, serialstrmax, L"%S", serial); // convert to wchar_t*
#else
    swprintf( wserialstr, serialstrmax, L"%s", serial); // convert to wchar_t*
#endif

    handle = hid_open(rfidscan_getCachedVid(i), rfidscan_getCachedPid(i), wserialstr ); 
    if( handle ) LOG("rfidscan_openBySerial: got a rfidscan_device handle\n"); 

    i = rfidscan_getCacheIndexBySerial( serial );
    if( i >= 0 ) {
        LOG("rfidscan_openBySerial: good, serial id:%d was in cache\n",i);
        rfidscan_infos[i].dev = handle;
    }
    else { // uh oh, not in cache, now what?
        LOG("rfidscan_openBySerial: uh oh, serial id:%d was NOT IN CACHE\n",i);
    }

    return handle;
}

//
rfidscan_device* rfidscan_openById( uint32_t i ) 
{ 
    LOG("rfidscan_openById: %d \n", i );
    if( i > rfidscan_max_devices ) { // then i is a serial number not an array index
        char serialstr[serialstrmax];
        sprintf( serialstr, "%X", i);  // convert to wchar_t* 
        return rfidscan_openBySerial( serialstr );  
    } 
    else {
        return rfidscan_openByPath( rfidscan_getCachedPath(i) );
    }
}

rfidscan_device* rfidscan_open(void)
{
    rfidscan_enumerate();
    return rfidscan_openById( 0 );
}


//
// FIXME: search through rfidscans list to zot it too?
void rfidscan_close( rfidscan_device* dev )
{
    if( dev != NULL ) {
        rfidscan_clearCacheDev(dev); // FIXME: hmmm 
        hid_close(dev);
    }
    dev = NULL;
    //hid_exit(); // FIXME: this cleans up libusb in a way that hid_close doesn't
}

int rfidscan_exchange(rfidscan_device* dev, unsigned char *buf, int len)
{
  int rc;
  
  if( dev==NULL )
  {
    return -1; // RFIDSCAN_ERR_NOTOPEN;
  }
  rc = hid_send_feature_report( dev, buf, len );
  // FIXME: put this in an ifdef?
  if( rc==-1 )
  {
    LOG("rfidscan_write error: %ls\n", hid_error(dev));
    return rc;
  }
  
  rfidscan_sleep( 5 ); //FIXME:

  LOG("get_feature_report\n");
  
  if( (rc = hid_get_feature_report(dev, buf, len) == -1) )
  {
    LOG("error reading data: %d\n", rc);
    return rc;
  }

  return rc;
}

