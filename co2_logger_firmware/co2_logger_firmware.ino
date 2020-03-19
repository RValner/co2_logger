/*
 * This firmware integrates F30 CO2 sensor with DS1307 RTC and SD card module.
 * 
 * The RTC and F30 communicate via I2C on arduino where pinout is:
 *  - SDA  on pin 4 
 *  - SCL  on pin 5
 * NOTE: SDA and SCL require pullup resistors (4.7 kOhm should do it).
 *  
 * The SD card communicates via SPI where pinout is:
 *  - SCK  on pin D13
 *  - MISO on pin D12
 *  - MOSI on pin D11
 *  - CS   on pin D10
 * 
 * More info about F30: https://cdn.shopify.com/s/files/1/0406/7681/files/AN102-GasLab-K30-Sensor-Arduino-I2C.pdf?
 */

#include <SD.h>
#include <Wire.h>
#include <RTClib.h>

#define F30_I2C_ADDR 0x7F // Default address of the CO2 sensor, 7bits shifted left.
#define SD_CS_PIN 10      // Chip Select pin for SD card

char log_file_name[14];
File log_file_handle;     // File handle for logged data
RTC_DS1307 rtc;           // RTC communication object

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int _year, _month, _day, _hour, _min, _sec ;
char log_entry_timestamp[50]; 

/*
 * Declare the functions
 */
void initSDcard();
void newField();
void logData(int& data);
void getTime();
void dataTime(uint16_t* date, uint16_t* time);
int readCO2();

/* * * * * * * * * * * * * * * * * * * * * * * * 
 * SETUP
 * * * * * * * * * * * * * * * * * * * * * * * */
void setup() 
{
  Serial.begin(9600);
  Wire.begin ();

  while (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    delay(1000);
  }

  while (!rtc.isrunning())
  {
    Serial.println("RTC is NOT running!");
    delay(1000);
  }

  Serial.println("RTC is operational");
  newField();
}

/* * * * * * * * * * * * * * * * * * * * * * * * 
 * MAIN LOOP
 * * * * * * * * * * * * * * * * * * * * * * * */
void loop()
{
  int co2Value = readCO2();
  if (co2Value > 0)
  {
    Serial.print("CO2 Value: ");
    Serial.println(co2Value);
    logData(co2Value);
  }
  else
  {
    Serial.println("Checksum failed / Communication failure");
  }
  delay(2000);
}

//-----------------------
void dataTime(uint16_t* date, uint16_t* time)
{
  getTime();
  *date =  FAT_DATE(_year, _month, _day);
  *time =  FAT_TIME(_hour, _min, _sec);           
}

//-----------------------
void getTime()
{
  DateTime now = rtc.now();
  _year = now.year(); 
  _month = now.month();
  _day = now.day();
  _hour = now.hour();
  _min = now.minute();
  _sec = now.second();
}


//-----------------------
void initSDcard(int timeout)
{ 
  // TODO: Add timeout functionality
  while (!SD.begin(SD_CS_PIN))
  {
    Serial.println("SD card initialization failed!");
    Serial.println("Insert CD card  ");
    delay(2000);
  }
}

//-----------------------
void newField()
{  
  initSDcard(3000);
  getTime();
  SdFile::dateTimeCallback(dataTime);
  
  // Create log entry time stamp
  snprintf(log_entry_timestamp, sizeof(log_entry_timestamp)
  , "%04d-%02d-%02d %02d:%02d:%02d"
  , _year, _month, _day, _hour, _min, _sec);
  
  // Field raw data (log) file name
  snprintf(log_file_name, sizeof(log_file_name)
  , "%02d%02d%02d%02d.log"
  , _month, _day, _hour, _min);
 
  log_file_handle = SD.open(log_file_name, FILE_WRITE);
  if (log_file_handle)
  {
    //Serial.print("Writing to ");
    //Serial.println(log_file_name); 
    log_file_handle.println(log_entry_timestamp);
    log_file_handle.println("CO2(ppm)\ttime"); 
    log_file_handle.println();    
    log_file_handle.close();
  } 
  else 
  {
    // if the file didn't open, print an error:
    Serial.print("error opening file:");
    Serial.println(log_file_name);
  }
}

//-----------------------
void logData(int& data)
{
  char lbuf[120];
  char data_char_buf[8];  
  dtostrf(float(data), 4, 2, data_char_buf);
  
  snprintf( lbuf, sizeof(lbuf)
  , "%s\t%02d:%02d:%02d"
  , data_char_buf
  , _hour, _min, _sec);      
  
  log_file_handle = SD.open(log_file_name, FILE_WRITE);
  if (log_file_handle)
  {  
    log_file_handle.println(lbuf);
    log_file_handle.close(); 
  }    
  else
  {
    initSDcard(3000);
  }
}

/*
 * Returns : CO2 Value upon success, 0 upon checksum failure
 * Assumes : - Wire library has been imported successfully.
 *           - CO2 sensor address is defined in co2_addr
 */
int readCO2()
{
  int co2_value = 0;  // We will store the CO2 value inside this variable.

  //////////////////////////
  /* Begin Write Sequence */
  //////////////////////////

  Wire.beginTransmission(F30_I2C_ADDR);
  Wire.write(0x22);
  Wire.write(0x00);
  Wire.write(0x08);
  Wire.write(0x2A);

  Wire.endTransmission();

  /////////////////////////
  /* End Write Sequence. */
  /////////////////////////

  /*
    We wait 10ms for the sensor to process our command.
    The sensors's primary duties are to accurately
    measure CO2 values. Waiting 10ms will ensure the
    data is properly written to RAM
  */

  delay(10);

  /////////////////////////
  /* Begin Read Sequence */
  /////////////////////////

  /*
    Since we requested 2 bytes from the sensor we must
    read in 4 bytes. This includes the payload, checksum,
    and command status byte.

  */

  Wire.requestFrom(F30_I2C_ADDR, 4);

  byte i = 0;
  byte buffer[4] = {0, 0, 0, 0};

  /*
    Wire.available() is not nessessary. Implementation is obscure but we leave
    it in here for portability and to future proof our code
  */
  while (Wire.available())
  {
    buffer[i] = Wire.read();
    i++;
  }

  ///////////////////////
  /* End Read Sequence */
  ///////////////////////

  /*
    Using some bitwise manipulation we will shift our buffer
    into an integer for general consumption
  */

  co2_value = 0;
  co2_value |= buffer[1] & 0xFF;
  co2_value = co2_value << 8;
  co2_value |= buffer[2] & 0xFF;


  byte sum = 0; //Checksum Byte
  sum = buffer[0] + buffer[1] + buffer[2]; //Byte addition utilizes overflow

  if (sum == buffer[3])
  {
    // Success!
    return co2_value;
  }
  else
  {
    // Failure!
    /*
      Checksum failure can be due to a number of factors,
      fuzzy electrons, sensor busy, etc.
    */
    return 0;
  }
}
