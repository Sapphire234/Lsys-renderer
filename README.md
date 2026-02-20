# Lsys-renderer
Command-based L-system renderer in C with turtle graphics and PPM output


## Overview

This project is built around a custom shell that interprets commands in real time.  
Instead of drawing shapes manually, the program generates patterns from L-system rules and then renders them visually.  
I also implemented an UNDO/REDO mechanism. Each successful command stores a deep copy of the program state, allowing previous versions to be restored 
without corrupting the memory.   

## Features
1. Pattern Generation with L-Systems

The renderer uses Lindenmayer Systems to expand simple rule sets. A small set of symbols can grow into detailed patterns through iterative rewriting.

2. Coordinate-Based Rendering (Turtle Graphics)

Generated symbols are interpreted by a turtle graphics function that converts them into movements and rotations in a 2D coordinate system.

Lines are drawn using the Bresenham algorithm to ensure accurate rendering on a discrete pixel grid (binary PPM format).

3. Bitmap Font Support (BDF)

This project includes support for BDF bitmap fonts.

Glyphs are parsed from hexadecimal format, converted into binary matrices, and rendered onto the image with customizable position and color. 
To ensue proper text aligment, character offsets and cursor positioning for the next glyph are made.

4. Bit-Level Diagnostic Tool

A built-in BITCHECK utility scans the image at bit level and detects specific patterns (0010 and 1101), that could cause errors.

Detected sequences are mapped back to their exact pixel coordinates, providing an inspection tool for analyzing the image data.

## Technical details

Dynamic memory allocation across all major components

Deep copy implementation for persistent UNDO history

## File support:

Binary PPM images

.lsys grammar definitions

.bdf bitmap fonts

## Setup
make build
./runic

## Example usage:

LOAD background.ppm
FONT mystic.bdf
TYPE "Rune Activated" 50 50 0 255 0
SAVE result.ppm
