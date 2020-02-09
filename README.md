# Othello
Classical game of Othello

This repository includes the source code in C++11, a compiled version (for Windows 32-bits), and a well documented configuration launch file (othello.bat), which allows the change of board size, AI's parameterization...

Compilation instructions are included in the othello.cpp stand-alone source code - it is the only source file needed. I've used MinGW and compiled using:

    g++ -Wall -O3 -o "othello" "othelo.cpp" -s -std=c++11 -static-libgcc -static-libstdc++ -static -lwinpthread
If you have MinGW installed, you can generate a smaller .exe file using:

    g++ -Wall -O3 -o "othello" "othello.cpp" -s -std=c++11
Othello's Artificial Intelligence is a Monte-Carlo, and the software uses parallel threading for maximum efficiency. Monte-Carlo is a fairly efficient strategy for a game like Othello and the computer will play quite accurately...

From the game, press F1 to display the help screen at any time, which describes the keyboard and mouse input allowed.
