 /* 
 *  FILE    :   looper_pedal_midi_v1.5.1.ino
 *  AUTHOR  :   Matt Joseph
 *  DATE    :   9/27/2022
 *  VERSION :   1.5.1
 *  
 *
 *  DESCRIPTION
 *  https://www.instructables.com/id/DIY-Chewie-Monsta-Looper-Based-on-Ed-Sheerans/
 *  
 *  
 *  
 *  REV HISTORY
 *  1.0)  Initial release per description
 *  1.1)  Added volume control functionality
 *  1.2)  Added bit shift latching function for button reads
 *        Updated MIDI command values for each output
 *  1.3)  release date 8/20/2018 
 *		    Added "UNDO" function to "CLEAR"  button   
 *        Short press (<1000ms) activates "UNDO"
 *        Long press (>=1000ms) activates "CLEAR"
 *  1.3a) release date 8/23/2019 (Claudio Cas)
 * 		    Added code for debounce button press without using delay() in mail loop: improve responsiveness of button (more precise loop start and close)
 *		    default value is 60ms, you can tweak changing the minRetriggerTime variable
 *  1.4)  release date 8/26/2019
 *        Minor updates to release 1.3a changes
 *  1.5)  release date TBD
 *        Moved all loop playback logic from Mobius scripts to Arduino code
 *        added logic to "ARM" tracks for unmuting
 *  1.5.1)Added logic to turn on LEDs for UNDO and CLEAR
 */  

#define NUM_BUTTONS  7
#define NUM_TRACKS 3
#define OFF 0
#define GREEN 1
#define RED 2
#define AMBER 3

//PIN ASSIGNMENTS
const uint8_t btn_rec_play = 3;  //REC-PLAY
const uint8_t btn_reset = 4;     //RESET
const uint8_t btn_mult_stop = 5; //MULT-STOP
const uint8_t btn_clear = 6;     //CLEAR
const uint8_t btn_track1 = 7;    //TRACK 1
const uint8_t btn_track2 = 8;    //TRACK 2
const uint8_t btn_track3 = 9;    //TRACK 3
const uint8_t btn_mode = 10;     //MODE
const uint8_t recLED = 11;
const uint8_t volLED = 12;

const uint8_t t1_green_led = 48;
const uint8_t t1_red_led = 49;
const uint8_t t2_green_led = 50;
const uint8_t t2_red_led = 51;
const uint8_t t3_green_led = 52;
const uint8_t t3_red_led = 53;
const uint8_t undo_led = 42;
const uint8_t clear_led = 43;

//MIDI values for each function
const uint8_t reset_cmd = 10;
const uint8_t undo_cmd = 11;
const uint8_t clear_cmd = 12;

const uint8_t play_play_cmd = 20;
const uint8_t play_stop_cmd = 21;
const uint8_t play_track_cmd[3] = {22, 23, 24};
const uint8_t restart_track_cmd[3] = {25, 26, 27};

const uint8_t rec_rec_cmd = 30;
const uint8_t rec_mult_cmd = 31;
const uint8_t rec_track_cmd[3] = {32, 33, 34};

const uint8_t vol_track_cmd[3] = {42, 43, 44};
const uint8_t mute_track_cmd[3] = {52, 53, 54};
const uint8_t pause_track_cmd[3] = {72, 73, 74};

const uint8_t buttons[NUM_BUTTONS] = {btn_reset, btn_clear, btn_rec_play, btn_mult_stop, btn_track1, btn_track2, btn_track3};

int mode = 0;                   // 0=Play, 1=Record, 2=Volume control
int modeValue = 1;
int previousMode = 0;
int previousModeValue = 1;
unsigned long modeLongPress = 0;
unsigned long clearLongPress = 0;
int longPressTime = 1000;
int modeChange = 0;
int modeStateChange = 0;
uint8_t modeBuffer = 0xFF;
uint8_t modeEval = 0xFF;
uint8_t priorModeEval = 0xFF;
uint8_t btnBuffer[NUM_BUTTONS] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t btnEval[NUM_BUTTONS] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t priorBtnEval[NUM_BUTTONS] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

int track_play_state[3] = {0,0,0};    //0=play, 1=mute, 2=arm, 3=record
int track_rec_state[3] = {0,0,0};     //0=rec, 1=overdub, 2=play
int rec_mode = 0;                     //0=reset, 1=record
int play_mode = 0;                    //0=stopped, 1=playing
int active_track_index = 0;
int active_track[3] = {1,2,3};


// Retrigger and debouce
const int minRetriggerTime = 100;  //Min millis between 2 button press
unsigned long btnLastPressTime[NUM_BUTTONS] = {0,0,0,0,0,0,0};  
unsigned long modeLastPressTime = 0;

// variables for UNDO and CLEAR LEDs
unsigned long undoPressTime = 0;
unsigned long clearPresTime = 0;
const int led_on_time = 2000;

void setup() {
 
  // Set MIDI baud rate:
  //Serial.begin(31250);
  // Set baud rate to 38400 for Hairless MIDI
  Serial.begin(38400);   
  
  // set the LED pins as output:
  pinMode(recLED, OUTPUT);
  pinMode(volLED, OUTPUT);
  
  //set pins for UNDO and CLEAR LEDs
  pinMode(undo_led, OUTPUT);
  pinMode(clear_led, OUTPUT);

  // set up the TRACK LEDs
  pinMode(t1_green_led, OUTPUT);
  pinMode(t1_red_led, OUTPUT);
  pinMode(t2_green_led, OUTPUT);
  pinMode(t2_red_led, OUTPUT);
  pinMode(t3_green_led, OUTPUT);
  pinMode(t3_red_led, OUTPUT);

  //Track LEDs are "active-low" so init to HIGH so they are OFF
  digitalWrite(t1_green_led, HIGH);
  digitalWrite(t1_red_led, HIGH);
  digitalWrite(t2_green_led, HIGH);
  digitalWrite(t2_red_led, HIGH);
  digitalWrite(t3_green_led, HIGH);
  digitalWrite(t3_red_led, HIGH);

  // set up buttons, they are "active-low" so set to "PULLUP"
  for (int i = 0; i < NUM_BUTTONS; i++){
      pinMode(buttons[i], INPUT_PULLUP);
  }
  pinMode(btn_mode, INPUT_PULLUP);
}


void loop() {
  readButtons();
}

/*
* Check if the current press is a valid and not a bounce of signal
* Ignore button press before minRetriggerTime ms
*/
bool retriggerTimeExpired(unsigned long lastPressTime)
{
    if(millis() - lastPressTime >= minRetriggerTime)
    {
        return true;
    }else{
        return false;
    }
}

void readButtons()
{
  int i = 0;
  int j = 0;
  int k = 0;

  //check state of MODE button
  modeValue = digitalRead(btn_mode);      //read current input
  modeBuffer = modeBuffer << 1;           //shift buffer 1 bit left
  bitWrite(modeBuffer, 0, modeValue);     //add new sample to buffer

  
  priorModeEval = modeEval;               //update last evaluated value of MODE
  modeEval = modeBuffer & 0x07;           //keep last 3 samples for evaluation (0b00000111)

  
  if ( modeEval == 0x00 && retriggerTimeExpired(modeLastPressTime))   //Evaluated input has at least 3 sequential 0s
  {   
      if (modeEval != priorModeEval)        //if evaluated MODE has changed from 1 to 0 for 3 consecutive samples (button pressed)
      {
          modeChange = 1;
          modeLastPressTime = millis();
          
          //Toggle MODE betwee PLAY and REC
          if (mode == 0)
          {
              mode = 1;
          }else{
              mode = 0;
          }
      }else if(millis() - modeLongPress >= longPressTime)   //if evaluated MODE has stayed 0 for at least "longPressTime" (1000ms)
      {
          //Set MODE to VOL_CTL
          if(mode != 2)
          {
              mode = 2;
              modeChange = 1;
          }
         
      }
  }else if(modeEval == 0x07){           //If evaulated MODE is 1 for 3 consecutive samples (button not pressed)
    //logic to reset the timer for vol mode
    modeLongPress = millis();           //reset the timer for when MODE was pressed to current time
    modeChange = 0;
  }
  
  
      
  
  //light record LED if in Record mode
  if (mode == 1)
  {
    digitalWrite(recLED, HIGH);
  }else{
    digitalWrite(recLED, LOW);
  }

  //light volume control LED if in Volume Control mode
  if (mode == 2)
  {
    digitalWrite(volLED, HIGH);
  }else{
    digitalWrite(volLED, LOW);
  }

  //Read remaining buttons
  memcpy(priorBtnEval, btnEval, NUM_BUTTONS*sizeof(uint8_t));     //save copy of last button evaluated values

    
  //Read the rest of the buttons
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
      btnBuffer[i] = btnBuffer[i] << 1;                      //shift buffer 1 bit left
      bitWrite(btnBuffer[i], 0, digitalRead(buttons[i]));     //add new sample to buffer
      btnEval[i] = btnBuffer[i] & 0x07;                      //keep last 3 samples for evaluation    
  }

  //send midi for mode button on state change
  if (modeChange == 1)
  {
      //Reset modeChange flag
      modeChange = 0;
      //If in play or vol ctrl mode send midi cmd to stop any active recording
      if(mode==0){
          for(k=0;k<NUM_TRACKS;k++){
              track_rec_state[k] = 2;
              
              if(track_play_state[k] != 1){
                  track_play_state[k] = 0;
                  sendCmd(1, play_track_cmd[k], 127);
                  if(rec_mode == 1){
                      setTrackLED(k, GREEN);
                  }
              }
          }         
      }else if(mode==2){
          for(k=0;k<NUM_TRACKS;k++){
              track_rec_state[k] = 2;
              
              if(track_play_state[k] != 1){
                  track_play_state[k] = 0;
                  sendCmd(1, play_track_cmd[k], 127);
                  if(rec_mode == 1){
                      setTrackLED(k, GREEN);
                  }
              }
          }
      }else{
          //sendCmd(1, rec_rec_cmd, 127);
      }
  }


  //Check button status and send correct MIDI Command
  //RESET and CLEAR buttons do not depend on MODE. 
  
  //Logic for RESET button
  if (btnEval[0] == 0 && retriggerTimeExpired(btnLastPressTime[0])){
      if(btnEval[0] != priorBtnEval[0]) {
          sendCmd(1, reset_cmd, 127);

          for(k=0;k<NUM_TRACKS;k++){
              //clear the track states
              track_play_state[k] = 0;
              track_rec_state[k] = 0;
              //Set track LEDs to OFF
              setTrackLED(k, OFF);
          }
          rec_mode = 0;
          play_mode = 0;         
      }
      btnLastPressTime[0] = millis();
  }

  //Logic for CLEAR button
  if (btnEval[1] == 0 && retriggerTimeExpired(btnLastPressTime[1])){
      //handle short press
      if(btnEval[1] != priorBtnEval[1]){                      
            sendCmd(1, undo_cmd, 127);
            btnLastPressTime[1] = millis();

            //Turn on UNDO LED and store time;
            digitalWrite(undo_led, HIGH); 
            undoPresstime = millis();           
      //handle long press
      }else if((millis() - clearLongPress) >= longPressTime){
            sendCmd(1, clear_cmd, 127);
            clearLongPress = millis();

            //Turn on CLEAR LED, turn off UNDO LED and store time;
            digitalWrite(undo_led, LOW); 
            digitalWrite(clear_led, HIGH); 
            clearPresstime = millis(); 
      }
  }else{
      //reset last time clear was pressed
      clearLongPress = millis();

      //check for led_on_time to expire and turn off UNDO LED
      if(millis() - undoPressTime >= led_on_time){
        digitalWrite(undo_led, LOW);
        undoPressTime = 0;        
      }      

      //check for led_on_time to expire and turn off CLEAR LED
      if(millis() - clearPressTime >= led_on_time){
        digitalWrite(clear_led, LOW);
        clearPressTime = 0;        
      }        
  }

 
  //Check for button presss and send correct note in Play mode for the rest of the buttons

  
  //Use index 2 for PLAY button
  i=2;
  if (btnEval[i] == 0 && retriggerTimeExpired(btnLastPressTime[i])){
      if(btnEval[i] != priorBtnEval[i]){
           if(mode==0){
              //bring tracks in and restart the loop from STOP mode
              if(play_mode==0){
                  play_mode=1;
                  for(j=0; j<NUM_TRACKS; j++){
                      //Play all tracks in 'ARM' or 'REC' state
                      if(track_play_state[j]==2 || track_play_state[j]==3){
                          //Unmute and restart
                          sendCmd(1, mute_track_cmd[j], 127);
                          sendCmd(1, restart_track_cmd[j], 127);
                          track_play_state[j] = 0;
                          //Set track LED to GREEN
                          setTrackLED(j,GREEN);
                      }else if(track_play_state[j]==1){
                          //Restart the track
                          sendCmd(1, restart_track_cmd[j], 127);
                      }
                  }
              }else{    //bring tracks in without restarting the loop
                  for(j=0; j<NUM_TRACKS; j++){
                      //Play all tracks in 'ARM' or 'REC' state
                      if(track_play_state[j]==2 || track_play_state[j]==3){
                          sendCmd(1, mute_track_cmd[j], 127);
                          track_play_state[j] = 0;
                          //Set track LED to GREEN
                          setTrackLED(j,GREEN);
                      }
                  }
              }
              
           }else if(mode==1){
              
              //Set tracks previously in REC state to PLAY and change LED to GREEN
              for(j=0; j<NUM_TRACKS; j++){
                  if(track_play_state[j]==3){
                      //Play all tracks in 'REC' state
                      track_play_state[j] = 0;
                      //Set track LED to GREEN
                      setTrackLED(j,GREEN);
                  }
              }

              track_play_state[active_track_index]=3;  //Set track mode to REC so that we can change the LED later
          
              if(rec_mode==0){
                  rec_mode = 1;
                  play_mode=1;

                  // REC on all tracks on first loop (from RESET mode)
                  //The Mobius script handles all tracks so we only need to send one command
                  sendCmd(1, rec_rec_cmd, 127);

                  for(k=0;k<NUM_TRACKS;k++){
                       track_rec_state[k] = 0;
                      //Set track LEDs to RED
                      setTrackLED(k, RED);
                  }
              }else if(rec_mode==1){
                  //Set active track to overdub when prior state was Record, set other tracks to play
                  if(track_rec_state[active_track_index]==0){
                      //The Mobius script handles all tracks so we only need to send one command
                      sendCmd(1, rec_rec_cmd, 127);
                      
                      for(k=0;k<NUM_TRACKS;k++){
                          if(k == active_track_index){
                              track_rec_state[k] = 1;
                              //SET track LEDs 
                              setTrackLED(k,RED);
                          }else{
                              track_rec_state[k] = 2;
                              //SET track LEDs 
                              setTrackLED(k,GREEN);
                          }
                      }
                  }else if(track_rec_state[active_track_index]==1){
                      //Set active track to play when prior state was overdub
                      sendCmd(1, rec_rec_cmd, 127);
                      track_rec_state[active_track_index] = 2;
                      //Set LED on active track to GREEN
                      setTrackLED(active_track_index,GREEN);
                  }else if(track_rec_state[active_track_index]==2){
                      //Set active track to overdub when prior state was play
                      sendCmd(1, rec_rec_cmd, 127);
                      track_rec_state[active_track_index] = 1;
                      //Set LED on active track to RED
                      setTrackLED(active_track_index,RED);
                  }
              }
           }
      }
      btnLastPressTime[i] = millis();
  }

  //Use index 3 for STOP button
  i=3;
  if (btnEval[i] == 0 && retriggerTimeExpired(btnLastPressTime[i])){
      if(btnEval[i] != priorBtnEval[i]){
           if(mode==0){
              for(j=0; j<NUM_TRACKS; j++){
                    if(track_play_state[j]==0){
                        //Change all tracks in 'PLAY' state to 'ARM' so that they will be started with PLAY is pressed
                        track_play_state[j] = 2;
                        //Set track LED to AMBER
                        setTrackLED(j,AMBER);
                        sendCmd(1, mute_track_cmd[j], 127);
                    }else if(track_play_state[j]==1){
                        //have to unmute the track before you can pause it
                        //sendCmd(1, mute_track_cmd[j], 127);
                        //sendCmd(1, pause_track_cmd[j], 127);
                    }
                    
                }
                play_mode=0;    //set play_mode to 'stopped' so that tracks will restart when PLAY is pressed;
                
           }else if(mode==1){
              sendCmd(1, rec_mult_cmd, 127);
           }
      }
      btnLastPressTime[i] = millis();
  }

  //Use index 4 for TRACK 1 button
  i=4;
  j=0;
  handleTrackPress(i, j);
  
  
  //Use index 5 for TRACK 2 button
  i=5;
  j=1;
  handleTrackPress(i, j);
  

  //Use index 6 for TRACK 3 button
  i=6;
  j=2;
  handleTrackPress(i, j);
 
  delay(10); 
}

// Sends a MIDI control command. Doesn't check to see that cmd is greater than 127, or that data values are less than 127:
// first parameter is the event type, combined with the channel.
// Second parameter is the control number number (0-119).
// Third parameter is the control value (0-127).

void sendCmd(int channel, int command, int value) {
  Serial.write(0xB0 | (channel-1));
  Serial.write(command);
  Serial.write(value);
}


void handleTrackPress(int btn_index, int track_index){
     int i;
     int j;
     int k;

     i=btn_index;
     j=track_index;
     
     if (btnEval[i] == 0 && retriggerTimeExpired(btnLastPressTime[i])){
        if(btnEval[i] != priorBtnEval[i]){
             active_track_index=j;
             if(mode==0){
                if(track_play_state[j]==0){
                    track_play_state[j]=1;  //MUTE track
                    sendCmd(1, mute_track_cmd[j], 127);
                    //Set track LED to OFF
                    setTrackLED(j,OFF);
                }else if(track_play_state[j]==1){
                    track_play_state[j]=2;  //ARM track
                    //sendCmd(1, arm_track_cmd[j], 127);      //Mobuis doesnt understand 'ARM' so don't sending a midi command
                    //Set track LED to AMBER
                    setTrackLED(j,AMBER);
                }else{
                    track_play_state[j]=1;  //MUTE track
                    //sendCmd(1, mute_track_cmd[j], 127);
                    //Set track LED to OFF
                    setTrackLED(j,OFF);
                }
             }else if(mode==1){
                if(rec_mode==0){
                    rec_mode = 1;
                    play_mode=1;

                    // REC on all tracks on first loop (from RESET mode)
                    //The Mobius script handles all tracks so we only need to send one command
                    sendCmd(1, rec_track_cmd[j], 127);
                  
                    for(k=0;k<NUM_TRACKS;k++){
                        track_rec_state[k] = 0;
                        //Set track LEDs to RED
                        setTrackLED(k, RED);
                    }
                }else{
                    //Set track to play when prior state was Record, set other tracks to play

                    //The Mobius script handles all tracks so we only need to send one command
                    sendCmd(1, rec_track_cmd[j], 127);
                    if(track_rec_state[j]==0){                        
                        for(k=0;k<NUM_TRACKS;k++){
                            track_rec_state[k] = 2;
                            //Set track LEDs to OFF
                            setTrackLED(k, GREEN);
                        }
                    }else if(track_rec_state[j]==1){
                        //Set track to play when prior state was overdub
                        track_rec_state[j] = 2;
                        //Set LED on active track to GREEN
                        setTrackLED(j,GREEN);
                        //set non-active tracks to PLAY
                        for(k=0;k<NUM_TRACKS;k++){
                            if(k != active_track_index){
                                track_rec_state[k] = 2;
                                //Set LED on active track to GREEN
                                setTrackLED(k,GREEN);
                            }
                        }
                    }else if(track_rec_state[j]==2){
                        //Set active track to overdub when prior state was play, check if any other track is in overdub and set it to play
                        track_rec_state[j] = 1;
                        //Set LED on active track to RED
                        setTrackLED(j,RED);
                        //set non-active tracks to PLAY
                        for(k=0;k<NUM_TRACKS;k++){
                            if(k != active_track_index){
                                track_rec_state[k] = 2;
                                //Set LED on active track to GREEN
                                setTrackLED(k,GREEN);
                            }
                        }
                    }                    
                }
             }else if(mode==2){
                //volume control
                 sendCmd(1, vol_track_cmd[j], 127);
             }
        }
        btnLastPressTime[i] = millis();
    }
}

void setTrackLED(int track_index, int color){
  //track_index:  0=track1, 1=track2, 2=track3
  //color: 0=OFF, 1=GREEN, 2=RED, 3=AMBER

  //Track 1
  if(track_index==0){
      if(color==0){
          digitalWrite(t1_green_led, HIGH);
          digitalWrite(t1_red_led, HIGH);
      }else if(color==1){
          digitalWrite(t1_green_led, LOW);
          digitalWrite(t1_red_led, HIGH);
      }else if(color==2){
          digitalWrite(t1_green_led, HIGH);
          digitalWrite(t1_red_led, LOW);
      }else if(color==3){
          digitalWrite(t1_green_led, LOW);
          digitalWrite(t1_red_led, LOW);
      }else{
          digitalWrite(t1_green_led, HIGH);
          digitalWrite(t1_red_led, HIGH);
      }
  }else if(track_index==1){
      if(color==0){
          digitalWrite(t2_green_led, HIGH);
          digitalWrite(t2_red_led, HIGH);
      }else if(color==1){
          digitalWrite(t2_green_led, LOW);
          digitalWrite(t2_red_led, HIGH);
      }else if(color==2){
          digitalWrite(t2_green_led, HIGH);
          digitalWrite(t2_red_led, LOW);
      }else if(color==3){
          digitalWrite(t2_green_led, LOW);
          digitalWrite(t2_red_led, LOW);
      }else{
          digitalWrite(t2_green_led, HIGH);
          digitalWrite(t2_red_led, HIGH);
      }
  }else if(track_index==2){
      if(color==0){
          digitalWrite(t3_green_led, HIGH);
          digitalWrite(t3_red_led, HIGH);
      }else if(color==1){
          digitalWrite(t3_green_led, LOW);
          digitalWrite(t3_red_led, HIGH);
      }else if(color==2){
          digitalWrite(t3_green_led, HIGH);
          digitalWrite(t3_red_led, LOW);
      }else if(color==3){
          digitalWrite(t3_green_led, LOW);
          digitalWrite(t3_red_led, LOW);
      }else{
          digitalWrite(t3_green_led, HIGH);
          digitalWrite(t3_red_led, HIGH);
      }
  }else {
      //do nothing
  }
}
