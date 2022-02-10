/*  
 * ____                     _      ______ _____    _____
  / __ \                   | |    |  ____|  __ \  |  __ \               
 | |  | |_ __   ___ _ __   | |    | |__  | |  | | | |__) |__ _  ___ ___ 
 | |  | | '_ \ / _ \ '_ \  | |    |  __| | |  | | |  _  // _` |/ __/ _ \
 | |__| | |_) |  __/ | | | | |____| |____| |__| | | | \ \ (_| | (_|  __/
  \____/| .__/ \___|_| |_| |______|______|_____/  |_|  \_\__,_|\___\___|
        | |                                                             
        |_|          
 Open LED Race
 An minimalist cars race for LED strip  
  
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 by gbarbarov@singulardevices.com  for Arduino day Seville 2019 
 https://www.hackster.io/gbarbarov/open-led-race-a0331a
 https://twitter.com/openledrace
 
 Code made dirty and fast, next improvements in: 
 https://gitlab.com/open-led-race
 https://openledrace.net/open-software/

 Modified by Robotech
*/

                                                            
#include <Adafruit_NeoPixel.h>
#define MAXLED         50 // MAX LEDs actives on strip

//Pins Maker Faire Roma 19 version 
#define PIN_LED        2  // R 500 ohms to DI pin for WS2812 and WS2813, for WS2813 BI pin of first LED to GND  ,  CAP 1000 uF to VCC 5v/GND,power supplie 5V 2A  
#define PIN_P1         A0 // switch player 1 to PIN and GND
#define PIN_P2         A2 // switch player 2 to PIN and GND 
#define PIN_AUDIO      3  // through CAP 2uf to speaker 8 ohms

int NPIXELS=MAXLED; /* leds on track */


int win_music[] = {
  2637, 2637, 0, 2637, 
  0, 2093, 2637, 0,
  3136    
};
      
//byte  gravity_map[MAXLED];     

//int TBEEP=3; 

float speed1=0;
float speed2=0;
float dist1=0;
float dist2=0;

byte loop1=0;
byte loop2=0;

byte leader=0;
byte loop_max=5; /* total laps race */


float ACEL=0.04;
float kf=0.015; /* friction constant */

byte flag_sw1=0; /* If the race stars the can move */
byte flag_sw2=0;
byte draworder=0; /* To show the two cars when one is over the other one */
 
unsigned long timestamp=0;

Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);

int tdelay = 10; 


void setup() {
  track.begin(); 
  pinMode(PIN_P1,INPUT_PULLUP); 
  pinMode(PIN_P2,INPUT_PULLUP);  
  Serial.begin(9600);
 
}


/* Init sequence */
void start_race(){
  for (int i = 0; i < NPIXELS ; i++){
      track.setPixelColor(i, track.Color(0, 0, 0));
      track.show(); 
    }
  track.setPixelColor(12, track.Color(0,255,0));
  track.show();
  
  tone(PIN_AUDIO,400);
  delay(2000);
  noTone(PIN_AUDIO);  
                  
  track.setPixelColor(12, track.Color(0,0,0));
  track.show();
  delay(500);
  track.setPixelColor(11, track.Color(255,255,0));
  track.show();
  tone(PIN_AUDIO,600);
  delay(2000);
  noTone(PIN_AUDIO);  
                  
  track.setPixelColor(11, track.Color(0,0,0));
  track.show();
  delay(500);
  track.setPixelColor(10, track.Color(255,0,0));
  track.show();
  tone(PIN_AUDIO,1200);
  delay(2000);
  noTone(PIN_AUDIO);  

  track.setPixelColor(11, track.Color(0,0,0));
  track.show();                     
  timestamp=0;  
}
     

void winner_fx() {
  int msize = sizeof(win_music) / sizeof(int);
  for (int note = 0; note < msize; note++) {
    tone(PIN_AUDIO, win_music[note],200);
    delay(230);
    noTone(PIN_AUDIO);
  }  
}                                         

/* CHANGED loop1 and loop2 */
int global_car = 0;
/* CHANGED loop1 and loop2 */
void draw_car1(void){for(int i=0;i<=global_car;i++){track.setPixelColor(((word)dist1 % NPIXELS)+i, track.Color(0,255,0));};                   
  }

void draw_car2(void){for(int i=0;i<=global_car;i++){track.setPixelColor(((word)dist2 % NPIXELS)+i, track.Color(255,0,0));};            
 }


int carrera(){
  while (true){
    track.clear();
   /* Moves car 1 */
    if ( (flag_sw1==1) && (digitalRead(PIN_P1)==0) ) {flag_sw1=0;speed1+=ACEL;};
    if ( (flag_sw1==0) && (digitalRead(PIN_P1)==1) ) {flag_sw1=1;};

    speed1-=speed1*kf; 
    
    /* Moves car 2 */
    if ( (flag_sw2==1) && (digitalRead(PIN_P2)==0) ) {flag_sw2=0;speed2+=ACEL;};
    if ( (flag_sw2==0) && (digitalRead(PIN_P2)==1) ) {flag_sw2=1;};
      
    speed2-=speed2*kf; 
        
    dist1+=speed1;
    dist2+=speed2;

    if (dist1>NPIXELS) {loop1++;dist1 = 0;};
    if (dist2>NPIXELS) {loop2++;dist2 = 0;};
    
    /* First player wins */
    if (loop1>loop_max) {return 1;}
    
    /* Second player wins */
    if (loop2>loop_max) {return 0;}
    /* Draws one car over the other one */                      
    if ((millis() & 512)==(512*draworder)) {if (draworder==0) {draworder=1;}
                          else {draworder=0;}   
                         }; 

    if (draworder==0) {draw_car1();draw_car2();}
        else {draw_car2();draw_car1();}   
                 
    track.show(); 
    delay(tdelay);
    }
}

void fin_carrera(int ganador){
    int color1 = 0;
    int color2 = 0;
    if (ganador == 0){color1 = 255; color2 = 0;}
    else{color1 = 0; color2= 255;}
    Serial.println("ENCIENDETE PUTO");
    for (int i = 0; i < NPIXELS ; i++){
      track.setPixelColor(i, track.Color(color1, color2,0));
      track.show(); 
    }

    //winner_fx();
    delay(2000); 
    loop1=0;
    loop2=0;
    dist1=0;
    dist2=0;
    speed1=0;
    speed2=0;
    timestamp=0;
}

  
void loop() {
    int ganador;
    /* Turns off all the leds */
    track.clear();
    start_race();   
    ganador = carrera();
    Serial.println(ganador);
    fin_carrera(ganador);
        
}
                                                    
