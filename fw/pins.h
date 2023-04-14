#pragma once

typedef struct {
  volatile uint8_t* port;
  volatile uint8_t* pin;
  uint8_t pin_num;
} pin_t;

// rows
pin_t d1 = {&PORTD, &PIND, 1};
pin_t d0 = {&PORTD, &PIND, 0};
pin_t d4 = {&PORTD, &PIND, 4};
pin_t c6 = {&PORTC, &PINC, 6};
// cols
pin_t b2 = {&PORTB, &PINB, 2};
pin_t b5 = {&PORTB, &PINB, 5};
pin_t b4 = {&PORTB, &PINB, 4};
pin_t e6 = {&PORTE, &PINE, 6};
pin_t d7 = {&PORTD, &PIND, 7};
pin_t b6 = {&PORTB, &PINB, 6};
