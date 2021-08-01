//Main changes
//Key increment moved to 0-6, location of key assignment changed
//Using Loop Length variable

//TODO
//Investigate more consice audio object initialization and parameter setting
//Create Drum loop from list callback function 
//Create instrument process callback function with inst and tone as input parameters.

//Create Process Tone functions for instruments? Custom classes? - C
//Customizable Drum Loops from ESP32
//Tweakable Tone Parameters
//Investigate New Audio Effects


//Import Libraries
#include "daisy_seed.h"
#include "daisysp.h"
#include <math.h>

// Use the daisy namespace to prevent having to type
// daisy:: before all libdaisy functions
using namespace daisy;
using namespace daisysp;

//Initialize DaisySeed Audio objects 
DaisySeed hw;
static Metro tick;
static Oscillator osc;
static Autowah autowah;
ReverbSc verb; 
SyntheticBassDrum bd;
SyntheticSnareDrum sd;

//Initialize DaisySeed HW objects
Led redLed, gLed, yLed, bLed;
Switch note1, note2, note3, note4, note5, recbtn ;


//TODO Add Jambot Class to keep track of device state and settings.
//Global State Variables setup once but used in Audio Callback
float key_list[7][5]{};

//TODO: Find way to switch drum loops Add flexible loop length
//   Create Drum Loop Switch Function - takes a new loop array as input, calculates loop length, sets global variables
//int kick_loop[] =  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}; 
//int snare_loop[] = {0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1};
int kick_loop[] =  {0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1};
int snare_loop[] = {0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1};
int note_loop[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int loop_length = 32;
int count = loop_length-1;
bool recording = false;

//Initialize Daisy Seed Parameters
void setupDaisySeed(){
    //Daisy Seed Initialization
  hw.Configure();
  hw.Init();
  hw.SetLed(true);
  float sample_rate = hw.AudioSampleRate();

  //LEDs
  redLed.Init(hw.GetPin(8),false);
  yLed.Init(hw.GetPin(10),false);
  gLed.Init(hw.GetPin(11),false);
  bLed.Init(hw.GetPin(12),false);
    
  //Switches
  note1.Init(hw.GetPin(0),sample_rate/48.f);
  note2.Init(hw.GetPin(1),sample_rate/48.f);
  note3.Init(hw.GetPin(2),sample_rate/48.f);
  note4.Init(hw.GetPin(3),sample_rate/48.f);
  note5.Init(hw.GetPin(4),sample_rate/48.f);
  recbtn.Init(hw.GetPin(5),sample_rate/48.f);

  //Analog Digital Conversion
  AdcChannelConfig adcConfig[3];
  adcConfig[0].InitSingle(hw.GetPin(15));
  adcConfig[1].InitSingle(hw.GetPin(16));
  adcConfig[2].InitSingle(hw.GetPin(17));
  
  //Init and Start ADC
  hw.adc.Init(adcConfig,3);
  hw.adc.Start();

  //Oscillator
  osc.Init(sample_rate);
  osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
  osc.SetFreq(100);
  osc.SetAmp(0.25);

  //Autowah
  autowah.SetLevel(.8);
  autowah.SetDryWet(100);
  autowah.SetWah(1);

  //Verb
  verb.Init(sample_rate);
  verb.SetFeedback(0.85f);
  verb.SetLpFreq(18000.0f);

  //Synth Kick Drum
  bd.Init(sample_rate);
  bd.SetFreq(80.f);
  bd.SetDirtiness(.5f);
  bd.SetFmEnvelopeAmount(.6f);
  bd.SetAccent(.8f); //random() / (float)RAND_MAX);
  bd.SetDecay(.8f);

  //Synth Snare Drum
  sd.Init(sample_rate);
  sd.SetAccent(0.75f);
  sd.SetDecay(0.3f);
  sd.SetSnappy(0.75f);

  //Metronome
  tick.Init(1.f, sample_rate);
}

//Generate KeyFrequencies from list of roots, store in global variable
void getKeys(){
    float root_freq[]={82.41f, 87.31f, 98.00f, 110.00f,123.47f,130.81f,146.83f}; // E2, F,G, A, B, C, D,
    int pent_st[]= {0,3,5,7,10}; //semitones away from root for each note of the minor pentatonic scale

    for (int i=0;i<7; i++){
        float root = root_freq[i];
      for (int j=0;j<5;j++){
        int n = pent_st[j];
        float p = (float) n/ 12.0f;
        key_list[i][j] = root * pow(2,p);
        }
    }
}

//Get Notes from Button Press
int getBtnNote(){  
  //Read Button States
  note1.Debounce();
  note2.Debounce();
  note3.Debounce();
  note4.Debounce();
  note5.Debounce();
  
//Generate Note State from Buttons
  if(note1.Pressed()){return 1;}// osc.SetFreq(key[0]*oct);}
  else if(note2.Pressed()){return 2;}// osc.SetFreq(key[1]*oct);}
  else if(note3.Pressed()){return 3;}// osc.SetFreq(key[2]*oct);}
  else if(note4.Pressed()){return 4;}//osc.SetFreq(key[3]*oct);}
  else if(note5.Pressed()){return 5;}//osc.SetFreq(key[4]*oct);}
  else{return 0;}
}

//check recording state from button
void getRecordingState(){
  recbtn.Debounce();

  //Check recording button 
  if(recbtn.RisingEdge()){
      if (recording){
          redLed.Set(0.0);
          redLed.Update();
          for (int i=0; i<32; i++){note_loop[i] = 0;}
          recording = false;
      } else {
          redLed.Set(1.0);
          redLed.Update();
          recording = true;
      }
  } 
}

//Set Instrument State Based off of Knobs
int getInstKnob(){
    //Read potentiometer 2 state - set to octave multiplier
      float pot2 = hw.adc.GetFloat(1);//TODO: Move analog read
      int inst = (int) ((pot2/0.3f) + 1);//outputs 4 octave settings 
      return inst;
}

//Reads potentiometer and sets tempo
void setTempo(){
  //Read Potentiometer 1 State - Set Bass Tempo
      float pot1 = hw.adc.GetFloat(0);//TODO: Move analog read
      float tempo = pot1/.150f;
      tick.SetFreq(tempo);
}

//Get Key Value From Knob
int getKeyVal(){
      float pot3 = hw.adc.GetFloat(2); //TODO: Move analog read
      int key_val = (int) ((pot3/0.14f));//outputs 7 octave settings #TODO Check change to key_val increment
      return key_val;
  }

//Handle Looper Playback and Recording. Returns note from looper or stores value
int syncLooper(int note){
    if (note==0 && note_loop[count] > 0){
        return note_loop[count];
      } 
    else if (note > 0 && recording){  //if recording and note is present, change note_loop to played note
        note_loop[count] = note;
        return note;
        } 
    else {
          return note;
        }
}

//Play Synth sound and adjust output based on state
float playSynth(int note, int key_val, int inst){
  //Synth Output  

    //Assign frequency to note
    float freq = key_list[key_val][note-1];
    
  //Main output call for the signal based on Mode
    if (note==0) {return 0;} //If no note is playing clear output to prevent noise
    
    if(note>0){ osc.SetFreq(freq);} //Set frequency based on note 
    //Bass
    if(inst == 1){ 
    return osc.Process();
    }
    //WAH
    else if (inst ==2) {
      float out = osc.Process();
      return autowah.Process(out);
    }
    //VERB
    else if (inst ==3){ 
    return osc.Process();
    
    }
    //LEAD
    else {
      //harm.SetFreq(key[note-1]*oct);
      return osc.Process();//harm.Process();
      }
}

//Play Drums - pass in tick value
float playDrums(float t){
    float out;
    if (kick_loop[count] ==1){out =  bd.Process(t);}
    //Snare Output
    if (snare_loop[count] ==1){out = out + sd.Process(t);}
    return out;
}

//Increment Click - return count
void incrementClick(float t){
  if (t) {      
        //Increment Count by 1 unless you are on the last beat then change to 0
        if(count < (loop_length-1)){
          count = count + 1;
          }else{
            count = 0;
            }
    }
  }

//Audio Callback
void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size) 
{ 
  //Loop mix variables
  float output, synthout, drumout, vout1, vout2;
  
  
  // Start Main Callback Sample Loop
  for (size_t i = 0; i < size; i+= 2) {
    
    //Process Click Each Iteration
    float t = tick.Process(); 
    
    //Sets tempo from knob
    setTempo(); 

    //Increments Metronome
    incrementClick(t);
  
    //Get note from buttons 
    int note = getBtnNote();

    //Get Recording State -VERIFY RECORDING STATE
    getRecordingState();

    //Get Key value from knob
    int key_val = getKeyVal();

    //Get instrument setting from Knob -Change to map to two variables
    int inst = getInstKnob();
    
    //Looper Playback and Recording
    note = syncLooper(note);
    
    //Synth Output      
    synthout = playSynth(note, key_val, inst);
    
    //Drum Output
    drumout = playDrums(t);
    
    //Mix Main Output
    output = synthout + drumout;

    //Reverb on input channels
    verb.Process(in[i+1], in[i+1], &vout1, &vout2);

    //Mix Reverb and Generate Output
    out[i] = output * 0.5f + vout1 +in[i+1];
    out[i+1] = output * 0.5f + vout2 + in[i+1];

  }//End sample loop
}//End Callback


//Main function
int main(void) {

  //Call setup function
  setupDaisySeed();

  //Set Keys from roots: 
  getKeys();

  //Start Audio Callback Loop - TODO Move to class - Jambot::run()
  hw.StartAudio(AudioCallback);

//Loop for non audio related callbacks
    for(;;){
      //System Delay
      //System::Delay(1);

    }//End For Loop

}//End Main
