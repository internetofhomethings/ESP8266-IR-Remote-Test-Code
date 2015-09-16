/*
 * IRremote
 * Version 0.11 August, 2009
 * Copyright 2009 Ken Shirriff
 * For details, see http://arcfn.com/2009/08/multi-protocol-infrared-remote-library.html
 *
 * Modified by Paul Stoffregen <paul@pjrc.com> to support other boards and timers
 * Modified  by Mitra Ardron <mitra@mitra.biz>
 * Added Sanyo and Mitsubishi controllers
 * Modified Sony to spot the repeat codes that some Sony's send
 *
 * Interrupt code based on NECIRrcv by Joe Knapp
 * http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1210243556
 * Also influenced by http://zovirl.com/2008/11/12/building-a-universal-remote-with-an-arduino/
 *
 * JVC and Panasonic protocol added by Kristian Lauszus (Thanks to zenwheel and other people at the original blog post)
 * Ported to ESP8266 Espressif SDK Platform (Ansi C, classes decomposed) by Dave St. Aubin (internetofhomethings.com)
 */

#define USE_US_TIMER 1
//#include "espmissingincludes.h"
#include <ets_sys.h>
#include <osapi.h>

#include "user_interface.h"
#include "user_config.h"

#include <os_type.h>
#include "c_types.h"

#if ESP_PLATFORM
#include "user_esp_platform.h"
#endif

#include <gpio.h>
#include "driver/gpio16.h"

#include "../include/driver/EspIRremote.h"
#include "../include/driver/EspIRremoteInt.h"

#define IR_SEND_PIN 6
// Provides ISR
////#include <avr/interrupt.h>

volatile irparams_t irparams;

// These versions of MATCH, MATCH_MARK, and MATCH_SPACE are only for debugging.
// To use them, set DEBUG in IRremoteInt.h
// Normally macros are used for efficiency
#ifdef DEBUG
int MATCH(int measured, int desired) {
  Serial.print("Testing: ");
  Serial.print(TICKS_LOW(desired), DEC);
  Serial.print(" <= ");
  Serial.print(measured, DEC);
  Serial.print(" <= ");
  Serial.println(TICKS_HIGH(desired), DEC);
  return measured >= TICKS_LOW(desired) && measured <= TICKS_HIGH(desired);
}

int MATCH_MARK(int measured_ticks, int desired_us) {
  Serial.print("Testing mark ");
  Serial.print(measured_ticks * USECPERTICK, DEC);
  Serial.print(" vs ");
  Serial.print(desired_us, DEC);
  Serial.print(": ");
  Serial.print(TICKS_LOW(desired_us + MARK_EXCESS), DEC);
  Serial.print(" <= ");
  Serial.print(measured_ticks, DEC);
  Serial.print(" <= ");
  Serial.println(TICKS_HIGH(desired_us + MARK_EXCESS), DEC);
  return measured_ticks >= TICKS_LOW(desired_us + MARK_EXCESS) && measured_ticks <= TICKS_HIGH(desired_us + MARK_EXCESS);
}

int MATCH_SPACE(int measured_ticks, int desired_us) {
  Serial.print("Testing space ");
  Serial.print(measured_ticks * USECPERTICK, DEC);
  Serial.print(" vs ");
  Serial.print(desired_us, DEC);
  Serial.print(": ");
  Serial.print(TICKS_LOW(desired_us - MARK_EXCESS), DEC);
  Serial.print(" <= ");
  Serial.print(measured_ticks, DEC);
  Serial.print(" <= ");
  Serial.println(TICKS_HIGH(desired_us - MARK_EXCESS), DEC);
  return measured_ticks >= TICKS_LOW(desired_us - MARK_EXCESS) && measured_ticks <= TICKS_HIGH(desired_us - MARK_EXCESS);
}
#else
int MATCH(int measured, int desired) {return measured >= TICKS_LOW(desired) && measured <= TICKS_HIGH(desired);}
int MATCH_MARK(int measured_ticks, int desired_us) {return MATCH(measured_ticks, (desired_us + MARK_EXCESS));}
int MATCH_SPACE(int measured_ticks, int desired_us) {return MATCH(measured_ticks, (desired_us - MARK_EXCESS));}
#endif

void ICACHE_FLASH_ATTR IRrecv(int recvpin)
{
  irparams.recvpin = recvpin;
  irparams.blinkflag = 0;
}

// initialization
void ICACHE_FLASH_ATTR enableIRIn() {

  // initialize state machine variables
  irparams.rcvstate = STATE_IDLE;
  irparams.rawlen = 0;

  set_gpio_mode(irparams.recvpin, GPIO_PULLUP, GPIO_INPUT);   //GPIO13 IR RECV

  //Start periodic loop
  os_timer_disarm(&ir_timer);
  os_timer_setfn(&ir_timer, (os_timer_func_t *)ir_cb, (void *)11);
  os_timer_arm_us(&ir_timer,50,true);
}

// enable/disable blinking of pin 13 on IR processing
void ICACHE_FLASH_ATTR blink16(int blinkflag)
{
  irparams.blinkflag = blinkflag;
  if (blinkflag)
  {
		//gpio16_output_set(0);
  }
}


void ICACHE_FLASH_ATTR resume() {
  irparams.rcvstate = STATE_IDLE;
  irparams.rawlen = 0;
}



// Decodes the received IR message
// Returns 0 if no data ready, 1 if data ready.
// Results of decoding are stored in results
int ICACHE_FLASH_ATTR decode(decode_results *results) {
  results->rawbuf = irparams.rawbuf;
  results->rawlen = irparams.rawlen;
  if (irparams.rcvstate != STATE_STOP) {
    return ERR;
  }

#ifdef DEBUG
  Serial.println("Attempting NEC decode");
#endif
  if (decodeNEC(results)) {
    return DECODED;
  }
/*
#ifdef DEBUG
  Serial.println("Attempting Sony decode");
#endif
  if (decodeSony(results)) {
    return DECODED;
  }*/
/*
#ifdef DEBUG
  Serial.println("Attempting Sanyo decode");
#endif
*/
  if (decodeSanyo(results)) {
    return DECODED;
  }
  /*
  */
/*
#ifdef DEBUG
  Serial.println("Attempting Mitsubishi decode");
#endif
  if (decodeMitsubishi(results)) {
    return DECODED;
  }*/
/*
#ifdef DEBUG
  Serial.println("Attempting RC5 decode");
#endif
  if (decodeRC5(results)) {
    return DECODED;
  }
  */
/*
#ifdef DEBUG
  Serial.println("Attempting RC6 decode");
#endif
  if (decodeRC6(results)) {
    return DECODED;
  }
  */
/*
#ifdef DEBUG
    Serial.println("Attempting Panasonic decode");
#endif
    if (decodePanasonic(results)) {
        return DECODED;
    }
	*/
/*
#ifdef DEBUG
    Serial.println("Attempting JVC decode");
#endif
    if (decodeJVC(results)) {
        return DECODED;
    }*/
  // decodeHash returns a hash on any input.
  // Thus, it needs to be last in the list.
  // If you add any decodes, add them before this.
  if (decodeHash(results)) {
    return DECODED;
  }
  // Throw away and start over
  resume();
  return ERR;
}


// NECs have a repeat only 4 items long
long ICACHE_FLASH_ATTR decodeNEC(decode_results *results) {
  long data = 0;
  int offset = 1; // Skip first space
  int i;
  // Initial mark
  if (!MATCH_MARK(results->rawbuf[offset], NEC_HDR_MARK)) {
    return ERR;
  }
  offset++;
  // Check for repeat
  if (irparams.rawlen == 4 &&
    MATCH_SPACE(results->rawbuf[offset], NEC_RPT_SPACE) &&
    MATCH_MARK(results->rawbuf[offset+1], NEC_BIT_MARK)) {
    results->bits = 0;
    results->value = REPEAT;
    results->decode_type = NEC;
    return DECODED;
  }
  if (irparams.rawlen < 2 * NEC_BITS + 4) {
    return ERR;
  }
  // Initial space
  if (!MATCH_SPACE(results->rawbuf[offset], NEC_HDR_SPACE)) {
    return ERR;
  }
  offset++;
  for (i = 0; i < NEC_BITS; i++) {
    if (!MATCH_MARK(results->rawbuf[offset], NEC_BIT_MARK)) {
      return ERR;
    }
    offset++;
    if (MATCH_SPACE(results->rawbuf[offset], NEC_ONE_SPACE)) {
      data = (data << 1) | 1;
    }
    else if (MATCH_SPACE(results->rawbuf[offset], NEC_ZERO_SPACE)) {
      data <<= 1;
    }
    else {
      return ERR;
    }
    offset++;
  }
  // Success
  results->bits = NEC_BITS;
  results->value = data;
  results->decode_type = NEC;
  return DECODED;
}
/*
long ICACHE_FLASH_ATTR decodeSony(decode_results *results) {
  long data = 0;
  if (irparams.rawlen < 2 * SONY_BITS + 2) {
    return ERR;
  }
  int offset = 0; // Dont skip first space, check its size

  // Some Sony's deliver repeats fast after first
  // unfortunately can't spot difference from of repeat from two fast clicks
  if (results->rawbuf[offset] < SONY_DOUBLE_SPACE_USECS) {
    // Serial.print("IR Gap found: ");
    results->bits = 0;
    results->value = REPEAT;
    results->decode_type = SANYO;
    return DECODED;
  }
  offset++;

  // Initial mark
  if (!MATCH_MARK(results->rawbuf[offset], SONY_HDR_MARK)) {
    return ERR;
  }
  offset++;

  while (offset + 1 < irparams.rawlen) {
    if (!MATCH_SPACE(results->rawbuf[offset], SONY_HDR_SPACE)) {
      break;
    }
    offset++;
    if (MATCH_MARK(results->rawbuf[offset], SONY_ONE_MARK)) {
      data = (data << 1) | 1;
    }
    else if (MATCH_MARK(results->rawbuf[offset], SONY_ZERO_MARK)) {
      data <<= 1;
    }
    else {
      return ERR;
    }
    offset++;
  }

  // Success
  results->bits = (offset - 1) / 2;
  if (results->bits < 12) {
    results->bits = 0;
    return ERR;
  }
  results->value = data;
  results->decode_type = SONY;
  return DECODED;
}
*/

// I think this is a Sanyo decoder - serial = SA 8650B
// Looks like Sony except for timings, 48 chars of data and time/space different
long ICACHE_FLASH_ATTR decodeSanyo(decode_results *results) {
  long data = 0;
  if (irparams.rawlen < 2 * SANYO_BITS + 2) {
    return ERR;
  }
  int offset = 0; // Skip first space
  // Initial space
  // Put this back in for debugging - note can't use #DEBUG as if Debug on we don't see the repeat cos of the delay
  //Serial.print("IR Gap: ");
  //Serial.println( results->rawbuf[offset]);
  //Serial.println( "test against:");
  //Serial.println(results->rawbuf[offset]);

  if (results->rawbuf[offset] < SANYO_DOUBLE_SPACE_USECS) {
    // Serial.print("IR Gap found: ");
    results->bits = 0;
    results->value = REPEAT;
    results->decode_type = SANYO;
    return DECODED;
  }
  offset++;

  // Initial mark
  if (!MATCH_MARK(results->rawbuf[offset], SANYO_HDR_MARK)) {
    return ERR;
  }
  offset++;

  // Skip Second Mark
  if (!MATCH_MARK(results->rawbuf[offset], SANYO_HDR_MARK)) {
    return ERR;
  }
  offset++;

  while (offset + 1 < irparams.rawlen) {
    if (!MATCH_SPACE(results->rawbuf[offset], SANYO_HDR_SPACE)) {
      break;
    }
    offset++;
    if (MATCH_MARK(results->rawbuf[offset], SANYO_ONE_MARK)) {
      data = (data << 1) | 1;
    }
    else if (MATCH_MARK(results->rawbuf[offset], SANYO_ZERO_MARK)) {
      data <<= 1;
    }
    else {
      return ERR;
    }
    offset++;
  }

  // Success
  results->bits = (offset - 1) / 2;
  if (results->bits < 12) {
    results->bits = 0;
    return ERR;
  }
  results->value = data;
  results->decode_type = SANYO;
  return DECODED;
}

/*
// Looks like Sony except for timings, 48 chars of data and time/space different
long IRrecv::decodeMitsubishi(decode_results *results) {
  // Serial.print("?!? decoding Mitsubishi:");Serial.print(irparams.rawlen); Serial.print(" want "); Serial.println( 2 * MITSUBISHI_BITS + 2);
  long data = 0;
  if (irparams.rawlen < 2 * MITSUBISHI_BITS + 2) {
    return ERR;
  }
  int offset = 0; // Skip first space
  // Initial space
  // Put this back in for debugging - note can't use #DEBUG as if Debug on we don't see the repeat cos of the delay
  //Serial.print("IR Gap: ");
  //Serial.println( results->rawbuf[offset]);
  //Serial.println( "test against:");
  //Serial.println(results->rawbuf[offset]);

  // Not seeing double keys from Mitsubishi
  //if (results->rawbuf[offset] < MITSUBISHI_DOUBLE_SPACE_USECS) {
    // Serial.print("IR Gap found: ");
  //  results->bits = 0;
  //  results->value = REPEAT;
  //  results->decode_type = MITSUBISHI;
  //  return DECODED;
  //}

  offset++;

  // Typical
  // 14200 7 41 7 42 7 42 7 17 7 17 7 18 7 41 7 18 7 17 7 17 7 18 7 41 8 17 7 17 7 18 7 17 7

  // Initial Space
  if (!MATCH_MARK(results->rawbuf[offset], MITSUBISHI_HDR_SPACE)) {
    return ERR;
  }
  offset++;
  while (offset + 1 < irparams.rawlen) {
    if (MATCH_MARK(results->rawbuf[offset], MITSUBISHI_ONE_MARK)) {
      data = (data << 1) | 1;
    }
    else if (MATCH_MARK(results->rawbuf[offset], MITSUBISHI_ZERO_MARK)) {
      data <<= 1;
    }
    else {
      // Serial.println("A"); Serial.println(offset); Serial.println(results->rawbuf[offset]);
      return ERR;
    }
    offset++;
    if (!MATCH_SPACE(results->rawbuf[offset], MITSUBISHI_HDR_SPACE)) {
      // Serial.println("B"); Serial.println(offset); Serial.println(results->rawbuf[offset]);
      break;
    }
    offset++;
  }

  // Success
  results->bits = (offset - 1) / 2;
  if (results->bits < MITSUBISHI_BITS) {
    results->bits = 0;
    return ERR;
  }
  results->value = data;
  results->decode_type = MITSUBISHI;
  return DECODED;
}*/


// Gets one undecoded level at a time from the raw buffer.
// The RC5/6 decoding is easier if the data is broken into time intervals.
// E.g. if the buffer has MARK for 2 time intervals and SPACE for 1,
// successive calls to getRClevel will return MARK, MARK, SPACE.
// offset and used are updated to keep track of the current position.
// t1 is the time interval for a single bit in microseconds.
// Returns -1 for error (measured time interval is not a multiple of t1).
int ICACHE_FLASH_ATTR getRClevel(decode_results *results, int *offset, int *used, int t1) {
  if (*offset >= results->rawlen) {
    // After end of recorded buffer, assume SPACE.
    return SPACE;
  }
  int width = results->rawbuf[*offset];
  int val = ((*offset) % 2) ? MARK : SPACE;
  int correction = (val == MARK) ? MARK_EXCESS : - MARK_EXCESS;

  int avail;
  if (MATCH(width, t1 + correction)) {
    avail = 1;
  }
  else if (MATCH(width, 2*t1 + correction)) {
    avail = 2;
  }
  else if (MATCH(width, 3*t1 + correction)) {
    avail = 3;
  }
  else {
    return -1;
  }

  (*used)++;
  if (*used >= avail) {
    *used = 0;
    (*offset)++;
  }
#ifdef DEBUG
  if (val == MARK) {
    Serial.println("MARK");
  }
  else {
    Serial.println("SPACE");
  }
#endif
  return val;
}
/*
long IRrecv::decodeRC5(decode_results *results) {
  if (irparams.rawlen < MIN_RC5_SAMPLES + 2) {
    return ERR;
  }
  int offset = 1; // Skip gap space
  long data = 0;
  int used = 0;
  // Get start bits
  if (getRClevel(results, &offset, &used, RC5_T1) != MARK) return ERR;
  if (getRClevel(results, &offset, &used, RC5_T1) != SPACE) return ERR;
  if (getRClevel(results, &offset, &used, RC5_T1) != MARK) return ERR;
  int nbits;
  for (nbits = 0; offset < irparams.rawlen; nbits++) {
    int levelA = getRClevel(results, &offset, &used, RC5_T1);
    int levelB = getRClevel(results, &offset, &used, RC5_T1);
    if (levelA == SPACE && levelB == MARK) {
      // 1 bit
      data = (data << 1) | 1;
    }
    else if (levelA == MARK && levelB == SPACE) {
      // zero bit
      data <<= 1;
    }
    else {
      return ERR;
    }
  }

  // Success
  results->bits = nbits;
  results->value = data;
  results->decode_type = RC5;
  return DECODED;
}*/
/*
long IRrecv::decodeRC6(decode_results *results) {
  if (results->rawlen < MIN_RC6_SAMPLES) {
    return ERR;
  }
  int offset = 1; // Skip first space
  // Initial mark
  if (!MATCH_MARK(results->rawbuf[offset], RC6_HDR_MARK)) {
    return ERR;
  }
  offset++;
  if (!MATCH_SPACE(results->rawbuf[offset], RC6_HDR_SPACE)) {
    return ERR;
  }
  offset++;
  long data = 0;
  int used = 0;
  // Get start bit (1)
  if (getRClevel(results, &offset, &used, RC6_T1) != MARK) return ERR;
  if (getRClevel(results, &offset, &used, RC6_T1) != SPACE) return ERR;
  int nbits;
  for (nbits = 0; offset < results->rawlen; nbits++) {
    int levelA, levelB; // Next two levels
    levelA = getRClevel(results, &offset, &used, RC6_T1);
    if (nbits == 3) {
      // T bit is double wide; make sure second half matches
      if (levelA != getRClevel(results, &offset, &used, RC6_T1)) return ERR;
    }
    levelB = getRClevel(results, &offset, &used, RC6_T1);
    if (nbits == 3) {
      // T bit is double wide; make sure second half matches
      if (levelB != getRClevel(results, &offset, &used, RC6_T1)) return ERR;
    }
    if (levelA == MARK && levelB == SPACE) { // reversed compared to RC5
      // 1 bit
      data = (data << 1) | 1;
    }
    else if (levelA == SPACE && levelB == MARK) {
      // zero bit
      data <<= 1;
    }
    else {
      return ERR; // Error
    }
  }
  // Success
  results->bits = nbits;
  results->value = data;
  results->decode_type = RC6;
  return DECODED;
}*/
/*
long IRrecv::decodePanasonic(decode_results *results) {
    unsigned long long data = 0;
    int offset = 1;

    if (!MATCH_MARK(results->rawbuf[offset], PANASONIC_HDR_MARK)) {
        return ERR;
    }
    offset++;
    if (!MATCH_MARK(results->rawbuf[offset], PANASONIC_HDR_SPACE)) {
        return ERR;
    }
    offset++;

    // decode address
    for (int i = 0; i < PANASONIC_BITS; i++) {
        if (!MATCH_MARK(results->rawbuf[offset++], PANASONIC_BIT_MARK)) {
            return ERR;
        }
        if (MATCH_SPACE(results->rawbuf[offset],PANASONIC_ONE_SPACE)) {
            data = (data << 1) | 1;
        } else if (MATCH_SPACE(results->rawbuf[offset],PANASONIC_ZERO_SPACE)) {
            data <<= 1;
        } else {
            return ERR;
        }
        offset++;
    }
    results->value = (unsigned long)data;
    results->panasonicAddress = (unsigned int)(data >> 32);
    results->decode_type = PANASONIC;
    results->bits = PANASONIC_BITS;
    return DECODED;
}*/
/*
long IRrecv::decodeJVC(decode_results *results) {
    long data = 0;
    int offset = 1; // Skip first space
    // Check for repeat
    if (irparams.rawlen - 1 == 33 &&
        MATCH_MARK(results->rawbuf[offset], JVC_BIT_MARK) &&
        MATCH_MARK(results->rawbuf[irparams.rawlen-1], JVC_BIT_MARK)) {
        results->bits = 0;
        results->value = REPEAT;
        results->decode_type = JVC;
        return DECODED;
    }
    // Initial mark
    if (!MATCH_MARK(results->rawbuf[offset], JVC_HDR_MARK)) {
        return ERR;
    }
    offset++;
    if (irparams.rawlen < 2 * JVC_BITS + 1 ) {
        return ERR;
    }
    // Initial space
    if (!MATCH_SPACE(results->rawbuf[offset], JVC_HDR_SPACE)) {
        return ERR;
    }
    offset++;
    for (int i = 0; i < JVC_BITS; i++) {
        if (!MATCH_MARK(results->rawbuf[offset], JVC_BIT_MARK)) {
            return ERR;
        }
        offset++;
        if (MATCH_SPACE(results->rawbuf[offset], JVC_ONE_SPACE)) {
            data = (data << 1) | 1;
        }
        else if (MATCH_SPACE(results->rawbuf[offset], JVC_ZERO_SPACE)) {
            data <<= 1;
        }
        else {
            return ERR;
        }
        offset++;
    }
    //Stop bit
    if (!MATCH_MARK(results->rawbuf[offset], JVC_BIT_MARK)){
        return ERR;
    }
    // Success
    results->bits = JVC_BITS;
    results->value = data;
    results->decode_type = JVC;
    return DECODED;
}*/

/* -----------------------------------------------------------------------
 * hashdecode - decode an arbitrary IR code.
 * Instead of decoding using a standard encoding scheme
 * (e.g. Sony, NEC, RC5), the code is hashed to a 32-bit value.
 *
 * The algorithm: look at the sequence of MARK signals, and see if each one
 * is shorter (0), the same length (1), or longer (2) than the previous.
 * Do the same with the SPACE signals.  Hszh the resulting sequence of 0's,
 * 1's, and 2's to a 32-bit value.  This will give a unique value for each
 * different code (probably), for most code systems.
 *
 * http://arcfn.com/2010/01/using-arbitrary-remotes-with-arduino.html
 */

// Compare two tick values, returning 0 if newval is shorter,
// 1 if newval is equal, and 2 if newval is longer
// Use a tolerance of 20%
int ICACHE_FLASH_ATTR compare(unsigned int oldval, unsigned int newval) {
  if (newval < oldval * .8) {
    return 0;
  }
  else if (oldval < newval * .8) {
    return 2;
  }
  else {
    return 1;
  }
}

// Use FNV hash algorithm: http://isthe.com/chongo/tech/comp/fnv/#FNV-param
#define FNV_PRIME_32 16777619
//#define FNV_BASIS_32 2166136261
#define FNV_BASIS_32 0x811C9DC5
//unsigned FNV_BASIS_32 = (unsigned) 2166136261;

/* Converts the raw code values into a 32-bit hash code.
 * Hopefully this code is unique for each button.
 * This isn't a "real" decoding, just an arbitrary value.
 */
long ICACHE_FLASH_ATTR decodeHash(decode_results *results) {
  int i;
  // Require at least 6 samples to prevent triggering on noise
  if (results->rawlen < 6) {
    return ERR;
  }
  long hash = FNV_BASIS_32;
  for (i = 1; i+2 < results->rawlen; i++) {
    int value =  compare(results->rawbuf[i], results->rawbuf[i+2]);
    // Add value into the hash
    hash = (hash * FNV_PRIME_32) ^ value;
  }
  results->value = hash;
  results->bits = 32;
  results->decode_type = UNKNOWN;
  return DECODED;
}

// TIMER interrupt code to collect raw data.
// Widths of alternating SPACE, MARK are recorded in rawbuf.
// Recorded in ticks of 50 microseconds.
// rawlen counts the number of entries recorded so far.
// First entry is the SPACE between transmissions.
// As soon as a SPACE gets long, ready is set, state switches to IDLE, timing of SPACE continues.
// As soon as first MARK arrives, gap width is recorded, ready is cleared, and new logging starts
void ICACHE_FLASH_ATTR ir_cb(void *arg)
{
  uint8_t irdata = (uint8_t) gpio_read(irparams.recvpin);
  irparams.value = irdata;

  irparams.timer++; // One more 50us tick
  if (irparams.rawlen >= RAWBUF) {
    // Buffer overflow11
    irparams.rcvstate = STATE_STOP;
  }
  gpio16_output_set(!irdata);

  switch(irparams.rcvstate) {
  case STATE_IDLE: // In the middle of a gap
    if (irdata == MARK) {
      if (irparams.timer < GAP_TICKS) {
        // Not big enough to be a gap.
        irparams.timer = 0;
      }
      else {
        // gap just ended, record duration and start recording transmission
        irparams.rawlen = 0;
        irparams.rawbuf[irparams.rawlen++] = irparams.timer;
        irparams.timer = 0;
        irparams.rcvstate = STATE_MARK;
      }
    }
    break;
  case STATE_MARK: // timing MARK
    if (irdata == SPACE) {   // MARK ended, record time
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_SPACE;
    }
    break;
  case STATE_SPACE: // timing SPACE
    if (irdata == MARK) { // SPACE just ended, record it
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_MARK;
    }
    else { // SPACE
      if (irparams.timer > GAP_TICKS) {
        // big SPACE, indicates gap between codes
        // Mark current code as ready for processing
        // Switch to STOP
        // Don't reset timer; keep counting space width
        irparams.rcvstate = STATE_STOP;
      }
    }
    break;
  case STATE_STOP: // waiting, measuring gap
    if (irdata == MARK) { // reset gap timer
      irparams.timer = 0;
    }
    break;
  }

  if (irparams.blinkflag) {
	/*
    if (irdata == MARK) {
  	  gpio16_output_set(1); // turn pin 16 LED on
    }
    else {
      gpio16_output_set(0); // turn pin 16 LED off
    }
    */
  }
}
////////////////////////
//IRsend::sendNEC
////////////////////////
void ICACHE_FLASH_ATTR sendNEC(unsigned long data, int nbits)
{
  int i;
  enableIROut(38);
  mark(NEC_HDR_MARK);
  space(NEC_HDR_SPACE);
  for (i = 0; i < nbits; i++) {
    if (data & TOPBIT) {
      mark(NEC_BIT_MARK);
      space(NEC_ONE_SPACE);
    }
    else {
      mark(NEC_BIT_MARK);
      space(NEC_ZERO_SPACE);
    }
    data <<= 1;
  }
  mark(NEC_BIT_MARK);
  space(1);
}
////////////////////////
//IRsend::sendSony
////////////////////////
void ICACHE_FLASH_ATTR sendSony(unsigned long data, int nbits) {
  int i;
  enableIROut(40);
  mark(SONY_HDR_MARK);
  space(SONY_HDR_SPACE);
  data = data << (32 - nbits);
  for (i = 0; i < nbits; i++) {
    if (data & TOPBIT) {
      mark(SONY_ONE_MARK);
      space(SONY_HDR_SPACE);
    }
    else {
      mark(SONY_ZERO_MARK);
      space(SONY_HDR_SPACE);
    }
    data <<= 1;
  }
}
////////////////////////
//IRsend::sendRaw
////////////////////////
void ICACHE_FLASH_ATTR sendRaw(unsigned int buf[], int len, int hz)
{
  int i;
  enableIROut(hz);
  for (i = 0; i < len; i++) {
    if (i & 1) {
      space(buf[i]);
    }
    else {
      mark(buf[i]);
    }
  }
  space(1); // Just to be sure
}
////////////////////////
//IRsend::sendRC5
////////////////////////
// Note: first bit must be a one (start bit)
void ICACHE_FLASH_ATTR sendRC5(unsigned long data, int nbits)
{
  int i;
  enableIROut(36);
  data = data << (32 - nbits);
  mark(RC5_T1); // First start bit
  space(RC5_T1); // Second start bit
  mark(RC5_T1); // Second start bit
  for (i = 0; i < nbits; i++) {
    if (data & TOPBIT) {
      space(RC5_T1); // 1 is space, then mark
      mark(RC5_T1);
    }
    else {
      mark(RC5_T1);
      space(RC5_T1);
    }
    data <<= 1;
  }
  space(1); // Turn off at end
}
////////////////////////
//IRsend::sendRC6
////////////////////////
// Caller needs to take care of flipping the toggle bit
void ICACHE_FLASH_ATTR sendRC6(unsigned long data, int nbits)
{
  int i;
  enableIROut(36);
  data = data << (32 - nbits);
  mark(RC6_HDR_MARK);
  space(RC6_HDR_SPACE);
  mark(RC6_T1); // start bit
  space(RC6_T1);
  int t;
  for (i = 0; i < nbits; i++) {
    if (i == 3) {
      // double-wide trailer bit
      t = 2 * RC6_T1;
    }
    else {
      t = RC6_T1;
    }
    if (data & TOPBIT) {
      mark(t);
      space(t);
    }
    else {
      space(t);
      mark(t);
    }

    data <<= 1;
  }
  space(1); // Turn off at end
}
////////////////////////
//IRsend::mark
////////////////////////
void ICACHE_FLASH_ATTR mark(int time) {
  // Sends an IR mark for the specified number of microseconds.
  // The mark output is modulated at the PWM frequency.
  ////TCCR2A |= _BV(COM2B1); // Enable pin 3 PWM output
  int loops;
  int i;
  loops = time/26;
  if(loops==0) loops=1;
  for (i=0;i<loops;i++) {
	  gpio_write(IR_SEND_PIN, 0); //Set GPIO12 LO -> IR On
	  os_delay_us(13);  //1M/38K/2
	  gpio_write(IR_SEND_PIN, 1); //Set GPIO12 HI -> IR Off
	  os_delay_us(13);  //1M/38K/2
  }
}
////////////////////////
//IRsend::space
////////////////////////
/* Leave pin off for time (given in microseconds) */
void ICACHE_FLASH_ATTR space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  ////TCCR2A &= ~(_BV(COM2B1)); // Disable pin 3 PWM output
  gpio_write(IR_SEND_PIN, 1); //Set GPIO12 HI -> IR Off
  os_delay_us(time);
}
////////////////////////
//IRsend::enableIROut
////////////////////////
void ICACHE_FLASH_ATTR enableIROut(int khz) {
  // Enables IR output.  The khz value controls the modulation frequency in kilohertz.
  // Disable the ir_timer Interrupt (which is used for receiving IR)
  os_timer_disarm(&ir_timer);

}

