/*
  Serial is for trace output.
  Serial1 should be connected to the GPS device.
*/

#include <Arduino.h>

#include "Streamers.h"

#include "ubxNMEA.h"

#if !defined( NMEAGPS_PARSE_GGA) & !defined( NMEAGPS_PARSE_GLL) & \
    !defined( NMEAGPS_PARSE_GSA) & !defined( NMEAGPS_PARSE_GSV) & \
    !defined( NMEAGPS_PARSE_RMC) & !defined( NMEAGPS_PARSE_VTG) & \
    !defined( NMEAGPS_PARSE_ZDA ) & \
    !defined( NMEAGPS_PARSE_PUBX_00 ) & !defined( NMEAGPS_PARSE_PUBX_04 )

#if defined(GPS_FIX_DATE)| defined(GPS_FIX_TIME)
#error No NMEA sentences enabled: no fix data available for fusing.
#else
#warning No NMEA sentences enabled: no fix data available for fusing,\n\
 only pulse-per-second is available.
#endif

#endif

#if !defined(GPS_FIX_DATE) & !defined(GPS_FIX_TIME)
static uint32_t seconds = 0L;
#endif

static ubloxNMEA gps;

static gps_fix fused;

//--------------------------

static void poll()
{
  gps.send_P( &Serial1, PSTR("PUBX,00") );
  gps.send_P( &Serial1, PSTR("PUBX,04") );
}

//--------------------------

static void traceIt()
{
#if !defined(GPS_FIX_DATE) & !defined(GPS_FIX_TIME)
  //  Date/Time not enabled, just output the interval number
  trace << seconds << ',';
#endif

  trace << fused;

#if defined(NMEAGPS_PARSE_SATELLITES)
  if (fused.valid.satellites) {
    trace << ',' << '[';

    uint8_t i_max = fused.satellites;
    if (i_max > NMEAGPS::MAX_SATELLITES)
      i_max = NMEAGPS::MAX_SATELLITES;

    for (uint8_t i=0; i < i_max; i++) {
      trace << gps.satellites[i].id;
#if defined(NMEAGPS_PARSE_SATELLITE_INFO)
      trace << ' ' << 
        gps.satellites[i].elevation << '/' << gps.satellites[i].azimuth;
      trace << '@';
      if (gps.satellites[i].tracked)
        trace << gps.satellites[i].snr;
      else
        trace << '-';
#endif
      trace << ',';
    }
    trace << ']';
  }

#else

#ifdef GPS_FIX_SATELLITES
  trace << fused.satellites << ',';
#endif

#endif

  trace << '\n';

} // traceIt

//--------------------------

static void sentenceReceived()
{
  // See if we stepped into a different time interval,
  //   or if it has finally become valid after a cold start.

  bool newInterval;
#if defined(GPS_FIX_TIME)
  newInterval = (gps.fix().valid.time &&
                (!fused.valid.time ||
                 (fused.dateTime.Second != gps.fix().dateTime.Second) ||
                 (fused.dateTime.Minute != gps.fix().dateTime.Minute) ||
                 (fused.dateTime.Hour   != gps.fix().dateTime.Hour)));
#elif defined(GPS_FIX_DATE)
  newInterval = (gps.fix().valid.date &&
                (!fused.valid.date ||
                 (fused.dateTime.Day   != gps.fix().dateTime.Day) ||
                 (fused.dateTime.Month != gps.fix().dateTime.Month) ||
                 (fused.dateTime.Year  != gps.fix().dateTime.Year)));
#else
  //  No date/time configured, so let's assume it's a new interval
  //  if it has been a while since the last sentence was received.
  static uint32_t last_sentence = 0L;
  
  newInterval = (seconds != last_sentence);
  last_sentence = seconds;
#endif
//trace << PSTR("ps mvd ") << fused.valid.date << PSTR("/") << gps.fix().valid.date;
//trace << PSTR(", mvt ") << fused.valid.time << PSTR("/") << gps.fix().valid.time;
//trace << fused.dateTime << PSTR("/") << gps.fix().dateTime;
//trace.print( F("ni = ") ); trace << newInterval << '\n';
//trace << 'v' << gps.fix().valid.as_byte << '\n';

  if (newInterval) {

    // Log the previous interval
    traceIt();

    //  Since we're into the next time interval, we throw away
    //     all of the previous fix and start with what we
    //     just received.
    fused = gps.fix();

  } else {
    // Accumulate all the reports in this time interval
    fused |= gps.fix();
  }

} // sentenceReceived


//--------------------------

void setup()
{
  // Start the normal trace output
  Serial.begin(9600);
  trace.print( F("NMEAUBX: started\n") );
  trace.print( F("fix object size = ") );
  trace.println( sizeof(gps.fix()) );
  trace.print( F("NMEAGPS object size = ") );
  trace.println( sizeof(NMEAGPS) );
  trace.flush();
  
  // Start the UART for the GPS device
  Serial1.begin(9600);
  poll();
}

//--------------------------

void loop()
{
  while (Serial1.available())
    if (gps.decode( Serial1.read() ) == NMEAGPS::DECODE_COMPLETED) {
//      trace << ((uint8_t) gps.nmeaMessage) << ' ';
      sentenceReceived();

// Make sure that the only sentence we care about is enabled
#ifndef NMEAGPS_PARSE_PUBX_00
#error NMEAGPS_PARSE_PUBX_00 must be defined in ubxNMEA.h!
#endif

      if (gps.nmeaMessage == (NMEAGPS::nmea_msg_t) ubloxNMEA::PUBX_00) {
#if !defined(GPS_FIX_DATE) & !defined(GPS_FIX_TIME)
        //  No date/time fields enabled, use received GPRMC sentence as a pulse
        seconds++;
#endif
        poll();
      }
    }
}