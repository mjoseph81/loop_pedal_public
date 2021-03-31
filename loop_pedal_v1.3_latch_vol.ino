 /* 
 *  FILE    :   looper_pedal_midi_volume_undo.ino
 *  AUTHOR  :   Matt Joseph
 *  DATE    :   8/20/2018  
 *  VERSION :   1.3
 *  
 *
 *  DESCRIPTION
 *  https://www.instructables.com/id/DIY-Chewie-Monsta-Looper-Based-on-Ed-Sheerans/
 *  
 *  
 *  
 *  REV HISTORY
 *  1.0) Initial release per description
 *  1.1) Added volume control functionality
 *  1.2) Added bit shift latching function for button reads
 *       Updated MIDI command values for each output
 *  1.3) Added "UNDO" function to "CLEAR"  button
 *       Short press (<1000ms) activates "UNDO"
 *       Long press (>=1000ms) activates "CLEAR"
 */  

#define NUM_BUTTONS  7


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
const uint8_t reset_cmd = 0x10;
const uint8_t undo_cmd = 0x11;
const uint8_t clear_cmd = 0x12;
const uint8_t play_start_cmd = 0x20;
const uint8_t rec_start_cmd = 0x30;
const uint8_t vol_start_cmd = 0x40;

const uint8_t buttons[NUM_BUTTONS] = {btn_reset, btn_clear, btn_rec_play, btn_mult_stop, btn_track1, btn_track2, btn_track3};


const int ledPin =  LED_BUILTIN;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED
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


void setup() {
  // Set MIDI baud rate:
  //Serial.begin(31250);
  // Set baud rate to 38400 for Hairless MIDI
  Serial.begin(38400);
  
  // set the digital pin as output:
  pinMode(ledPin, OUTPUT);
  pinMode(recLED, OUTPUT);
  pinMode(volLED, OUTPUT);
  
  for (int i = 0; i < NUM_BUTTONS; i++){
    pinMode(buttons[i], INPUT_PULLUP);
  }
  pinMode(btn_mode, INPUT_PULLUP);
}


void loop() {
  readButtons();
}


void readButtons()
{
  
  //check state of MODE button
  modeValue = digitalRead(btn_mode);      //read current input
  modeBuffer = modeBuffer << 1;           //shift buffer 1 bit left
  bitWrite(modeBuffer, 0, modeValue);     //add new sample to buffer

  
  priorModeEval = modeEval;               //update last evaluated value of MODE
  modeEval = modeBuffer & 0x07;           //keep last 3 samples for evaluation (0b00000111)
  
  if ( modeEval == 0x00)   //Evaluated input has at least 3 sequential 0s
  {   
      if (modeEval != priorModeEval)        //if evaluated MODE has changed from 1 to 0 for 3 consecutive samples (button pressed)
      {
          modeChange = 1;
         
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
      if(mode==0)
      {
          sendCmd(1, play_start_cmd, 127);
      }else if(mode==2){
          sendCmd(1, play_start_cmd, 127);
      }else{
          sendCmd(1, rec_start_cmd, 127);
      }
  }


  //Check button status and send correct MIDI Command
  //RESET and CLEAR buttons do not depend on MODE.  Evaulate them separately
  //Logic for RESET button
  if (btnEval[0] == 0){
      if(btnEval[0] != priorBtnEval[0]) 
          {sendCmd(1, reset_cmd, 127);}
  }

  //Logic for CLEAR button
  if (btnEval[1] == 0){
      //handle short press
      if(btnEval[1] != priorBtnEval[1]){                      
            sendCmd(1, undo_cmd, 127);
      //handle long press
      }else if((millis() - clearLongPress) >= longPressTime){
            sendCmd(1, clear_cmd, 127);
            clearLongPress = millis();
      }
  }else{
      //reset last time clear was pressed
      clearLongPress = millis();
  }

  if(mode==0){
  //Check for button presss and send correct note in Play mode for the rest of the buttons
  //Start at index 2 in the array since RESET and CLEAR are handled outside the loop
      for( int i=2;i<NUM_BUTTONS;i++){
           if (btnEval[i] == 0){
              if(btnEval[i] != priorBtnEval[i])
                    {sendCmd(1, play_start_cmd+i, 127);}
          }
      }
  }else if(mode==1){
  //Check for button presss and send correct note in REC mode for the rest of the buttons
  //Start at index 2 in the array since RESET and CLEAR are handled outside the loop
      for( int i=2;i<NUM_BUTTONS;i++){
           if (btnEval[i] == 0){
              if(btnEval[i] != priorBtnEval[i])
                    {sendCmd(1, rec_start_cmd+i, 127);}
          }
      }
  }else if(mode==2){
  //Check for button presss and send correct note in VOL mode for the rest of the buttons
  //Start at index 4 in the array since VOL CTL only uses TRACK1/2/3 buttons
      for( int i=4;i<NUM_BUTTONS;i++){
           if (btnEval[i] == 0){
              if(btnEval[i] != priorBtnEval[i])
                    {sendCmd(1, vol_start_cmd+i, 127);}
          }
      }
  }
  
  delay(30);
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



