#include "mbed.h"
#include "rtos.h"
#include "uLCD_4DGL.h"
#include "SDFileSystem.h"
#include "wave_player.h"
#include "Servo.h"
#include <mpr121.h>
#include <string>
#include <list>
#define PCSIZE 8

/* Status LED  instantiation*/
DigitalOut green(p23);
DigitalOut red(p24);
DigitalOut doorlock(p22);
DigitalIn ms(p29, PullUp);
/*uLCD instantiation*/
uLCD_4DGL uLCD(p9,p10,p11); // serial tx, serial rx, reset pin;
Mutex mutex;

//Servo
Servo myservo(p21);

SDFileSystem sd(p5, p6, p7, p8, "sd"); //SD card
//AnalogOut DACout(p18); //audio amp in
//wave_player waver(&DACout);
PwmOut speaker(p22);
//bluetooth init
RawSerial bluemod(p13,p14);

//Create the interrupt receiver object on pin 26
InterruptIn interrupt(p26);
// Setup the i2c bus on pins 9 and 10
I2C i2c(p28, p27);
// Setup the Mpr121:
// constructor(i2c object, i2c address of the mpr121)
Mpr121 mpr121(&i2c, Mpr121::ADD_VSS);


//debug leds
DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
DigitalOut led4(LED4);

//threads
Thread ISRthread(osPriorityAboveNormal);
Thread tLCD;
Thread tAlarm;
Thread tBluetooth;
osThreadId ISRthreadId;

Timer t;


int passcode[PCSIZE] = {5,0,8,0,9,0,1,0};
char bunlock[PCSIZE/2] = {'u','l','c','k'};
char block[PCSIZE/2] = {'l','o','c','k'};
int singleKey;
int userInput[PCSIZE];
char binput[PCSIZE/2];
int keylength;
volatile bool bread = false;;
volatile bool alarm_on = false;;
volatile bool locked = true;
volatile bool passAllow = false;
volatile bool cleared = false;
volatile int tries = 3;

void fallInterrupt() {
    ISRthreadId = osThreadGetId();
    for(;;){
        osSignalWait(0x01, osWaitForever);
        int key_code=0;
        int i=0;
        int value=mpr121.read(0x00);
        value +=mpr121.read(0x01)<<8;
        // LED demo mod
        i=0;
        // puts key number out to LEDs for demo
         for (i=0; i<12; i++) {
         if (((value>>i)&0x01)==1) key_code=i+1;
            }
        led4=key_code & 0x01;
        led3=(key_code>>1) & 0x01;
        led2=(key_code>>2) & 0x01;
        led1=(key_code>>3) & 0x01;
        if (keylength < PCSIZE && !alarm_on) {
            if ((key_code - 1) == 11 and !passAllow and locked) {
                passAllow = true;
                mutex.lock();
                uLCD.cls();
                mutex.unlock();
                keylength = -1;  
            }
            else if((key_code - 1) == 10 and !locked) {
                passAllow = false;
                locked = true;
                mutex.lock();
                uLCD.cls();
                mutex.unlock();
                keylength = -1; 
                tries = 3;
                red = 1;
                green = 0;
                for (int ii = 0; ii < PCSIZE; ii++) {
                    userInput[ii] = 0;
                    keylength = 0;
                }
                for (int ii = 0; ii < PCSIZE/2; ii++) {
                    binput[ii] = NULL;
                }
                myservo = 1.0;
                for(int i=100; i>0; i--) {
                    myservo = i/100.0;
                    Thread::wait(1);
                }
                bread = false;
            }
            else if (passAllow and (key_code-1 != 0) and ((key_code - 1) != 11) and (key_code - 1) != 10){
                userInput[keylength] = key_code - 1;
                keylength++;
            }
            else {
               keylength++; 
            }
        }
    }
}


void passkey() {
    osSignalSet(ISRthreadId,0x01);
    osDelay (2000); 
}

void lcd_thread(void const *args) {
    //int dist = 40;
    uLCD.baudrate(1500000);
    while (true) {
        mutex.lock();
        if (passAllow) {  
            uLCD.line(0, 0, 127, 0,BLUE);
            uLCD.line(127, 0, 127, 127,BLUE);
            uLCD.line(127, 127, 0, 127, BLUE);
            uLCD.line(0, 127, 0, 0, BLUE);
            uLCD.locate(6,2);
            uLCD.printf(" Enter\n"); 
            uLCD.locate(5,3);
            uLCD.printf("Passcode\n");
            uLCD.locate(6,7);
            for (int ii = 0; ii < PCSIZE; ii +=2) {
                uLCD.printf("_ ");
            }
            uLCD.locate(6,7);
            for (int ii = 0; ii < keylength; ii +=2) {
                uLCD.printf("%d ", userInput[ii]);
            }
            uLCD.locate(3,10);
            uLCD.printf("Tries left: %d", tries);
        }
        else if (!locked){
            uLCD.line(0, 0, 127, 0, WHITE);
            uLCD.line(127, 0, 127, 127, WHITE);
            uLCD.line(127, 127, 0, 127, WHITE);
            uLCD.line(0, 127, 0, 0, WHITE);
            uLCD.locate(3,5);
            uLCD.color(0xFFFF00); //yellow text
            uLCD.text_mode(OPAQUE);
            uLCD.printf("System Status: ");
            uLCD.locate(5,7);
            uLCD.color(0x00FF00); //yellow text
            uLCD.printf("Unlocked");
        }
        else {
            uLCD.filled_circle(60, 20, 10, 0xFF00FF);
            uLCD.triangle(40, 30, 50, 25, 60, 40, 0x0000FF);
            uLCD.line(35, 8, 95, 8, 0xFF0000);
            uLCD.line(35, 8, 35, 40, 0xFF0000);
            uLCD.line(35, 40, 95, 40, 0xFF0000);
            uLCD.line(95, 40, 95, 8, 0xFF0000);
            uLCD.filled_rectangle(70, 20, 90, 40, 0xBFBFBF);
            uLCD.line(0, 0, 127, 0,GREEN);
            uLCD.line(127, 0, 127, 127, GREEN);
            uLCD.line(127, 127, 0, 127, GREEN);
            uLCD.line(0, 127, 0, 0, GREEN);
            uLCD.locate(4,6);
            uLCD.color(0xFFFF00); //yellow text
            uLCD.text_width(1.5); //4X size text
            uLCD.text_height(1.5);
            uLCD.printf("Home Entry\n");
            uLCD.locate(5,7);
            uLCD.printf(" System\n");
            uLCD.locate(3,10);
            uLCD.printf("Motion Sensor\n");
            uLCD.locate(3,11);
            uLCD.printf("   to wake\n");
            uLCD.locate(3,12);
            uLCD.printf(" then unlock\n");
            uLCD.locate(3,13);
            uLCD.printf("with passcode\n");
        }
        mutex.unlock();
        //Thread::wait(1);
    }
}

void alarm_thread(void const *args) {
    //FILE *wave_file;
    alarm_on = false;
    while(1)
    {
      if (alarm_on) {
//        //THIS PLAYS SOUNND FROM THE SD CARD
//        wave_file=fopen("/sd/mydir/siren.wav","r");
//        waver.play(wave_file);
//        fclose(wave_file);
////      //}
        t.start();  
        int i;
        for (i=0; i<26; i=i+2) {
            speaker.period(1.0/969.0);
            speaker = float(i)/50.0;
            wait(.001);
            speaker.period(1.0/800.0);
            wait(.001);
        }
        // decrease volume
        for (i=25; i>=0; i=i-2) {
            speaker.period(1.0/969.0);
            speaker = float(i)/50.0;
            wait(.001);
            speaker.period(1.0/800.0);
            wait(.001);
        }
        if (t.read_ms() >= 5000) {
            t.stop();
            speaker = 0.0;
            alarm_on = false;
            locked = true;
            passAllow = false;    
        }  
      }
    }
}

void bluetooth_thread(void const *args) {
    bluemod.baud(9600);
    int allow = 0;
    bread = false;
    while(1) {
        while (bluemod.readable()) {
            if (!passAllow ) {
                for (int ii = 0; ii < PCSIZE/2; ii++) {
                    binput[ii] = bluemod.getc();
                    if (binput[ii] == bunlock[ii]) {
                        allow++;    
                    }   
                }
                if (allow == PCSIZE/2) {
                    passAllow = true;
                    mutex.lock();
                    uLCD.cls();
                    mutex.unlock();   
                }
                for (int ii = 0; ii < PCSIZE/2; ii++) {
                    binput[ii] = NULL;
                }
                allow = 0;     
            }
            else if (!locked) {
                for (int ii = 0; ii < PCSIZE/2; ii++) {
                    binput[ii] = bluemod.getc();
                    if (binput[ii] == block[ii]) {
                        allow++;    
                    }   
                }
                if (allow == PCSIZE/2) {
                    passAllow = false;
                    locked = true;
                    mutex.lock();
                    uLCD.cls();
                    mutex.unlock();
                    tries = 3;   
                }
                for (int ii = 0; ii < PCSIZE/2; ii++) {
                    binput[ii] = NULL;
                }
                myservo = 1.0;
                for(int i=100; i>0; i--) {
                    myservo = i/100.0;
                    Thread::wait(1);
                }
                allow = 0; 
            }
            else {
                for (int ii = 0; ii < PCSIZE/2; ii++) {
                    binput[ii] = bluemod.getc();
                }
                bread = true;                    
            }
        }
    }    
}

bool checksum() {
    bool keycorrect = true;
    for(int ii = 0; ii < PCSIZE; ii+=2) {
        if (passcode[ii] != userInput[ii]) {
             keycorrect = false;
             break;
        }
    }
    bool bluecorrect = true;
    for (int ii = 0; ii < PCSIZE/2; ii++) {
        if (((int)(binput[ii] - '0')) != passcode[ii*2]) {
            bluecorrect = false;
            led2 = !led2;
            break;
        }
    }     
    return (keycorrect || bluecorrect);    
}

int main() {
    while (ms==0) {
        led1 = 1;
        Thread::wait(2);
        led1 = 0;
        Thread::wait(2);      
    }
    
    singleKey = 0;
    keylength = 0;
    red = 1;
    green = 0;
    doorlock = 1;
    ISRthread.start(callback(fallInterrupt));
    interrupt.fall(&passkey);
    interrupt.mode(PullUp); 
    tLCD.start(callback(lcd_thread,(void *)"See things"));
    tAlarm.start(callback(alarm_thread,(void *)"Hear things"));
    tBluetooth.start(callback(bluetooth_thread,(void *)"smurf things"));

    while(1)
    {
       if (passAllow && !alarm_on) {
           if ((keylength == PCSIZE) || bread) {
             if (checksum()){
                locked = false;
                passAllow = false;
                mutex.lock();
                uLCD.cls();
                mutex.unlock();
                led3 = !led3;
                red = 0;
                green = 1; 
                singleKey = 2;
                keylength = 0;
                doorlock = 0;
                for(int i=0; i<100; i++) {
                    myservo = i/100.0;
                    Thread::wait(1);
                    }
             }
             else {
                tries--;
                if (tries == 0) {
                    passAllow = false;
                    alarm_on = true;
                    locked = true;
                    mutex.lock();
                    uLCD.cls();
                    mutex.unlock();
                    tries = 3;   
                }
                red = 1;
                for (int ii = 0; ii < PCSIZE; ii++) {
                    userInput[ii] = 0;
                    keylength = 0;
                }
                bread = false;      
             }
          }
       }
    }
}