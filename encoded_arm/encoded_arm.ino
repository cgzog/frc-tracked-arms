//  encoded_arm
//
//  track the position of and report 4 arm joints independently


#include <SPI.h>
#include <SD.h>

#define NUM_OF_POTS     4

#define MIN_A2D         0
#define MAX_A2D         1023

#define ARM_ROT_RANGE   216   // measured in degrees
#define ARM_MIN_ANGLE   (-1 * ARM_ROT_RANGE / 2)
#define ARM_MAX_ANGLE   (ARM_ROT_RANGE / 2)

#define ZERO_PIN_IN     A5

#define LOG_OPEN_SUCCESS 0
#define LOG_OPEN_ERROR   -1



typedef struct {
  int   pin;              // connection
  int   mechMin, mechMax; // measured a2d @ mechanical limits
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
SdFile    rootFile;

const int sdSelect = 10;

void dumpSdCardInfo() {

  Serial.print("Card type:         ");
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
      Serial.println("Unknown cardtype");
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

  Serial.print("\nVolume type is:    FAT");
  Serial.println(sdVolume.fatType(), DEC);
  volumesize = sdVolume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= sdVolume.clusterCount();       // we'll have a lot of clusters
  volumesize /= 2;                             // SD card blocks are always 512 bytes (2 blocks are 1KB)

  Serial.print("Volume size (Kb):  ");
  Serial.println(volumesize);
  Serial.print("Volume size (Mb):  ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Gb):  ");
  Serial.println((float)volumesize / 1024.0);
  
  Serial.println("\nFiles (name, date and size in bytes): ");

  rootFile.openRoot(sdVolume);
  rootFile.ls(LS_R | LS_DATE | LS_SIZE);
  rootFile.close();
}



void setup() {

  Serial.begin(115200);
  while ( ! Serial);                    // wait until serial port is initialized

  pinMode(ZERO_PIN_IN, INPUT_PULLUP);   // when this pin goes down, we re-zero

  Serial.print("\nInitializing SD card...");

  if (!sdCard.init(SPI_HALF_SPEED, sdSelect)) {
    Serial.println("Initialization failed.");
    while (1);
  } else {
    Serial.println("Card detected...\n");
    dumpSdCardInfo();
  }
}


int mapToAngle(int a2dVal) {

}


int append2LogFile(char *filename) {

  File dataFile = SD.open(filename, FILE_WRITE);

  if (dataFile) {
    dataFile.println(filename);
    dataFile.close();
    Serial.println(filename);
  } else {
    Serial.print("Error opening \"");
    Serial.print(filename);
    Serial.println("\"");
    return(LOG_OPEN_ERROR);
  }

  return(LOG_OPEN_SUCCESS);
}
void loop() {

  int   i;
  char  buf[10];


  if (digitalRead(ZERO_PIN_IN) == HIGH) {   // normal operation
    Serial.write("H ");
    
    for (i = 0 ; i < NUM_OF_POTS ; i++) {
      Serial.write("*");
      
      armPots[i].rawPos   = analogRead(armPots[i].pin);
      armPots[i].rawAngle = map(armPots[i].rawPos, armPots[i].mechMin, armPots[i].mechMax, ARM_MIN_ANGLE, ARM_MAX_ANGLE);
      armPots[i].adjAngle = armPots[i].rawAngle - armPots[i].zeroOffset;
      
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
  } else {
    Serial.write("L ");                     // arm zeroing (the set of arms zeroed at once)
    
    for (i = 0 ; i < NUM_OF_POTS ; i++) {
      int current, zero;
      
      current = analogRead(armPots[i].pin);   // the current arm locations are now "zero"
      zero    = map(armPots[i].rawPos, armPots[i].mechMin, armPots[i].mechMax, ARM_MIN_ANGLE, ARM_MAX_ANGLE);
      armPots[i].zeroOffset = zero;
    }  
  }

  Serial.write("\n");

  delay(200);
}
