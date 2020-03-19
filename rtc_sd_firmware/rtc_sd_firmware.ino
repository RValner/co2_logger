#include <SD.h>
#include <Wire.h>
#include <RTClib.h>

#define SD_CS_PIN 10

File myFile;
RTC_DS1307 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//int expTime[3];
int _year, _month, _day, _hour, _min, _sec ;
char buf[50];
char log_file[14]; 

void initSDcard();
void newField();
void logData();
void getTime();
void dataTime(uint16_t* date, uint16_t* time);

void setup()
{
  Wire.begin();             // Join I2c Bus as master
  Serial.begin(9600);
  if (! rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    while (1)
    {
      delay(3000);
    }
  }
  
  if (! rtc.isrunning())
  {
    Serial.println("RTC is NOT running!");
  }

  newField();  
}

void loop() 
{
  logData();
  delay(1000);
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
 // Field measerement start time
   snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           _year, _month, _day,
           _hour, _min, _sec);
 // Field raw data (log) file name
  snprintf(log_file, sizeof(log_file), "%02d%02d%02d%02d.log",
            _month, _day, _hour, _min);
 
  
  Serial.println(buf);
  Serial.println();
  myFile = SD.open(log_file, FILE_WRITE);
  if (myFile)
  {
    //Serial.print("Writing to ");
    //Serial.println(log_file); 
    myFile.println(buf);
    myFile.println("T_amb_1\tT_obj_1\tT_amb_2\tT_obj_2\ttime"); 
    myFile.println();    
    myFile.close();
  } 
  else 
  {
    // if the file didn't open, print an error:
    Serial.print("error opening file:");
    Serial.println(log_file);
  }
  
}

//-----------------------
void logData()
{
  char lbuf[120];
  char str_T_obj_1[8];
  char str_T_obj_2[8];
  char str_T_amb_1[8];
  char str_T_amb_2[8];
  
  dtostrf(11.11, 4, 2, str_T_amb_1);
  dtostrf(22.22, 4, 2, str_T_obj_1);
  dtostrf(33.33, 4, 2, str_T_amb_2);
  dtostrf(44.44, 4, 2, str_T_obj_2);
  
  snprintf( lbuf, sizeof(lbuf), "%s\t%s\t%s\t%s\t%02d:%02d:%02d",
            str_T_amb_1, str_T_obj_1, str_T_amb_2, str_T_obj_2,
            _hour, _min, _sec);      
  //Serial.println(lbuf);
  
  myFile = SD.open(log_file, FILE_WRITE);
  if (myFile)
  {  
    myFile.println(lbuf);
    myFile.close(); 
  }    
  else initSDcard(3000);

}
