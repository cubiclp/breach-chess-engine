#ifndef UCI_H
#define UCI_H

#include "types.h"

// -------- Time control vars ---------- //

// exit from engine flag
extern int quit;

// UCI "movestogo" command moves counter
extern int movestogo;

// UCI "movetime" command time counter
extern int movetime;

// UCI "time" command holder (ms)
extern int time_left;

// UCI "inc" command's time increment holder
extern int inc;

// search start / stop time (ms)
extern U64 starttime;
extern U64 stoptime;

// variable to flag time control availability
extern int timeset;

// variable to flag when the time is up
extern int stopped;

// called regularly from the search: checks the clock and the GUI input
void communicate(void);

int parse_move(char *move_string);
void parse_position(char *command);
void parse_go(char *command);
void uci_loop(void);

#endif