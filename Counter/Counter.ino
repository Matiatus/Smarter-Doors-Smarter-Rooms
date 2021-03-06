#include <Vector.h>
#include <SPI.h>
#include <Ethernet.h>
#include <BlynkSimpleEthernet.h>

// PINS AND VARIABLES
int calibDataAmount = 5;

// PhotoResistor_Inside Pin and Variables
int photoRes_Ins = A0;
bool laserInsBreak = false;
int initialInside;

// PhotoResistor_Outside Pin and Variables
int photoRes_Out = A1;
bool laserOutBreak = false;
int initialOutside;

// Ultrasonic Sensor 1 Pins and Variables
int echoPin_1 = 9;
int trigPin_1 = 8;
int initDist_1;

//Ultrasonic Sensor 2 Pins and Variables
int echoPin_2 = 7;
int trigPin_2 = 6;
int initDist_2;

int lightEnable = 5; // Pin used to control Relay, HIGH => Lights are turned off

// Global Variables for our system
int doorWidth;
int peopleCount = 0;
int peopleThreshold = 65; // Threshold for width of one person
int deltaPerson;
bool *x1, *x2; // Laser order in terms of break time
int laserAssign; // Keeps track of *x1 and *x2 assignment

// Blynk Related Global Variables
int resetPin, resetCount, emergencyPin; // Flags used by Reset and Emergency Buttons

bool delayActive; // Flag for Delay and Neccessity of Relay control
int delayPostpone;
int delayAmount; // Parameters used to delay for required seconds

WidgetLED resetLed(V14), lightLed(V15), alarmLed(V16), clearLed(V17);
char auth[] = "cf2e633dd52241ad98537e83ed17985b"; // Our Blynk Token

 
void setup() {
  // Setting up Pins
  pinMode(echoPin_1, INPUT);
  pinMode(trigPin_1, OUTPUT);
  pinMode(echoPin_2, INPUT);
  pinMode(trigPin_2, OUTPUT);
  pinMode(photoRes_Ins, INPUT);
  pinMode(photoRes_Out, INPUT);
  pinMode(lightEnable, OUTPUT);
  Serial.begin(9600);
  Blynk.begin(auth);
  Blynk.run();
  calibrateValues(); // Callibrate our initial values
  displayCount();
  Blynk.virtualWrite(V6, "add", 7, "Exhibition Room", 0);
  // Giving random room numbers for table view, used to show the future potential
  Blynk.virtualWrite(V6, "add", 1, "Room Number", "Count");
  Blynk.virtualWrite(V6, "add", 2, "Room 1", "xxxx");
  Blynk.virtualWrite(V6, "add", 3, "Room 2", "xxxx");
  Blynk.virtualWrite(V6, "add", 4, "Room 3", "xxxx");
  Blynk.virtualWrite(V6, "add", 5, "Room 4", "xxxx");
  Blynk.virtualWrite(V6, "add", 6, "Room 5", "xxxx");
  Blynk.virtualWrite(V6, "add", 8, "Room 6", "xxxx");
  Blynk.virtualWrite(V6, "pick", 1);
  alarmLed.off();
}

void loop() {
  Blynk.run(); // Runs the server communication with Blynk
  if(resetPin != 0){
    calibrateValues();
    peopleCount = resetCount;
    resetPin = 0;
  }
  resetParameters(); 
  checkLasers();
  // State_0 Begins here
  if(laserInsBreak){
    x1 = &laserInsBreak;
    x2 = &laserOutBreak;
    laserAssign = -1; // Inside's laser bream broken first
    state_1();
  } else if(laserOutBreak){ 
    x1 = &laserOutBreak;
    x2 = &laserInsBreak;
    laserAssign = 1; // Outside's laser bream broken first
    state_1();
  }
  displayCount();
  }

void state_1(){ 
  Serial.println("STATE 1");
  Serial.println(*x1);
  Serial.println(*x2);
  while(*x1 && !*x2){
    checkLasers();
  }
  if(*x1 && *x2){
    state_2();
    return;
  } else if(!*x1 && !*x2){
    if(deltaPerson == 2){ // One person stood in the entrance and one person has passed
      peopleCount -= laserAssign;
      resetParameters();
    } else{ // One person just having a look or two people crossing, no change in peopleCount
      resetParameters();
    }
    return;
  }
  Serial.println("ERROR : State 1 got out bound");
}

void state_2(){
  Serial.println("STATE 2");
  Serial.println(*x1);
  Serial.println(*x2);
  
  while(*x1 && *x2){
    checkLasers();
    checkDistance();
  }
  if(!*x1 && *x2){
    state_3();
    return;
  } else if(*x1 && !*x2) {
    // One person just having a look or two people crossing, no change in peopleCount
    state_1(); // Better check it
    return;
  }
  Serial.println("ERROR : State 2 got out bound");
  Serial.println(*x1);
  Serial.println(*x2);
}

void state_3(){
  Serial.println("STATE 3");
  Serial.println(*x1);
  Serial.println(*x2);
  while(!*x1 && *x2){
    checkLasers();
  }
  if(*x1 && *x2){
    deltaPerson = 2;
    state_4();
    return;
  } else if (!*x1 && !*x2) {
    // One person or two people passed
    peopleCount +=  laserAssign * deltaPerson;
    resetParameters();
    return;
  }
  Serial.println("ERROR : State 3 got out bound");
}

void state_4(){
  Serial.println("STATE 4");
  Serial.println(*x1);
  Serial.println(*x2);
  while(*x1 && *x2){
    checkLasers();
  }
  if(!*x1 && *x2){
    state_5();
    return;
  } else if (*x1 && !*x2){
    state_6();
    return;
  }
  Serial.println("ERROR : State 4 got out bound" );
}

void state_5(){
  Serial.println("STATE 5");
  Serial.println(*x1);
  Serial.println(*x2);
  while(!*x1 && *x2){
    checkLasers();
  }
  if(!*x1 && !*x2){
    // Two or more people passed with a small time difference
    peopleCount +=  laserAssign * deltaPerson;
    resetParameters();
    return;
  } else if (*x1 && *x2){
    deltaPerson++;
    state_4();
    return;
  }
  Serial.println("ERROR : State 5 got out bound");
}

void state_6(){
  Serial.println("STATE 6");
  Serial.println(*x1);
  Serial.println(*x2);
  while(*x1 && !*x2){
    checkLasers();
  }
  if(!*x1 && !*x2){
    // Two people crossing
    resetParameters();
    return;
  } else if (*x1 && *x2){
    state_4(); // Not to stuck in an infinite loop
    return;
  }
  Serial.println("ERROR : State 6 got out bound");
}

void checkLasers(){ 
  // x1 has the priority to catch the time difference between laser beam boolean changes
  if (laserAssign == 1){ // x1 is set to outside laser beam, so outside has the priority
    if(analogRead(photoRes_Out)*(0.9) < initialOutside*0.8){
      if(laserOutBreak == false){
          laserOutBreak = true;
          return;
        }
    } else if(laserOutBreak == true){
        laserOutBreak = false;
        return;
    }
    // if not change in outside beam, check the outside
    if(analogRead(photoRes_Ins)*(0.9) < initialInside*0.8){
      if(laserInsBreak == false){
        laserInsBreak = true;
        return;
      }
    } else if(laserInsBreak == true){
        laserInsBreak = false;
        return;
      }
  } else { // x1 is set to inside laser beam, so inside has the priority
    if(analogRead(photoRes_Ins)*(0.9) < initialInside*0.8){
      if(laserInsBreak == false){
          laserInsBreak = true;
          return;
        }
    } else if(laserInsBreak == true){
        laserInsBreak = false;
        return;
    }
    // if not change in inside beam, check the outside
    if(analogRead(photoRes_Out)*(0.9) < initialOutside*0.8){
      if(laserOutBreak == false){
        laserOutBreak = true;
        return;
      }
    } else if(laserOutBreak == true){
        laserOutBreak = false;
        return;
      }
  }
}

void checkDistance(){
  if(deltaPerson == 2){ // For efficiency, it cannot be larger than that just return
      return;
  }
  int dist_1 = ultrasonDist(trigPin_1, echoPin_1);
  while(dist_1 > initDist_1){ // if the sensor measurement is invalid, re-measure
    dist_1 = ultrasonDist(trigPin_1, echoPin_1);
  }
  
  Serial.println("---------");
  Serial.println(dist_1);
  Serial.println("---------");
  
  int dist_2 = ultrasonDist(trigPin_2, echoPin_2);
  while(dist_2 > initDist_2){  // if the sensor measurement is invalid, re-measure
    dist_2 = ultrasonDist(trigPin_2, echoPin_2);
  }
  Serial.println(dist_2);
  int deltaDist = doorWidth - (dist_1 + dist_2); 
  if(deltaDist < 0) { // boundary check, cannot be less than zero
    deltaDist = 0; 
  }
  Serial.println("---------");
  Serial.print("deltaDist: ");
  Serial.println(deltaDist);
  if(deltaDist > peopleThreshold){ // the width missing is wider than one person
    deltaPerson = 2;
  } else {
    deltaPerson = 1;
  }
}

void resetParameters(){
  laserInsBreak = false;
  laserOutBreak = false;
  deltaPerson = 0;
  laserAssign = 0;
}

void displayCount(){
  Serial.print("People COUNT: ");
  Serial.println(peopleCount);
  if(!delayActive && peopleCount != 0){
    delayActive = true;
    if(peopleCount > 0){
      digitalWrite(lightEnable, LOW);
      lightLed.on();
    }
  }
  if(delayActive && peopleCount == 0){
    if(delayPostpone == 0){
      delayPostpone = millis() / 1000 + delayAmount;
    } else {
      if(millis() / 1000 >= delayPostpone){
        digitalWrite(lightEnable, HIGH);
        delayPostpone = 0;
        delayActive = false;
        lightLed.off();
      }
    }
  }
  if(emergencyPin == 1){
    alarmLed.on();
    emergencyPin++;
    if(peopleCount <= 0){
      emergencyPin = 0;
      
    }
  }
  if(emergencyPin == 2){
    if(peopleCount <= 0){
      emergencyPin = 0;
      alarmLed.off();
      clearLed.on();
    }
  }
}

void calibrateValues(){
  clearLed.off();
  if(resetCount == 0){
    digitalWrite(lightEnable, HIGH);
    lightLed.off();
  } else{
    digitalWrite(lightEnable, LOW);
    lightLed.on();
  }
  Serial.println("CALIBRATING");
  Blynk.virtualWrite(V2,"Calibrating");
  Vector<int> tempData;
  for(int i=0;i<5;i++){
    int temp = ultrasonDist(trigPin_1, echoPin_1);
  Serial.println(temp);
    if(temp == 0){
      i--;
    }else{
      tempData.push_back(temp);
    }
    tempData.push_back(temp);
  }
  initDist_1 = mostRepeatedElement(tempData);
  
  Serial.println("------------");
  
  Vector<int> tempData_2;
  for(int i=0;i<5;i++){
    int temp = ultrasonDist(trigPin_2, echoPin_2);
  Serial.println(temp);
    if(temp == 0){
      i--;
    }else{
      tempData_2.push_back(temp);
    }
  }
  initDist_2 = mostRepeatedElement(tempData_2);
  doorWidth = (initDist_1 + initDist_2) / 2 ;
  
  //Calibrating Lasers and Photoresistors
  Vector<int> tempData_3;
  for(int i=0;i<5;i++){
    int temp = analogRead(photoRes_Ins)*(0.95);
    if(temp == 0){
      i--;
    }else{
      tempData_3.push_back(temp);
    }
    tempData_3.push_back(temp);
  }
  initialInside = mostRepeatedElement(tempData_3);
  
  Vector<int> tempData_4;
  for(int i=0;i<5;i++){
    int temp = analogRead(photoRes_Out)*(0.95);
    if(temp == 0){
      i--;
    }else{
      tempData_4.push_back(temp);
    }
  }
  initialOutside = mostRepeatedElement(tempData_4); 
  Serial.println("BEGIN");
  Serial.print("Door Width:");
  Serial.println(doorWidth);
  Blynk.virtualWrite(V5, doorWidth);
  delay(2000);
  Blynk.virtualWrite(V2,"SHERLOCK COUNTER:");
  resetLed.off();
}

int ultrasonDist(int trigPin, int echoPin){
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10); 
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH);
    int distance = duration/58.2;
    delay(10);
    return distance;
}

int mostRepeatedElement(Vector<int> &myVec){
  // takes a vetor of integers and returns the most repoeated integer in its tolerance range 
  int bestIndex = 0;
  int maxRep = 0;
  for(int i=0; i<myVec.size()-maxRep; i++){
    int tempRep = 1;
    for(int j=i+1; j<myVec.size();j++){
       if(myVec[i]*(1.10) >= myVec[j] && myVec[i]*(0.9) <= myVec[j]){
        tempRep++;
       }
    }
    if(tempRep > maxRep){
      maxRep = tempRep;
      bestIndex = i;
    }
    if(maxRep >= myVec.size()/2){
      return myVec[bestIndex];
    }
  }
  return myVec[bestIndex];
}

BLYNK_READ(V0){ //Count Display Widget is connected to pin V0
  Blynk.virtualWrite(V0, peopleCount);
  Blynk.virtualWrite(V6, "update", 7, "Exhibition Room", peopleCount);
}

BLYNK_WRITE(V1) //Reset Button Widget is writing to pin V1
{
  if(resetPin == 0){
    resetLed.on();
    resetPin = param.asInt(); 
  }
}

BLYNK_WRITE(V3) //People Threshold Slider Widget is writing to pin V3
{
  peopleThreshold = param.asInt(); 
}

BLYNK_WRITE(V4) //Reset Count InputWidget is connected to V4
{
  resetCount = param.asInt(); 
}

BLYNK_WRITE(V11) //Delay Amount Slider Widget is writing to pin V11
{
  delayAmount = param.asInt(); 
}

BLYNK_WRITE(V8) //Emergency Button Widget is writing to pin V8
{
  if(emergencyPin == 0){
    emergencyPin = param.asInt(); 
  }
}
