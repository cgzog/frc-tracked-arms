//  servo_arms
//
//  take positional input and control 4 servo joints independently
//
// debug shunt can be used to pull down pin 7 to disable debug messages

#include <SPI.h>
#include <SD.h>
#include <Servo.h>


#define NUM_OF_SERVOS   4

#define MIN_SERVO_POS   0
#define MAX_SERVO_POS   180
#define MID_SERVO_POS   ((MIN_SERVO_POS + MAX_SERVO_POS) / 2)

#define SERVO_DELAY     10    // lots of routines use longer delays but we'll be
                              // setting each servo going through a loop each time
                              // (thereby setting each servo in the entire arm) so the
                              // delay bewteen hitting the same servio will be > than
                              // 4X this value

#define DEBUG_PIN_IN    7
#define DEBUG_IN_ACTIVE LOW

#define SD_SELECT_PIN   10

#define LOG_OPEN_SUCCESS 0
#define LOG_OPEN_ERROR   -1
#define LOG_FILE_NAME   "arms.txt"

#define READ_BUF_SIZE   50

#define READ_OK         1
#define READ_ERROR      0


typedef struct {
  int   pin;          // connection
  Servo *servo;       // servo object
  int   pos;          // current position
  int   zeroOffset;   // offset applied to midpoint to be straight
} SERVOS;

Servo servo0, servo1, servo2, servo3;   // shoulder to wrist

SERVOS  armServos[NUM_OF_SERVOS] = {
  { 6, &servo0, MID_SERVO_POS,  3 },
  { 9, &servo1, MID_SERVO_POS,  3 },
  { 5, &servo2, MID_SERVO_POS,  4 },
  { 3, &servo3, MID_SERVO_POS, -2 }
};


Sd2Card   sdCard;
SdVolume  sdVolume;
SdFile    sdRoot;

File      datafile;

int       debugFlag = 0;



void dumpSdCardInfo() {
  
  Serial.println("\nCard ------------------------------------------");
  Serial.print("Type:              ");
  switch (sdCard.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;

    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;

    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;

    default:
      Serial.println("Unknown card type");
  }

  if (!sdVolume.init(sdCard)) {
    Serial.println("Could not find a FAT16/FAT32 partition.  Make sure the card is formatted.");
    while (1);
  }

  Serial.print("Clusters:          ");
  Serial.println(sdVolume.clusterCount());

  Serial.print("Blocks x Cluster:  ");
  Serial.println(sdVolume.blocksPerCluster());

  Serial.print("Total Blocks:      ");
  Serial.println(sdVolume.blocksPerCluster() * sdVolume.clusterCount());

  uint32_t volumesize;
  
  Serial.println("\nVolume ----------------------------------------");
  Serial.print("Type:              FAT");
  Serial.println(sdVolume.fatType(), DEC);
  volumesize = sdVolume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= sdVolume.clusterCount();       // we'll have a lot of clusters
  volumesize /= 2;                             // SD card blocks are always 512 bytes (2 blocks are 1KB)

  Serial.print("Size:              ");
  Serial.print(volumesize);
  Serial.print("Kb - ");
  volumesize /= 1024;
  Serial.print(volumesize);
  Serial.print("Mb - ");
  Serial.print((float)volumesize / 1024.0);
  Serial.println("Gb");

  Serial.println("\nFiles (name, date, bytes) ---------------------");

  sdRoot.openRoot(sdVolume);
  sdRoot.ls(LS_R | LS_DATE | LS_SIZE);
  sdRoot.close();

  Serial.println("-----------------------------------------------\n");
}

void setup() {
  
  Serial.begin(115200);
  while ( ! Serial);                    // wait until serial port is initialized

  pinMode(DEBUG_PIN_IN, INPUT_PULLUP);  // when this pin goes down, display debug information

#ifdef  DUMP_SD_CARD_INFO

// if enabled, normal card library through the SD library doesn't work anymore
 
  if ( ! sdCard.init(SPI_HALF_SPEED, SD_SELECT_PIN)) {
    Serial.println("sdCard initialization failed");
    while (1);
  } else {
    Serial.println("Card detected");
    dumpSdCardInfo();
  }
  
  while (1);    // just keep looping...
  
#endif  // DUMP_SD_CARD_INFO


  // normal initialization

  if (digitalRead(DEBUG_PIN_IN) == DEBUG_IN_ACTIVE) {
    debugFlag = 1;
    Serial.print("Initializing SD card... ");
  }
  
  if ( ! SD.begin(SD_SELECT_PIN)) {
    if (debugFlag) {
      Serial.println("SD initialization failed");
    }
    while (1);
  } else {
    if (debugFlag) {
      Serial.println("card detected.");
    }
  }

  // set the initial servo positions - all vertical
  //
  // this might not even be needed, we probably could just go directly to the
  // first position
  
  int i;

  for (i = 0 ; i < NUM_OF_SERVOS ; i++) {
    armServos[i].servo->attach(armServos[i].pin);
    armServos[i].servo->write(MID_SERVO_POS + armServos[i].zeroOffset);
    delay(SERVO_DELAY);
  }

  datafile = SD.open(LOG_FILE_NAME, FILE_READ);

  if ( ! datafile) {
    Serial.print("Cannot open \"");
    Serial.print(LOG_FILE_NAME);
    Serial.println("\"");

    while (1);
  }
}



// readRecord   get a line from the data file and parse the fields:
//
// timestamp arms angle (4 - shoulder to end)
//
// comment lines start with '#' in first column

int readRecord(unsigned long *timeStamp, int angles[], int numOfAngles) {

  char  readBuf[READ_BUF_SIZE], *readPtr;
  int   readOK;

  readPtr = readBuf;
  
  do {
    while (readOK = datafile.read(readPtr, 1)) {

      //  skip carriage return
      if (*readPtr == '\r') {
        continue;     // don't advance readPtr so '\r' will be overwritten
      }

      // newline ends the reading of the line
      if (*readPtr == '\n') {
        *readPtr = '\0';    // terminate the read string
        break;
      }
    
      readPtr++;
    }
  } while (readOK && readBuf[0] == '#');  // skip comment lines

  if ( ! readOK) {
    if (debugFlag) {
      Serial.println("<EOF>");
    }
    return(READ_ERROR);
  } else {
    if (debugFlag) {
      Serial.print("Read line >");
      Serial.print(readBuf);
      Serial.println("<");
    }
  }

  // we've read a line - now break it down

  *timeStamp = atol(readBuf);

  if (debugFlag) {
    Serial.print("timestamp: ");
    Serial.println(*timeStamp);
    Serial.println("angles:");
  }

  // process the angle values

  readPtr = readBuf;

  int i;
  
  for (i = 0 ; i < numOfAngles ; i++) {

    // skip the just processed numeric value and the subsequent white space
    while (isdigit(*readPtr) || *readPtr == '-') {    // be sure to handle minus signs!
      readPtr++;
    }

    while (isspace(*readPtr)) {
      readPtr++;
    }

    angles[i] = atoi(readPtr);
    
    if (debugFlag) {
      Serial.println(angles[i]);
    }
  }

  return(READ_OK);
}

void loop() {

  unsigned long timestamp, currentTime, prevTime;
  int i, angles[NUM_OF_SERVOS];
  
  // at this point the data file is open - process it line by line
  // extracting the data values:
  //
  // timestamp (in ms) & arms angles for all 4 joints (starting at "shoulder" (base end)

  currentTime = 0;
  
  while (readRecord(&timestamp, angles, NUM_OF_SERVOS)) {

    /* 
    prevTime = timestamp;
    
    if ( ! currentTime) {
      // first time through so delay until the read timestamp unless
      // it's already passed
      currentTime = millis();

      if ((timestamp - currentTime) > 0) {
        delay(timestamp - currentTime);   // wait until it's time
      }
    } else {
      currentTime = millis();
      delay(timestamp - currentTime);
    }
    */
    
    for (i = 0 ; i < NUM_OF_SERVOS ; i++) {
      armServos[i].servo->write(MID_SERVO_POS - angles[i]);
    }
    Serial.print("loop(): ");
    Serial.print(timestamp);
    Serial.print(" ");
    Serial.print(angles[0]);
    Serial.print(" ");
    Serial.print(angles[1]);
    Serial.print(" ");
    Serial.print(angles[2]);
    Serial.print(" ");
    Serial.println(angles[3]);       
  }

  while (1);
  
 /*

  for (pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
  }
  for (pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    myservo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(15);                       // waits 15ms for the servo to reach the position
  }
  */
  
}
