#ifndef TUNER_H
#define TUNER_H

// Texel-tunes the weights in weights_file (read, then overwritten with the result) on the
// positions in positions_file ("<fen> <result>" per line, result 1-0 / 0-1 / 1/2-1/2).
// max_positions <= 0 means use the whole file. Returns 0 on success, -1 on error.
// The engine must be initialised first; the board position is overwritten.
int tune(char *weights_file, char *positions_file, int max_positions);

#endif
 