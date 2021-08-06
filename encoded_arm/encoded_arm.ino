//  encoded_arm
//
//  track the position of and report 4 arm joints independently
//
// supports zeroing the arms - arms should be zeroed when all pointing
// straight up for normal operation
//
// debug shunt can be used to pull down pin 7 to disable debug messages

#include <SPI.h>
#include <SD.h>


#define NUM_OF_POTS     4

#define MIN_A2D         0
#define MAX_A2D         1023

#define ARM_ROT_RANGE   216     // measured in degrees
#define ARM_MIN_ANGLE   (-1 * ARM_ROT_RANGE / 2)
#define ARM_MAX_ANGLE   (ARM_ROT_RANGE / 2)

#define ARM_READ_PACE   200   // milliseconds

#define ZERO_PIN_IN     A5
#define ZERO_IN_ACTIVE  LOW

#define DEBUG_PIN_IN    7
#define DEBUG_IN_ACTIVE LOW

#define SD_SELECT_PIN   10

#define LOG_OPEN_SUCCESS 0
#define LOG_OPEN_ERROR   -1
#define LOG_FILE_NAME   "ARMS.TXT"



typedef struct {
  int   pin;              // connection
  int   mechMin, mechMax; // measured a2d @ each of the mechanical limits
  int   rawPos;           // unadjusted measured position
  int   rawAngle;         // map A2d to an angle
  int   zeroOffset;       // angle offset
  int   adjAngle;         // adjusted based on the zero
 
} POTS;

POTS  armPots[NUM_OF_POTS] = {       // shoulder to wrist
  { A0, 160, 976, 0, 0, 0, 0},
  { A1, 160, 984, 0, 0, 0, 0},
  { A2, 128, 933, 0, 0, 0, 0},
  { A3, 198, 999, 0, 0, 0, 0}
};


Sd2Card   sdCard;
SdVolume  sdVolume;
SdFile    sdRoot;

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

  pinMode(ZERO_PIN_IN, INPUT_PULLUP);   // when this pin goes down, we re-zero
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
}


int append2LogFile(const char *filename, POTS currentInfo[], int num) {

  String        record = "";
  unsigned long myTime;
  char          timeBuff[12];     // enough to hold into single digit billions
  
  File dataFile = SD.open(filename, FILE_WRITE);

  if (dataFile) {

    myTime = millis();
    
    ltoa(myTime, timeBuff, 10);   // convert current milliseconds to base 10 time
    record = timeBuff;      // timestamp
    record += " ";
    
    for (int i= 0 ; i < num ; i++) {
      record += String(currentInfo[i].adjAngle) + " ";
    }
    dataFile.println(record);
    dataFile.close();
    if (debugFlag) {
      Serial.println(record);
    }
  } else {
    Serial.print("Error opening \"");
    Serial.print(filename);
    Serial.println("\"");
    return(LOG_OPEN_ERROR);
  }

  return(LOG_OPEN_SUCCESS);
}


void loop() {

  int           i;
  unsigned long currentMillis, readDelay;
  
  currentMillis = millis();
    
  if ( ! (digitalRead(ZERO_PIN_IN) == ZERO_IN_ACTIVE)) {   // normal operation
    
    for (i = 0 ; i < NUM_OF_POTS ; i++) {
      
      armPots[i].rawPos   = analogRead(armPots[i].pin);
      armPots[i].rawAngle = map(armPots[i].rawPos, armPots[i].mechMin, armPots[i].mechMax, ARM_MIN_ANGLE, ARM_MAX_ANGLE);
      armPots[i].adjAngle = armPots[i].rawAngle - armPots[i].zeroOffset;
      
      if (0) {
        
        char          buf[10];

        Serial.write("*");
        Serial.write(itoa(armPots[i].rawPos, buf, 10));
        Serial.write(" (");
        Serial.write(itoa(armPots[i].mechMin, buf, 10));
        Serial.write(", ");
        Serial.write(itoa(armPots[i].mechMax, buf, 10));
        Serial.write(") ");
        Serial.write(itoa(armPots[i].zeroOffset, buf, 10));
        Serial.write(" ");
        Serial.write(itoa(armPots[i].adjAngle, buf, 10));
        Serial.write("* ");
      }
    }

    if (0) {
        Serial.write("\n");
    }

    append2LogFile(LOG_FILE_NAME, armPots, NUM_OF_POTS);
    
  } else {
    if (debugFlag) {
      Serial.write("Z\n");                     // arm zeroing (the set of arms are all zeroed at once)
    }
    
    for (i = 0 ; i < NUM_OF_POTS ; i++) {
      int current, zero;
      
      current = analogRead(armPots[i].pin);   // the current arm locations are now "zero"
      zero    = map(armPots[i].rawPos, armPots[i].mechMin, armPots[i].mechMax, ARM_MIN_ANGLE, ARM_MAX_ANGLE);
      armPots[i].zeroOffset = zero;
    }  
  }

  // calculate the millis used in the current loop and adjust the delay to hit our read pace as cloase as practical
  delay(ARM_READ_PACE - (millis() - currentMillis));
}
