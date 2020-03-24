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
#include <LiquidCrystal.h>
#include <RTClib.h>
#include "co2_logger_state.h"

#define F30_I2C_ADDR 0x7F         // Default address of the CO2 sensor, 7bits shifted left.
#define SD_CS_PIN 10              // Chip Select pin for SD card
#define SD_CARD_INIT_TIMEOUT 2000
#define SAMPLING_PERIOD 5000

/*
 * Structure that represents the logger
 */
struct CO2Logger
{
  // Default constructor
  CO2Logger()
  {
    rtc_state.setParentStatePointer(&system_state);
    sd_card_state.setParentStatePointer(&system_state);
    log_file_state.setParentStatePointer(&system_state);
  }

  Status::Status getSystemState()
  {
    return system_state.status;
  }

  Status::Status getRtcState()
  {
    return rtc_state.status;
  }

  Status::Status getSdCardState()
  {
    return sd_card_state.status;
  }

  Status::Status getLogFileState()
  {
    return log_file_state.status;
  }

  void setMessage(char* message_in)
  {
    // TODO: Make sure that the 'message_in' is not larger than 'message'
    snprintf(message, sizeof(message), "%s", message_in);
  }

  State system_state;
  State rtc_state;
  State sd_card_state;
  State log_file_state;
  
  char message[16];
};

CO2Logger co2_logger;
char log_file_name[20];
RTC_DS1307 rtc;                   // RTC communication object
LiquidCrystal lcd(3, 4, 5, 6, 7, 8);

int _year, _month, _day, _hour, _min, _sec ;
char log_entry_timestamp[20]; 

/*
 * Declare the functions
 */
bool initSdCard(unsigned int timeout);
bool initLogFile();
bool logData(int& data);
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
  lcd.begin(16, 2);
  co2_logger.system_state.setToInitialize();
}

/* * * * * * * * * * * * * * * * * * * * * * * * 
 * MAIN LOOP
 * * * * * * * * * * * * * * * * * * * * * * * */
void loop()
{
  /*
   * Check the state machine
   */
  switch(co2_logger.getSystemState())
  {
    /* * * * * * * * * * * * * * *
     * Initialize
     * * * * * * * * * * * * * * */
    case Status::INITIALIZE:
    {
      /*
       * Initialize the Real Time Clock
       */
      if (co2_logger.getRtcState() != Status::WORKING)
      {
        // Start the RTC
        if (!rtc.begin())
        {
          co2_logger.rtc_state.setToError();
          co2_logger.setMessage("RTC NOT FOUND");
          break; 
        }

        // Check if RTC is running
        if (!rtc.isrunning())
        {
          co2_logger.rtc_state.setToError();
          co2_logger.setMessage("RTC NOT RUNNING");
          break;
        }

        co2_logger.rtc_state.setToWorking();
        Serial.println("RTC is operational");
      }

      /*
       * Initialize the SD card
       */
      if (co2_logger.getSdCardState() != Status::WORKING)
      {
        if (!initSdCard(SD_CARD_INIT_TIMEOUT))
        {
          co2_logger.sd_card_state.setToError();
          co2_logger.setMessage("INSERT SD CARD");
          break;
        }

        co2_logger.sd_card_state.setToWorking();
        Serial.println("SD card is operational");
      }

      /*
       * Initialize the log file card
       */
      if (co2_logger.getLogFileState() != Status::WORKING)
      {
        if (!initLogFile())
        {
          co2_logger.log_file_state.setToError();
          co2_logger.setMessage("CAN'T OPEN FILE");
          break;
        }

        co2_logger.log_file_state.setToWorking();
        Serial.println("Log file has been initialized");
      }

      /*
       * All components are operational, start logging
       */
      co2_logger.system_state.setToWorking();
    }
    break;

    /* * * * * * * * * * * * * * *
     * Working
     * * * * * * * * * * * * * * */
    case Status::WORKING:
    {
      int co2Value = readCO2();
      lcd.clear();
      lcd.setCursor(0, 0);
      
      if (co2Value > 0)
      { 
        if (!logData(co2Value))
        {
          co2_logger.log_file_state.setToError();
          co2_logger.setMessage("LOGGING ISSUE");
          break;
        }
        else
        {
          Serial.print("CO2 Value: ");
          Serial.println(co2Value);

          // Create log entry time stamp
          char current_timestamp[20];
          snprintf(current_timestamp, sizeof(current_timestamp)
          , "%04d-%02d-%02d %02d:%02d:%02d"
          , _year, _month, _day, _hour, _min, _sec);
          
          lcd.print("CO2: ");
          lcd.print(co2Value);
          lcd.print(" ppm");
          lcd.setCursor(0, 1);
          lcd.print(current_timestamp);
          
          delay(SAMPLING_PERIOD);
        }
      }
      else
      {
        lcd.print("NO CO2 SENSOR");
        Serial.println("Checksum failed / Communication failure");
        delay(100);
      }
    }
    break;

    /* * * * * * * * * * * * * * *
     * Error
     * * * * * * * * * * * * * * */
    case Status::ERROR:
    { 
      // TODO: Print message to LCD diaplay
      Serial.println(co2_logger.message);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(co2_logger.message);
      co2_logger.system_state.setToInitialize();
      delay(1000);
    }
    break;

    /* * * * * * * * * * * * * * *
     * Default
     * * * * * * * * * * * * * * */
    default:
    {
      // Got into an invalid state ... help
      co2_logger.setMessage("I AM BROKEN");
      // TODO: Print message to LCD diaplay
    }
  }
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
bool initSdCard(unsigned int timeout)
{ 
  unsigned int start_time = millis();
  unsigned int duration = 0;
  bool sd_init_success = SD.begin(SD_CS_PIN);
  
  while ( (duration <= timeout) && !sd_init_success)
  {
    sd_init_success = SD.begin(SD_CS_PIN);
    duration = millis() - start_time;
  }
  
  return sd_init_success;
}

//-----------------------
bool initLogFile()
{
  if (!initSdCard(SD_CARD_INIT_TIMEOUT))
  {
    co2_logger.sd_card_state.setToError();
    return false;  
  }
  
  getTime();
  SdFile::dateTimeCallback(dataTime);
  
  // Create log entry time stamp
  snprintf(log_entry_timestamp, sizeof(log_entry_timestamp)
  , "%04d-%02d-%02d %02d:%02d:%02d"
  , _year, _month, _day, _hour, _min, _sec);
  
  // Field raw data (log) file name
  snprintf(log_file_name, sizeof(log_file_name)
  , "co2_%02d%02d.log"
  , _month, _day);

  bool file_exists = SD.exists(log_file_name);
  File log_file_handle = SD.open(log_file_name, FILE_WRITE);
  if (log_file_handle)
  {
    if (!file_exists)
    {
      Serial.println("Created new log file.");
      log_file_handle.println(log_entry_timestamp);
      log_file_handle.println("CO2(ppm)\ttime"); 
      log_file_handle.println();    
      log_file_handle.close();
    }
    else
    {
      Serial.println("Logging data to existing file.");
      log_file_handle.close();
    }
    return true;
  } 
  else 
  {
    // if the file didn't open, print an error:
    Serial.print("error opening file: ");
    Serial.println(log_file_name);
    log_file_handle.close();
    return false;
  }
}

//-----------------------
bool logData(int& data)
{
  char lbuf[20];
  char data_char_buf[8];  
  dtostrf(float(data), 4, 2, data_char_buf);
  
  snprintf( lbuf, sizeof(lbuf)
  , "%s\t%02d:%02d:%02d"
  , data_char_buf
  , _hour, _min, _sec);      

  File log_file_handle = SD.open(log_file_name, FILE_WRITE);
  if (log_file_handle)
  {  
    log_file_handle.println(lbuf);
    log_file_handle.close(); 
    return true;
  }    
  else
  {
    log_file_handle.close();
    return false;
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
