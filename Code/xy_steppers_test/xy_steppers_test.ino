#include <Stepper.h>

// set how many steps one command should move (max 300 in one direction for the x-y motors)
#define STEPS 100

// define the ESP pins of the x and y stepper "IN" pins
const int y_1 = 16;  //A+
const int y_2 = 17;  //A-
const int y_3 = 18;  //B-
const int y_4 = 19;  //B+
const int x_1 = 13;  //A+
const int x_2 = 14;  //A-
const int x_3 = 33;  //B-
const int x_4 = 32;  //B+

// create an instance of the stepper class, specifying
// the number of steps of the motor and the pins it's
// attached to
Stepper stepperX = Stepper(STEPS, x_1, x_2, x_4, x_3);
Stepper stepperY = Stepper(STEPS, y_1, y_2, y_4, y_3);

// Create a variable to read the user input to control the motors
char motor_command;

void setup()
{
  Serial.begin(9600);
  Serial.println("Stepper test!");
  // set the speed of the motors, 2 was chosen in the motors.ino code
  stepperX.setSpeed(10); 
  stepperY.setSpeed(10);
}

void loop()
{
  motor_command = '0';  // Reset the command every loop
  
  if (Serial.available()) // Check if a character is available
  {
    motor_command = Serial.read();
  }
  
  if(motor_command == 'r')
  {
    Serial.println("X Right");
    stepperX.step(STEPS);
    TurnStepperCoilsOff();
  }
  else if(motor_command == 'l')
  {
    Serial.println("X Left");
    stepperX.step(-STEPS);
    TurnStepperCoilsOff();
  }
  if(motor_command == 'f')
  {
    Serial.println("Y Forward");
    stepperY.step(-STEPS);
    TurnStepperCoilsOff();
  }
  else if(motor_command == 'b')
  {
    Serial.println("Y Backward");
    stepperY.step(STEPS);
    TurnStepperCoilsOff();
  }
  else if(motor_command == 'o')
  {
    Serial.println("Turning Motor(s) Off");
    TurnStepperCoilsOff();
  }
}

void TurnStepperCoilsOff()
{
  Serial.println("Turning Motor(s) Off");
  
  digitalWrite(x_1,LOW);
  digitalWrite(x_2,LOW);
  digitalWrite(x_3,LOW);
  digitalWrite(x_4,LOW);

  digitalWrite(y_1,LOW);
  digitalWrite(y_2,LOW);
  digitalWrite(y_3,LOW);
  digitalWrite(y_4,LOW);
}
