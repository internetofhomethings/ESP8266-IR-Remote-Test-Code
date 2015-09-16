/*
 * IRremote
 * Version 0.1 July, 2009
 * Copyright 2009 Ken Shirriff
 * For details, see http://arcfn.com/2009/08/multi-protocol-infrared-remote-library.htm http://arcfn.com
 * Edited by Mitra to add new controller SANYO
 *
 * Interrupt code based on NECIRrcv by Joe Knapp
 * http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1210243556
 * Also influenced by http://zovirl.com/2008/11/12/building-a-universal-remote-with-an-arduino/
 *
 * JVC and Panasonic protocol added by Kristian Lauszus (Thanks to zenwheel and other people at the original blog post)
 * Ported to ESP8266 Espressif SDK Platform by Dave St. Aubin (internetofhomethings.com)
 */

#ifndef IRremote_h
#define IRremote_h
#include "ets_sys.h"
#include "osapi.h"
#include <os_type.h>
#include "c_types.h"

/*Definition of GPIO PIN params, for GPIO initialization*/
#define PWM_0_OUT_IO_MUX PERIPHS_IO_MUX_MTDI_U
#define PWM_0_OUT_IO_NUM 12
#define PWM_0_OUT_IO_FUNC  FUNC_GPIO12


// The following are compile-time library options.
// If you change them, recompile the library.
// If DEBUG is defined, a lot of debugging output will be printed during decoding.
// TEST must be defined for the IRtest unittests to work.  It will make some
// methods virtual, which will be slightly slower, which is why it is optional.
// #define DEBUG
// #define TEST

// class decode_results types
typedef struct decode_results_ {
  int decode_type;               // NEC, SONY, RC5, UNKNOWN
  unsigned int panasonicAddress; // This is only used for decoding Panasonic data
  unsigned long value;           // Decoded value
  int bits;                      // Number of bits in decoded value
  volatile unsigned int *rawbuf; // Raw intervals in .5 us ticks
  int rawlen;                    // Number of records in rawbuf.
} decode_results;

// class decode_results variables
  int decode_type; // NEC, SONY, RC5, UNKNOWN
  unsigned int panasonicAddress; // This is only used for decoding Panasonic data
  unsigned long value; // Decoded value
  int bits; // Number of bits in decoded value
  volatile unsigned int *rawbuf; // Raw intervals in .5 us ticks
  int rawlen; // Number of records in rawbuf.

  // main class for receiving IR class IRrecv variables & functions
  os_timer_t ir_timer;
  void ICACHE_FLASH_ATTR IRrecv(int recvpin);
  void ICACHE_FLASH_ATTR blink16(int blinkflag);
  int ICACHE_FLASH_ATTR decode(decode_results *results);
  void ICACHE_FLASH_ATTR enableIRIn(void);
  void ICACHE_FLASH_ATTR resume(void);
  // These are called by decode
  int ICACHE_FLASH_ATTR getRClevel(decode_results *results, int *offset, int *used, int t1);
  long ICACHE_FLASH_ATTR decodeNEC(decode_results *results);
  //long decodeSony(decode_results *results);
  long ICACHE_FLASH_ATTR decodeSanyo(decode_results *results);
  //long decodeMitsubishi(decode_results *results);
  //long decodeRC5(decode_results *results);
  //long decodeRC6(decode_results *results);
  //long decodePanasonic(decode_results *results);
  //long decodeJVC(decode_results *results);
  long decodeHash(decode_results *results);
  int compare(unsigned int oldval, unsigned int newval);

  // class Values for decode_type
  #define NEC 1
  #define SONY 2
  #define RC5 3
  #define RC6 4
  #define DISH 5
  #define SHARP 6
  #define PANASONIC 7
  #define JVC 8
  #define SANYO 9
  #define MITSUBISHI 10
  #define UNKNOWN -1

  // Decoded value for NEC when a repeat code is received
  #define REPEAT 0xffffffff



// Only used for testing; can remove virtual for shorter code
#ifdef TEST
#define VIRTUAL virtual
#else
#define VIRTUAL
#endif
// Some useful constants

#define USECPERTICK 50  // microseconds per clock interrupt tick
#define RAWBUF 100 // Length of raw duration buffer

// Marks tend to be 100us too long, and spaces 100us too short
// when received due to sensor lag.
#define MARK_EXCESS 100

#endif

//class IRsend
  void ICACHE_FLASH_ATTR IRsend(void);
  void ICACHE_FLASH_ATTR sendNEC(unsigned long data, int nbits);
  void ICACHE_FLASH_ATTR sendSony(unsigned long data, int nbits);
  void ICACHE_FLASH_ATTR sendRaw(unsigned int buf[], int len, int hz);
  void ICACHE_FLASH_ATTR sendRC5(unsigned long data, int nbits);
  void ICACHE_FLASH_ATTR sendRC6(unsigned long data, int nbits);
  // private:
  void ICACHE_FLASH_ATTR enableIROut(int khz);
  void ICACHE_FLASH_ATTR mark(int usec);
  void ICACHE_FLASH_ATTR space(int usec);


void ICACHE_FLASH_ATTR ir_cb(void *arg);
