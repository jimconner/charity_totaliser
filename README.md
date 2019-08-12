# charity_totaliser

This repository contains the code needed to run the charity totaliser project. 

The totaliser is a large display board (8'x4') with very large digits at the top to display the total numerically, and with a decorative vertical strip to indicate the progress towards a given target.

The totaliser is driven by an ESP32 board running Arduino code. 

The user interface for adjusting display values, colours and progress bar is implemented using a TM1638-based 'LED&KEYS' board to provide a simple menu system.

The numeric digits are constructed using strips of addressable RGB leds, arranged into a 7-segment display format. Each segment consists of eight addressable LEDS and with 7 segements per digit, this means that we have 56 LEDs to control per digit.

This code also implements the ability to change colours, colour-pattern-sequences and brightness.


