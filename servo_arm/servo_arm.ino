//  servo_arms
//
//  take input and control 4 servo joints independently

#define NUM_OF_SERVOS   4

#define MIN_SERVO_POS   0
#define MAX_SERVO_POS   180
#define MID_SERVO_POS   ((MIN_SERVO_POS + MAX_SERVO_POS) / 2)

#define SERVO_DELAY     10    // lots of routines use longer delays but we'll be
                              // setting each servo going through a loop each time
                              // (theerby setting each servo in the entire arm) so the
                              // delay bewteen hitting the same servio will be > than
                              // 4X this value

#include <Servo.h>

typedef struct {
  int   pin;          // connection
  Servo *servo;       // servo object
  int   pos;          // current position
  int   zeroOffset;   // offset applied to midpoint to be straight
} SERVOS;

Servo servo0, servo1, servo2, servo3;   // shoulder to wrist

SERVOS  armServos[NUM_OF_SERVOS] = {
  {  6, &servo0, MID_SERVO_POS,  3 },
  {  9, &servo1, MID_SERVO_POS,  3 },
  { 10, &servo2, MID_SERVO_POS,  4 },
  { 11, &servo3, MID_SERVO_POS, -2 }
};




void setup() {
  int i;

  for (i = 0 ; i < NUM_OF_SERVOS ; i++) {
    armServos[i].servo->attach(armServos[i].pin);
    armServos[i].servo->write(MID_SERVO_POS + armServos[i].zeroOffset);
    delay(SERVO_DELAY);
  }
}

void loop() {

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
