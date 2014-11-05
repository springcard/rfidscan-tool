/* 
*
*/

#include <stdio.h>
#include <stdarg.h>    // vararg stuff
#include <string.h>    // for memset(), strcmp(), et al
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>    // for getopt_long()

#ifndef WIN32
#include <unistd.h>
#define stricmp strcasecmp
#endif

#ifdef WIN32
#include <Windows.h>
#define sleep(s) Sleep(1000*s)
#endif

#include "rfidscan-lib.h"

int verbose = 0;
int quiet = 0;

// printf that can be shut up
void msg(char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  if( !quiet ) {
    vprintf(fmt,args);
  }
  va_end(args);
}

static uint8_t htoq(char q)
{
  return (((q >= '0') && (q <= '9')) ? (q - '0') : (((q >= 'A') && (q <= 'F')) ? (q + 10 - 'A') : (((q >= 'a') && (q <= 'f')) ? (q + 10 - 'a') : 0)));
}

static uint8_t htob(const char s[2])
{
  uint8_t i;
  uint8_t r = 0;
  
  for (i=0; i<2; i++)
  {
    r <<= 4;
    r  |= htoq(s[i]);
  }
  
  return r;
}

static int hstob(const char *str, uint8_t *data, int size)
{
  int len = 0;
  int offset = 0;
  
  /* Left trim */
  while ((str[offset] == '=') || (str[offset] == ':') || (str[offset] == ' ') || (str[offset] == '\t'))
    offset += 1;

  if (str[offset] == '@')
  {
    /* ASCII mode ? */
    offset += 1;
    while (str[offset] != '\0')
    {
      data[len++] = str[offset];
      if (len >= size) break;
      offset += 1;
    }
  } else
  {
    /* Hexadecimal mode */
    while ((str[offset] != '\0') && (str[offset+1] != '\0'))
    {
      data[len++] = htob(&str[offset]);
      if (len >= size) break;
      offset += 2;
      while ((str[offset] == ' ') || (str[offset] == '\t') || (str[offset] == '.')  || (str[offset] == ':'))
        offset += 1;
    }
  }
  
  return len;
}

//
static int do_test(rfidscan_device *dev)
{
  int rc;

  msg("Test...\n");

  msg("\tRed LED\n");
  rc = rfidscan_setLedsP(dev, 1, 0, 0);
  if (rc < 0)
    return rc;

  sleep(1);

  msg("\tGreen LED\n");
  rc = rfidscan_setLedsP(dev, 0, 1, 0);
  if (rc < 0)
    return rc;

  sleep(1);

  msg("\tBlue LED\n");
  rc = rfidscan_setLedsP(dev, 0, 0, 1);
  if (rc < 0)
    return rc;

  sleep(1);

  msg("\tAll LEDs ON\n");
  rc = rfidscan_setLedsP(dev, 1, 1, 1);
  if (rc < 0)
    return rc;

  msg("\tBeep 150ms\n");
  rc = rfidscan_setBuzzer(dev, 150);
  if (rc < 0)
    return rc;

  sleep(1);

  msg("Everything looks OK!\n");
  rc = rfidscan_setLedsP(dev, 0xD, 0xD, 0xD);

  return rc;
}

//
static int do_get_version(rfidscan_device *dev)
{
  char data[64];
  int rc;

  rc = rfidscan_getVendorName(dev, data, sizeof(data));
  if (rc < 0)
    return rc;

  printf("\tVendorName  : %s\n", data);

  rc = rfidscan_getProductName(dev, data, sizeof(data));
  if (rc < 0)
    return rc;

  printf("\tProductName : %s\n", data);

  rc = rfidscan_getSerialNumber(dev, data, sizeof(data));
  if (rc < 0)
    return rc;

  printf("\tSerialNumber: %s\n", data);

  rc = rfidscan_getVersion(dev, data, sizeof(data));
  if (rc < 0)
    return rc;

  if (strlen(data) == 10)
  {
    printf("\tVersion     : %c%c.%c%c (SpringProx LIB %c%c.%c%c, build %c%c)\n",
      data[0] != '0' ? data[0] : 0, data[1], data[2], data[3],
      data[4] != '0' ? data[4] : 0, data[5], data[6], data[7],
      data[8] != '0' ? data[8] : 0, data[9]);
  } else
  {
    printf("\tVersion     : %s\n", data);
  }
  return rc;
}

//
int do_read(rfidscan_device *dev, uint8_t addr, int show_empty)
{
  uint8_t data[256];
  int rc, i;

  rc = rfidscan_RegisterRead(dev, addr, data, sizeof(data));
  if (rc < 0)
    return rc;

  if (rc > 0)
  {
    printf("%02X : ", addr);

    if ((addr == 0x55) || (addr == 0x56) || (addr == 0x6F))
    {
      for (i=0; i<rc; i++)
        printf("XX");
    } else
    {
      for (i=0; i<rc; i++)
        printf("%02X", data[i]);
    }
    printf("\n");
  } else
  if ((rc == 0) && show_empty)
  {
    printf("%02X : (empty)\n", addr);
  }

  return rc;
}

//
int do_write(rfidscan_device *dev, uint8_t addr, uint8_t *data, size_t size)
{
  int rc = rfidscan_RegisterWrite(dev, addr, data, size);

  if (rc >= 0)
    rc = do_read(dev, addr, 1);

  return rc;
}

//
int do_dump(rfidscan_device *dev)
{
  uint8_t addr;
  int count = 0;
  int rc;

  for (addr=1; addr<255; addr++)
  {
    rc = do_read(dev, addr, 0);
    if (rc < 0)
      return rc;
    if (rc > 0)
      count++;
  }

  if (!count)
    printf("No register defined in this RFID Scanner\n");

  return 0;
}

// --------------------------------------------------------------------------- 
// 
static uint8_t getopt_led(const char *s)
{
  if (!stricmp(s, "0") || !stricmp(s, "off")) return 0;
  if (!stricmp(s, "1") || !stricmp(s, "on")) return 1;
  if (!stricmp(s, "s") || !stricmp(s, "slow")) return 2;
  if (!stricmp(s, "a") || !stricmp(s, "auto")) return 3;
  if (!stricmp(s, "f") || !stricmp(s, "fast")) return 4;
  if (!stricmp(s, "h") || !stricmp(s, "heart")) return 5;
  if (!stricmp(s, "si") || !stricmp(s, "slowinv")) return 6;
  if (!stricmp(s, "fi") || !stricmp(s, "fastinv")) return 7;
  if (!stricmp(s, "hi") || !stricmp(s, "heartinv")) return 8;
  if (!stricmp(s, "2") || !stricmp(s, "half")) return 9;
  if (!stricmp(s, "2i") || !stricmp(s, "halfinv")) return 10;
  if (!stricmp(s, "t") || !stricmp(s, "type")) return 11;
  if (!stricmp(s, "rf") || !stricmp(s, "field")) return 12;
  if (!stricmp(s, "d") || !stricmp(s, "default")) return 13;
  if (!stricmp(s, "fl") || !stricmp(s, "float")) return 14;
  if (!stricmp(s, "_") || !stricmp(s, "ignore")) return 15;
  return 15;
}

// --------------------------------------------------------------------------- 
// 
static uint8_t getopt_layout(const char *s)
{
  if (!stricmp(s, "qwerty")) return 0x00;
  if (!stricmp(s, "qwertz")) return 0x02;
  if (!stricmp(s, "azerty-desktop") || !stricmp(s, "azerty-full")) return 0x01;
  if (!stricmp(s, "azerty") || !stricmp(s, "azerty-laptop")) return 0x03;
  return 0xFF;
}


// --------------------------------------------------------------------------- 
//
static void usage(char *myName)
{
  fprintf(stderr,
    "Usage: \n"
    "  %s <cmd> [options]\n"
    "\n"
    "where <cmd> is one of:\n"
    "  --leds <red>,<green>,<blue> [--during <time>]\n"
    "                       Set LED value for the specified time (in millisecond)\n"
    "                       Allowed values for red, green and blue parameters are:\n"
    "                         off, on, slow, fast, heart, slowinv, fastinv, heartinv\n"
    "  --leds-default       Restore default behaviour for the LEDs\n"
    "  --beep [--during <time>]\n"
    "                       Buzzer sounds for the specified time (in millisecond)\n"
    "  --list               List connected RFID Scanner(s)\n"
    "  --version            Display the version of the RFID Scanner(s)\n"
    "  --layout=<layout>    Set keyboard layout, supported layout values are:\n"
    "                         qwerty, azerty, qwertz\n"
    "  --read <addr>        Read a configuration register\n"
    "  --dump               Read all configuration registers\n"
    "  --write <addr>=<value>\n"
    "                       Write a configuration register\n"
    "  --write-conf <file>  Write the configuration from a .multiconf file\n"
    "\n"
    "and [options] are: \n"
    "  -i <devices>  --id <all|deviceIds>\n"
    "                       Use these device ids (from --list) \n"
    "  -q, --quiet          Mutes all stdout output (supercedes --verbose)\n"
    "  -v, --verbose        Verbose debugging msgs\n"
    "  -r, --reset          Reset the RFID Scanner when exiting\n"
    "  -p, --password <password>\n"
    "                       If the RFID Scanner is password-protected\n"
    "\n"
    "Examples\n"
    "  %s --leds fast,fastinv,off\n"
    "                       Fast blinking, Red/Green alternatively\n"
    "  %s --leds off,off,on --during 2000\n"
    "                       Blue during 2s (and then back to default)\n"
    "  %s --beep --during 2000\n"
    "                       Buzzer sounds during 2s\n"
    "  %s --layout=azerty --reset\n"
    "                       Configure for an AZERTY keyboard and restart\n"
    "\n"
    ,myName,myName,myName,myName,myName);
}

// local states for the "cmd" option variable
enum { 
  CMD_NONE = 0,
  CMD_HELP = '?',
  OPT_DEVICE = 'i',
  OPT_QUIET = 'q',
  OPT_VERBOSE = 'v',
  OPT_DURING = 'd',
  OPT_PASSWORD = 'p',
  OPT_RESET = 'r',
  CMD_LEDS = 'l',
  CMD_BEEP = 'b',
  CMD_DUMMY = 127,
  CMD_LEDS_DEFAULT,
  CMD_LIST,
  CMD_TEST,
  CMD_VERSION,
  CMD_EEREAD,
  CMD_EEWRITE,
  CMD_EEDUMP,
  CMD_EEFILE,
  CMD_LAYOUT,
};

//
int main(int argc, char** argv)
{
  rfidscan_device* dev;
  uint32_t  deviceIds[rfidscan_max_devices];

  int i, rc;
  
  int countDevices;
  int numDevicesToUse = 0;

  uint8_t leds_r = 0xD;
  uint8_t leds_g = 0xD;
  uint8_t leds_b = 0xD;
  uint8_t layout = 0xFF;

  uint8_t register_addr = 0;
  int register_size = 0;
  uint8_t register_data[64];

  uint16_t during_ms = 0;

  uint8_t password[2] = { 0xFF, 0xFF };

  const char *config_file = NULL;

  int cmd  = CMD_NONE;
  int reset = 0;

  // parse options
  int option_value, option_index = 0;
  
  const char *option_string = "i:qvrd:l:b:?";
  static struct option option_list[] = {
    {"help",         no_argument,       0,      CMD_HELP},
    {"id",           required_argument, 0,      OPT_DEVICE},
    {"ids",          required_argument, 0,      OPT_DEVICE},
    {"quiet",        no_argument,       0,      OPT_QUIET},
    {"verbose",      no_argument,       0,      OPT_VERBOSE},
    {"reset",        no_argument,       0,      OPT_RESET},
    {"password",     required_argument, 0,      OPT_PASSWORD},
    {"during",       required_argument, 0,      OPT_DURING},
    {"leds",         required_argument, 0,      CMD_LEDS},
    {"leds-default", no_argument,       0,      CMD_LEDS_DEFAULT},
    {"beep",         no_argument,       0,      CMD_BEEP},
    {"list",         no_argument,       0,      CMD_LIST},
    {"test",         no_argument,       0,      CMD_TEST},
    {"version",      no_argument,       0,      CMD_VERSION},
    {"read",         required_argument, 0,      CMD_EEREAD},
    {"write",        required_argument, 0,      CMD_EEWRITE},
    {"dump",         no_argument,       0,      CMD_EEDUMP},
    {"write-conf",   required_argument, 0,      CMD_EEFILE},
    {"layout",       required_argument, 0,      CMD_LAYOUT},
    {NULL,           0,                 0,      0}
  };

  while(1)
  {
    option_value = getopt_long(argc, argv, option_string, option_list, &option_index);
    if (option_value==-1)
      break; // parsed all the args

    switch (option_value)
    {
      case CMD_TEST:
        cmd = CMD_TEST;
        break;
      case CMD_LIST:
        cmd = CMD_LIST;
        break;
      case CMD_VERSION:
        cmd = CMD_VERSION;
        break;
      case CMD_LEDS:
      case CMD_LEDS_DEFAULT:
        cmd = CMD_LEDS;
        if (optarg != NULL)
        {
          char *pch = strtok( optarg, " ,");
          if (pch != NULL)
          {
            leds_r = getopt_led(pch);
            pch = strtok(NULL, " ,");
          }
          if (pch != NULL)
          {
            leds_g = getopt_led(pch);
            pch = strtok(NULL, " ,");
          }
          if (pch != NULL)
          {
            leds_b = getopt_led(pch);
            pch = strtok(NULL, " ,");
          }
        }
        break;
      case CMD_BEEP:
        cmd = CMD_BEEP;
        break;


      case CMD_EEREAD :
        cmd = CMD_EEREAD;
        register_addr = htob(optarg);
        if ((register_addr  == 0x00) || (register_addr  == 0xFF))
        {
          msg("Invalid register addr\n");
          exit(EXIT_FAILURE);
        }
        break;

      case CMD_EEWRITE :
        cmd = CMD_EEWRITE;
        if (optarg != NULL)
        {
          char *pch = strtok( optarg, "=");
          if (pch != NULL)
          {
            register_addr = htob(optarg);
            if ((register_addr  == 0x00) || (register_addr  == 0xFF))
            {
              msg("Invalid register addr\n");
              exit(EXIT_FAILURE);
            }
            pch = strtok(NULL, "=");
          }
          if (pch != NULL)
          {
            register_size = hstob(pch, register_data, sizeof(register_data));
          }
        }
        break;

      case CMD_EEDUMP :
        cmd = CMD_EEDUMP;
        break;

      case CMD_EEFILE :
        cmd = CMD_EEFILE;
        config_file = optarg;
        break;

      case CMD_LAYOUT :
        cmd = CMD_LAYOUT;
        if (optarg != NULL)
          layout = getopt_layout(optarg);
        if (layout == 0xFF)
        {
          msg("Invalid keyboard layout\n");
          exit(EXIT_FAILURE);
        }
        break;

      case OPT_DURING:
        if (optarg != NULL)
          during_ms = (uint16_t) strtol(optarg,NULL,10);
        break;

      case OPT_PASSWORD:
        if (optarg != NULL)
          hstob(optarg, password, 2);
        break;

      case OPT_RESET :
        reset++;
        break;

      case OPT_QUIET:
        if (optarg==NULL) quiet++;
        else quiet = strtol(optarg,NULL,0);
        break;
      case OPT_VERBOSE:
        if (optarg==NULL) verbose++;
        else verbose = strtol(optarg,NULL,0);
        if (verbose > 3)
          fprintf(stderr,"going REALLY verbose\n");
        break;
      case OPT_DEVICE:
        if( strcmp(optarg,"all") == 0 ) {
          numDevicesToUse = 0; //rfidscan_max_devices;
          for (i=0; i< rfidscan_max_devices; i++) {
            deviceIds[i] = i;
          }
        } 
        else { // if( strcmp(optarg,",") != -1 ) { // comma-separated list
          char* pch;
          int base = 0;
          pch = strtok( optarg, " ,");
          numDevicesToUse = 0;
          while( pch != NULL ) { 
            int base = (strlen(pch)==8) ? 16:0;
            deviceIds[numDevicesToUse++] = strtol(pch,NULL,base);
            pch = strtok(NULL, " ,");
          }
          for (i=0; i<numDevicesToUse; i++) { 
            printf("deviceId[%d]: %d\n", i, deviceIds[i]);
          }
        }
        break;

      case CMD_HELP:
        usage("rfidscan-tool");
        exit(EXIT_FAILURE);
        break;

      default :
        msg("Error while parsing the args\n");
        exit(EXIT_FAILURE);
    }
  }

  if (cmd == CMD_NONE)
  {
    usage("rfidscan-tool");
    exit(EXIT_FAILURE);
  }

  /* Get a list of all devices and their paths */
  countDevices = rfidscan_enumerate();
  if (countDevices == 0)
  {
    msg("No RFID Scanner found\n");
    exit(EXIT_FAILURE);
  }

  msg("%d RFID Scanner(s) found\n", countDevices);

  if (cmd == CMD_LIST)
  {
    for (i=0; i<countDevices; i++)
    {
      printf("id:%d - VID: %04X, PID: %04X, serial number: %s\n", i, rfidscan_getCachedVid(i), rfidscan_getCachedPid(i), rfidscan_getCachedSerial(i));
    }

    exit(EXIT_SUCCESS);
  }

  if (numDevicesToUse == 0)
  {
    for (i=0; i<countDevices; i++)
      deviceIds[i] = i;
    numDevicesToUse = countDevices;
  }

  for (i=0; i<numDevicesToUse; i++)
  {
    dev = rfidscan_openById(deviceIds[i]);
    if (dev == NULL)
    {
      msg("Failed to open RFID Scanner with id:%d/%d\n", i+1, numDevicesToUse);
      exit(EXIT_FAILURE);
    }

    if (numDevicesToUse > 1)
      msg("Working on RFID Scanner with id:%d/%d\n", i+1, numDevicesToUse);

    switch (cmd)
    {
      case CMD_LEDS :
      case CMD_BEEP :
      case CMD_VERSION :
        break;

      default :
        /* Check password */
        {
          uint8_t buffer[2];
          rc = rfidscan_RegisterRead(dev, 0x6F, buffer, sizeof(buffer));

          if (rc > 0) 
          {
            if ((rc != 2) || ((buffer[0] == 0xFF) && (buffer[1] == 0xFF)))
            {
              msg("This RFID Scanner has been locked\n");
              exit(EXIT_FAILURE);
            }

            if ((password[0] == 0xFF) && (password[1]))
            {
              msg("This RFID Scanner is password-protected\n");
              msg("Use the --password <password> option to login\n");
              exit(EXIT_FAILURE);
            }

            if (memcmp(password, buffer, 2))
            {
              msg("Wrong password\n");
              exit(EXIT_FAILURE);
            }

            msg("Password is OK!\n");
          }
        }
    }

    switch (cmd)
    {
      case CMD_LEDS :
        if (during_ms != 0)
          rc = rfidscan_setLedsT(dev, leds_r, leds_g, leds_b, during_ms);
        else
          rc = rfidscan_setLedsP(dev, leds_r, leds_g, leds_b);
        break;
      case CMD_BEEP :
        if (during_ms != 0)
          rc = rfidscan_setBuzzer(dev, during_ms);
        else
          rc = rfidscan_setBuzzer(dev, 30);
        break;
      case CMD_VERSION :
        msg("Querying RFID Scanner with id:%d/%d\n", i+1, numDevicesToUse);
        msg("\tVendorID    : %04X\n", rfidscan_getCachedVid(i));
        msg("\tProductID   : %04X\n", rfidscan_getCachedPid(i));
        rc = do_get_version(dev);
        break;

      case CMD_TEST :
        msg("Testing RFID Scanner width id:%d/%d\n", i+1, numDevicesToUse);
        rc = do_test(dev);
        break;

      case CMD_EEDUMP :
        rc = do_dump(dev);
        break;

      case CMD_LAYOUT :
        msg("Setting new keyboard layout\n");
        rc = do_write(dev, 0xA0, &layout, 1);
        break;

      case CMD_EEREAD :
        rc = do_read(dev, register_addr, 1);
        break;

      case CMD_EEWRITE :
        rc = do_write(dev, register_addr, register_data, register_size);
        break;

      case CMD_EEFILE :
        {
          FILE *fp = NULL;
          char buffer[512];
          int general_section = 0;
          int raw_section = 0;

          fp = fopen(config_file, "rt");
          if (fp == NULL)
          {
            msg("Failed to open the configuration file '%s'\n", config_file);
            exit(EXIT_FAILURE);
          }

          while (fgets(buffer, sizeof(buffer), fp))
          {
            strtok(buffer, "#;\r\n");
            if (!stricmp(buffer, "[general]"))
            {
              raw_section = 0;
              general_section = 1;
            } else
            if (!stricmp(buffer, "[raw]"))
            {
              general_section = 0;
              raw_section = 1;
            } else
            if (buffer[0] == '[')
            {
              general_section = 0;
              raw_section = 0;
            } else
            if (!stricmp(buffer, "erase=1") && (general_section || raw_section))
            {
              msg("Erasing previous values...\n");
              for (register_addr = 0; register_addr < 0xFF; register_addr++)
              {
                rc = rfidscan_RegisterWrite(dev, register_addr, NULL, 0);
                if (rc < 0)
                  break;
              }
            } else
            if (raw_section)
            {
              char *pch = strtok(buffer, "=");
              if ((pch != NULL) && (strlen(pch) == 2))
              {
                register_addr = htob(pch);
                if ((register_addr  == 0x00) || (register_addr  == 0xFF))
                {
                  msg("Invalid register addr\n");
                  exit(EXIT_FAILURE);
                }
                pch = strtok(NULL, "=");
                if (pch != NULL)
                  register_size = hstob(pch, register_data, sizeof(register_data));
                else
                  register_size = 0;

                rc = do_write(dev, register_addr, register_data, register_size);
                if (rc < 0)
                  break;
              }
            }
          }

          fclose(fp);
        }
        break;

      default :
        msg("Internal error\n");
        exit(EXIT_FAILURE);
    }

    if (reset && (rc >= 0))
    {
      msg("Applying the new settings...\n");
      rc = rfidscan_ApplyConfig(dev);
    }

    if (rc < 0)
    {
      msg("An error has occured\n");
      exit(EXIT_FAILURE);
    }

    rfidscan_close(dev);
  }

  exit(EXIT_SUCCESS);
}
