/*
 * main.c
 * Obstacle avoiding car on a PIC18F45K22: drives forward, and when the HC-SR04
 * reads under 20 cm it stops, reverses and turns right through the L298N.
 */

#include <p18cxxx.h>
#include <delays.h>

// Motor control pins
#define LeftMotorForward   PORTDbits.RD0
#define LeftMotorBackward  PORTDbits.RD1
#define RightMotorForward  PORTDbits.RD2
#define RightMotorBackward PORTDbits.RD3

// HC-SR04 pins
#define trig_pin  PORTBbits.RB0
#define echo_pin  PORTBbits.RB4

// Servo pin
#define servo_pin PORTCbits.RC2

// Servo direction
#define servo_forward()  dir = 0
#define servo_right()    dir = 1
#define servo_left()     dir = -1

#pragma config FOSC = INTIO67  // Internal OSC


unsigned int distance;
unsigned int timeout;
unsigned int duration;
unsigned int dist_right, dist_left;
char dir;

// function
void setup(void);
void stop_car(void);
void go_forward(void);
void go_backward(void);
void go_right(void);
void go_left(void);
void lookRight(void);
void lookLeft(void);
unsigned int measure_distance(void);
void avoidObstacle(void);

//====================================================================
// Initializes all I/O pins, timers, and variables for motor, servo, and sensor operation
void setup(void) {
    //Clock = 4 MHz
    OSCCON = 0b01010000;

    //Motor pins = digital output
    TRISD &= 0xF0;
    ANSELD &= 0xF0;

    //HC-SR04 trigger = output, echo = input (digital)
    TRISBbits.TRISB0 = 0;
    ANSELBbits.ANSB0 = 0;
    TRISBbits.TRISB4 = 1;
    ANSELBbits.ANSB4 = 0;

    //Servo pin = digital output
    TRISCbits.TRISC2 = 0;
    ANSELCbits.ANSC2 = 0;

    //Timer0 = servo PWM ISR
    T0CON = 0b10001000; // 16-bit, prescale 1:1
    INTCONbits.TMR0IE = 1; // enable Timer0 interrupt
    INTCONbits.GIE = 0; // global ints ON
    INTCONbits.TMR0IF = 1; // clear flag

    // Timer1 = distance timing, 16-bit, Fosc/4, prescale 1:1, OFF
    T1CON = 0b00000000; // RD16=1 for atomic 16-bit reads
    // TMR1CS=00, CKPS=00, TMR1ON=0

    //Initial servo & motor state
    dir = 0;
    trig_pin = 0;
    servo_pin = 0;

    //Preload Timer0 so servo pulse width comes correct on first ISR
    {
        unsigned int preload = 65536 - 18500;
        TMR0L = preload & 0xFF;
        TMR0H = preload >> 8;
    }
}

//====================================================================
// Timer0 ISR used to generate ~20ms PWM with 1-2ms pulse width for servo control
#pragma code ISR = 0x0008
#pragma interrupt ISR

void ISR(void) {
    INTCONbits.TMR0IF = 0;

    if (dir == 0) {
        // center
        if (servo_pin) {
            TMR0L = (65536 - 1500) & 0xFF;
            TMR0H = (65536 - 1500) >> 8;
        } else {
            TMR0L = (65536 - 18500) & 0xFF;
            TMR0H = (65536 - 18500) >> 8;
        }
    } else if (dir > 0) {
        // right
        if (servo_pin) {
            TMR0L = (65536 - 1806) & 0xFF;
            TMR0H = (65536 - 1806) >> 8;
        } else {
            TMR0L = (65536 - 18194) & 0xFF;
            TMR0H = (65536 - 18194) >> 8;
        }
    } else {
        // left
        if (servo_pin) {
            TMR0L = (65536 - 1194) & 0xFF;
            TMR0H = (65536 - 1194) >> 8;
        } else {
            TMR0L = (65536 - 18806) & 0xFF;
            TMR0H = (65536 - 18806) >> 8;
        }
    }

    servo_pin = ~servo_pin; // toggle output
}

//====================================================================
// Main loop

void main(void) {
    setup();
    go_forward();

    while (1) {
        // take a clean distance reading
        INTCONbits.GIE = 0;
        distance = measure_distance();
        INTCONbits.GIE = 1;

        if (distance < 20) {
            avoidObstacle();
        } else {
            go_forward();
        }
    }
}
//====================================================================
// Obstacle avoidance
// Main obstacle avoidance logic: stop, back up, scan sides, and choose best turn

void avoidObstacle(void) {
    stop_car();
    Delay10KTCYx(30); // 300 ms

    go_backward();
    Delay10KTCYx(50); // 500 ms
    stop_car();

    // look right and left, pick the larger gap
    //lookRight();
    //lookLeft();

    //if (dist_right >= dist_left)
    //    go_right();
    //else
    //    go_left();

    go_right();
    Delay10KTCYx(100); // let the turn complete
    stop_car();
}

//====================================================================
// Sweep + measure helpers

// Moves the servo to the right position and measures distance in that direction
void lookRight(void) {
    servo_right();
    Delay10KTCYx(50); // 500 ms to move servo
    dist_right = measure_distance();
    Delay10KTCYx(10);
    servo_forward();
}

// Moves the servo to the left position and measures distance in that direction
void lookLeft(void) {
    servo_left();
    Delay10KTCYx(50);
    dist_left = measure_distance();
    Delay10KTCYx(10);
    servo_forward();
}

//====================================================================
// Basic motor commands

// Stops all motor activity, making the car halt in place
void stop_car(void) {
    PORTD &= 0xF0;
}

// Activates motors to drive the car forward
void go_forward(void) {
    LeftMotorForward = 1;
    RightMotorForward = 1;
    LeftMotorBackward = 0;
    RightMotorBackward = 0;
}

// Reverses both motors to move the car backward
void go_backward(void) {
    LeftMotorForward = 0;
    RightMotorForward = 0;
    LeftMotorBackward = 1;
    RightMotorBackward = 1;
}

// Turns the car right by rotating left motor forward and right motor backward
void go_right(void) {
    LeftMotorForward = 1;
    RightMotorForward = 0;
    LeftMotorBackward = 0;
    RightMotorBackward = 1;
}

// Turns the car left by rotating right motor forward and left motor backward
void go_left(void) {
    LeftMotorForward = 0;
    RightMotorForward = 1;
    LeftMotorBackward = 1;
    RightMotorBackward = 0;
}

//====================================================================
// Measure distance via HC-SR04
// Triggers HC-SR04 ultrasonic sensor and returns measured distance in cm (255 on timeout)

unsigned int measure_distance(void) {
    // reset & stop Timer1
    T1CONbits.TMR1ON = 0;
    TMR1H = 0;
    TMR1L = 0;
    timeout = 0;

    while (!timeout) {
        // send 10 µs trigger pulse
        trig_pin = 0;
        Nop();
        Nop();
        trig_pin = 1;
        Delay10TCYx(1);
        trig_pin = 0;

        // wait for echo high (timeout if no echo)
        timeout = 30000;
        while (!echo_pin && --timeout);

        // start timing
        T1CONbits.TMR1ON = 1;

        while (echo_pin);
        T1CONbits.TMR1ON = 0;
    }

    // read 16-bit count
    duration = TMR1H;
    duration <<= 8;
    duration += TMR1L;

    // convert to cm (58 µs per cm)
    return (unsigned int) (duration / 58);
}
