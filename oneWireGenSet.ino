/*
   Auto Gen Start for ONAN Quiet Diesel
   Provides interface for single wire GenSet control from Cerbo GX
   NOTE: Active-Low inputs

       by: Michael J. Kidd <linuxkidd@gmail.com>
  Version: 1.0
     Date: 2023-06-21
  https://github.com/linuxkidd/oneWireGenSet

*/

/*
 * - Start process:
 *     1. Stop for 10 seconds
 *     2. Pause 4 seconds
 *     3. Start for 20 seconds or until Running indication
 *     4. Delay 2 minutes on failure, then retry up to 3 times
 *     5. Monitor for 20 seconds and attempt to restart if dies
 * - Stop process:
 *     1. Stop for 10 seconds
 */

/*
 * Uncomment the DEBUG define to get copious output on the serial line
 * to help troubleshoot logic issues.
 */

// #define DEBUG

const unsigned int GEN_STOP_PIN        = 9;
const unsigned int GEN_START_PIN       = 8;
const unsigned int GEN_RUN_SENSE_PIN   = 7;
const unsigned int GEN_RUN_DEMAND_PIN  = 6;

const unsigned int START_MAX_TRIES        = 3;
const unsigned int RUN_DEBOUNCE_MILLIS    = 1000;
const unsigned int DEMAND_DEBOUNCE_MILLIS = 750;
// RUN_DEBOUNCE_MILLIS    == how long the 'run' input must be active before considering the GenSet as running.
// DEMAND_DEBOUNCE_MILLIS == how long the 'demand' input must be active before triggering the GenSet start sequence.

/* ############################################
 *   Variables below this point are used 
 *   internally and should not be changed.
 * ############################################
 */

const unsigned int RELAY_ON  = 0;
const unsigned int RELAY_OFF = 1;
const boolean  INPUT_ACTIVE  = LOW;

//                               START STEP:  0,     1,    2,     3,      4,     5
const unsigned long START_STEP_MILLIS[7]  = { 0, 10000, 4000, 20000, 120000, 20000 };
const unsigned int START_STEPS[7][3] = {
  // STOP_RELAY, START_RELAY
  { RELAY_OFF, RELAY_OFF }, // Step 0
  { RELAY_ON , RELAY_OFF }, // Step 1
  { RELAY_OFF, RELAY_OFF }, // Step 2
  { RELAY_OFF, RELAY_ON  }, // Step 3
  { RELAY_OFF, RELAY_OFF }, // Step 4
  { RELAY_OFF, RELAY_OFF }  // Step 5
};
const unsigned long STOP_MILLIS = 10000;

#ifdef DEBUG
const char onoff[2][4] = { " On", "Off" };
const char    yn[2][4] = { " No", "Yes" };
#endif // DEBUG

const unsigned long REPORT_INTERVAL = 1000;
unsigned long LAST_REPORT = 0;

void readPins();
void sendReport();
void startGen();
void stopGen();
void checkGenState();

typedef struct {
  boolean runDemand        = false;
  unsigned long runDemand_millis = 0;

  boolean isRunning        = false;
  unsigned long isRunning_millis = 0;
  
  boolean tryingToStart    = false;
  boolean startFailed      = false;
  unsigned int startStep   = 0;
  unsigned int startTries  = 0;
  unsigned long startStep_millis = 0;
 
  boolean tryingToStop      = false;
  unsigned long stopStep_millis  = 0;
} genState;

genState gen;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("GenSet Interface v1.0");
  Serial.println("https://github.com/linuxkidd/oneWireGenSet");
  Serial.println();
  Serial.print(millis());
  Serial.println(": Starting PIN Config");

  pinMode(GEN_STOP_PIN, OUTPUT);
  digitalWrite(GEN_STOP_PIN , RELAY_OFF);

  pinMode(GEN_START_PIN, OUTPUT);
  digitalWrite(GEN_START_PIN, RELAY_OFF);

  pinMode(GEN_RUN_SENSE_PIN , INPUT_PULLUP);
  pinMode(GEN_RUN_DEMAND_PIN, INPUT_PULLUP);

  Serial.print(millis());
  Serial.println(": PIN Config complete");
}

void loop() {
  readPins();
  checkGenState();
  delay(50);
  if(millis() - LAST_REPORT > REPORT_INTERVAL)
    sendReport();
}

void sendReport() {
#ifdef DEBUG
  Serial.print(millis());          Serial.print(": ");
  Serial.print("runDemand: ");     Serial.print(yn[gen.runDemand]);
  Serial.print(", isRunning: ");   Serial.print(yn[gen.isRunning]);
  Serial.print(", startFailed: "); Serial.print(yn[gen.startFailed]);
  Serial.print(", startTries: ");  Serial.print(gen.startTries);
  Serial.print(", startRelayOn: ");  Serial.print(onoff[digitalRead(GEN_START_PIN)]);
  Serial.print(", stopRelayOn: ");   Serial.print(onoff[digitalRead( GEN_STOP_PIN)]);
  if(gen.tryingToStart)
  {
    Serial.print(", tryingToStart: "); Serial.print(yn[gen.tryingToStart]);
    Serial.print(", startStep: "); Serial.print(gen.startStep);
    Serial.print(", startStep_millis: "); Serial.print(gen.startStep_millis);
    Serial.print(" ( "); Serial.print(START_STEP_MILLIS[gen.startStep] - (millis()-gen.startStep_millis)); Serial.print(" ) ");
  }

  if(gen.tryingToStop)
  {
    Serial.print(", tryingToStop: "); Serial.print(yn[gen.tryingToStop]);
    Serial.print(", stopStep_millis: "); Serial.print(gen.stopStep_millis);
    Serial.print(" ( "); Serial.print(STOP_MILLIS - ( millis()-gen.stopStep_millis )); Serial.print(" )");
  }
  Serial.println();
#endif // DEBUG
  LAST_REPORT=millis();
}

void readPins() {
  if(digitalRead(GEN_RUN_SENSE_PIN) == INPUT_ACTIVE)
  {
    if(gen.isRunning_millis != 0) {
      if(!gen.isRunning && ( millis() - gen.isRunning_millis > RUN_DEBOUNCE_MILLIS )) 
      {
        gen.isRunning = true;
        gen.isRunning_millis = 0;
        sendReport();
      }
    }
    else
      gen.isRunning_millis = millis();
  }
  else
  {
    if(gen.isRunning)
    {
      gen.isRunning = false;
      gen.isRunning_millis = 0;
      sendReport();
    }
  }

  if(digitalRead(GEN_RUN_DEMAND_PIN) == INPUT_ACTIVE) 
  {
    if(gen.runDemand_millis != 0) 
    {
      if(!gen.runDemand && ( millis() - gen.runDemand_millis > DEMAND_DEBOUNCE_MILLIS))
      {
        gen.runDemand=true;
        startGen();
        sendReport();
      }
    }
    else
      gen.runDemand_millis = millis();
  }
  else
  {
    if(gen.runDemand)
    {
      gen.runDemand=false;
      stopGen();
      sendReport();
    }
  }
}

void startGen() {
  if(!gen.isRunning)
  {
    gen.tryingToStart    = true;
    gen.startStep        = 1;
    gen.startTries++;
    gen.startStep_millis = millis();
    digitalWrite(GEN_START_PIN, START_STEPS[gen.startStep][1]);
    digitalWrite(GEN_STOP_PIN,  START_STEPS[gen.startStep][0]);
  } // !gen.isRunning
  else
  { // gen.isRunning
    digitalWrite(GEN_START_PIN, RELAY_OFF);
    digitalWrite(GEN_STOP_PIN,  RELAY_OFF);
    gen.startFailed      = false;
    gen.startStep        = 0;
    gen.startStep_millis = 0;
    gen.tryingToStart    = false;
  }
}

void stopGen() {
  if(gen.isRunning) {
    digitalWrite(GEN_START_PIN, RELAY_OFF);
    digitalWrite(GEN_STOP_PIN , RELAY_ON );
    gen.tryingToStop     = true;
    gen.stopStep_millis  = millis();
  } // gen.isRunning
  else
  {
    digitalWrite(GEN_START_PIN, RELAY_OFF);
    digitalWrite(GEN_STOP_PIN , RELAY_OFF);
  }
  gen.startFailed      = false;
  gen.startTries       = 0;
  gen.startStep        = 0;
  gen.startStep_millis = 0;
  gen.tryingToStart    = false;
}

/*
 * - Start process:
 *     1. Stop for 10 seconds
 *     2. Pause 4 seconds
 *     3. Start for 20 seconds or until Running indication
 *     4. Delay 2 minutes on failure, then retry up to 3 times total
 * - Stop process:
 *     1. Stop for 10 seconds
 */

void checkGenState() {
  if(gen.runDemand)
  {
    if(!gen.isRunning && !gen.startFailed)
    {
      if(gen.tryingToStart)
      {
        if(gen.startStep<=3)
        {
          if(millis() - gen.startStep_millis > START_STEP_MILLIS[gen.startStep])
          {
            gen.startStep++;
            gen.startStep_millis=millis();
            digitalWrite(GEN_STOP_PIN,START_STEPS[gen.startStep][0]);
            digitalWrite(GEN_START_PIN,START_STEPS[gen.startStep][1]);
            if(gen.startStep==4)
            {
              if(gen.startTries >= 3)
              {
                gen.startFailed      = true;
                gen.startStep        = 0;
                gen.startStep_millis = 0;
                gen.tryingToStart    = false;
              } // gen.startTries >= 3
            } // gen.startStep == 4
            sendReport();
          } // millis() - gen.startStep_millis > START_STEP_MILLIS[gen.startStep]
        } // gen.startStep<=3
        else if(gen.startStep==4)
        {
          // 4. Delay 2 minutes on failure, then retry up to 3 times total
          if(millis() - gen.startStep_millis > START_STEP_MILLIS[gen.startStep]) {
            startGen();
          }
        } // gen.startStep==4
        else if(gen.startStep==5)
        { // Only here if GenSet started, then died inside 10 seconds
          gen.startStep = 4;
          gen.startStep_millis = millis();
        }
      } // gen.tryingToStart
    } // !gen.isRunning && !gen.startFailed
    else if(gen.isRunning)
    {
      if(gen.tryingToStart)
      {
        if( digitalRead(GEN_START_PIN) == RELAY_ON )
        {
          digitalWrite(GEN_STOP_PIN , RELAY_OFF);
          digitalWrite(GEN_START_PIN, RELAY_OFF);
          gen.startFailed      = false;
          gen.startStep        = 5;
          gen.startStep_millis = millis();
        } // digitalRead(GEN_START_PIN) == RELAY_ON
        else if(gen.startStep == 5)
        { 
          if( millis() - gen.startStep_millis > START_STEP_MILLIS[gen.startStep] )
          { // If GenSet stayed running more than 10 seconds, clear final checks
            gen.startStep_millis = 0;
            gen.startStep        = 0;
            gen.tryingToStart    = false;
          }
        } // gen.startStep == 5
        else
        { // GenSet started without providing Start signal? Maybe manual start?
          digitalWrite(GEN_STOP_PIN , RELAY_OFF);
          digitalWrite(GEN_START_PIN, RELAY_OFF);
          gen.startFailed      = false;
          gen.startStep        = 5;
          gen.startStep_millis = millis();
        }
      } // gen.tryingToStart
    } // gen.isRunning
  } // gen.runDemand
  else if(gen.tryingToStop)
  { // gen.tryingToStop
    if(millis() - gen.stopStep_millis > STOP_MILLIS || digitalRead(GEN_STOP_PIN) != RELAY_ON) {
      digitalWrite(GEN_STOP_PIN , RELAY_OFF);
      digitalWrite(GEN_START_PIN, RELAY_OFF);
      gen.tryingToStop    = false;
      gen.stopStep_millis = 0;
    }
  }
}
/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
