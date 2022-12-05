/*
 * Created by Iain Mott
 * updated by Gaël Jaton
 *  
 * Official BNO055 support for Arduino can be downloaded form 
 * https://github.com/arduino-libraries/NineAxesMotion/archive/master.zip 
 * and unziped in your Arduino home folder
 */

// #define GPS      // Use the Neo GPS module
#define SIDEWAYS // For a device rotated by 90 degrees on Headphones
// #define DEBUG    // Print out human readable data

#include "NAxisMotion.h" 
#include <Wire.h>

#ifdef GPS

// The NeoGPS and AltSoftSerial Library can be installed through the Library manager found in the /Tools tab or by pressing "Ctl+Maj+I"
#include <NMEAGPS.h>       
#include <AltSoftSerial.h> 

NMEAGPS       gps;
gps_fix       fix;
bool          receivedFix = false;

static const uint32_t GPSBaud = 9600;
AltSoftSerial gpsPort;  //  only use RXPin = 9, TXPin = 8

const unsigned char UBLOX_INIT[] PROGMEM = {
  // Rate (pick one)
  // 0xB5,0x62,0x06,0x08,0x06,0x00,0x64,0x00,0x01,0x00,0x01,0x00,0x7A,0x12,              // (10Hz)
  0xB5,0x62,0x06,0x08,0x06,0x00,0xC8,0x00,0x01,0x00,0x01,0x00,0xDE,0x6A,                 // (5Hz)
  // 0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xFA, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x96, // (4Hz)
  // 0xB5,0x62,0x06,0x08,0x06,0x00,0xE8,0x03,0x01,0x00,0x01,0x00,0x01,0x39,              // (1Hz)

  // Disable specific NMEA sentences
  0xB5,0x62,0x06,0x01,0x08,0x00,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x24,    // GxGGA off
  // 0xB5,0x62,0x06,0x01,0x08,0x00,0xF0,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x2B, // GxGLL off
  0xB5,0x62,0x06,0x01,0x08,0x00,0xF0,0x02,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x32,    // GxGSA off
  // 0xB5,0x62,0x06,0x01,0x08,0x00,0xF0,0x03,0x00,0x00,0x00,0x00,0x00,0x01,0x03,0x39, // GxGSV off
  // 0xB5,0x62,0x06,0x01,0x08,0x00,0xF0,0x04,0x00,0x00,0x00,0x00,0x00,0x01,0x04,0x40, // GxRMC off
  0xB5,0x62,0x06,0x01,0x08,0x00,0xF0,0x05,0x00,0x00,0x00,0x00,0x00,0x01,0x05,0x47,    // GxVTG off
  0xB5,0x62,0x06,0x01,0x08,0x00,0xF0,0x08,0x00,0x00,0x00,0x00,0x00,0x01,0x08,0x5C,    // GxZDA off
  // turn off warnings
  0x06,0x02,10,0,1,0,0,0,0,0,0,0,0,0
};

struct message_t
{
  uint16_t heading;
  uint16_t roll;
  uint16_t pitch;
  int32_t lat;
  int32_t lon;
  int32_t alt;
};

#else

struct message_t
{
  uint16_t heading;
  uint16_t roll;
  uint16_t pitch;
}; 

#endif

message_t message;

NAxisMotion mySensor;
unsigned long lastStreamTime = 0;     // the last streamed time stamp
const int streamPeriod = 20;          // stream at 50Hz (time period(ms) =1000/frequency(Hz))

void setup()
{
  Serial.begin(115200);

#ifdef GPS
  
  gpsPort.begin(GPSBaud);

  for (size_t i = 0; i < sizeof(UBLOX_INIT); i++) {                        
    gpsPort.write( pgm_read_byte(UBLOX_INIT+i) );
  };
  
#endif

  I2C.begin();

  mySensor.initSensor();   // The I2C Address can be inside this function in the library
  mySensor.setOperationMode(OPERATION_MODE_NDOF);
  mySensor.setUpdateMode(MANUAL);
    // The default is AUTO. Changing to MANUAL requires calling the 
    // relevant update functions prior to calling the read functions
    // Setting to MANUAL requires fewer reads to the sensor  
}

void loop()
{

#ifdef GPS

  if (gps.available( gpsPort )) {
    fix = gps.read();

    if (fix.valid.location) {
      message.lat = fix.latitudeL();
      message.lon = fix.longitudeL();
      message.alt = fix.altitude_cm(); 
    }
    receivedFix = true;
  }

  if ((millis() > 5000) && !receivedFix) {
    Serial.println( F("No GPS detected: check wiring.") );
    while(true);
  }

#endif

  if ((millis() - lastStreamTime) >= streamPeriod)
  {
    lastStreamTime = millis();    
    mySensor.updateEuler();

    float headingf = mySensor.readEulerHeading();  
    
    headingf = headingf * PI / 180; // convert heading to radians

    float rollf = mySensor.readEulerRoll();

    rollf = rollf * PI / 180; // convert to radians

    float pitchf = mySensor.readEulerPitch();

    pitchf = pitchf * PI / 180; // convert to radians

#ifdef SIDEWAYS
    // note pitch and roll are swapped below because I rotate z axis of device by 90 degrees 
    message.heading = (headingf + (3 * PI) / 2) * 100;
    message.pitch = (rollf + PI) * 100; 
    message.roll = (pitchf + PI) * 100;
#else
    message.heading = (headingf + PI) * 100;
    message.pitch = (pitchf + PI) * 100; 
    message.roll = (rollf + PI) * 100;
#endif

#ifdef DEBUG
    Serial.print("Heading = ");  
    Serial.print(message.heading);
    Serial.print(". Pitch = ");  
    Serial.print(message.pitch);
    Serial.print(". Roll = ");  
#ifdef GPS
    Serial.print(message.roll);
    Serial.print("\t Latitude = ");  
    Serial.print(message.lat);
    Serial.print(". Longitude = ");  
    Serial.print(message.lon);
    Serial.print(". Altitude = ");  
    Serial.println(message.alt);
#else
    Serial.println(message.roll);
#endif
#else
    Serial.write(251);
    Serial.write(252); 
    Serial.write(253); 
    Serial.write(254);
    Serial.write( (uint8_t *) &message, sizeof(message) );
    Serial.write(255);    
#endif

  }
}
