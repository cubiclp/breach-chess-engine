<div align="center">
<img src="logo.png" alt="logo" width="200"/>

<div align="left">

## 
Breach — UCI Chess Engine

Breach is an open-source UCI chess engine written in C, and it is simply my hobby project.

The engine does not come with a GUI, thus you will need to download GUI. I can recommend
[en-croissant](https://github.com/franciscoBSalgueiro/en-croissant).

This engine is built directly on top of the BBC engine base by Maksim Korzh (Code Monkey King), 
it is not merely my own creation. I want to thank Maksim Korzh for the amazing tutorial series. Furthermore,
the engine was developed with the help of AI.

I currently estimate the engines strength to be ~2400 CCRL Blitz scale.

The engine is **WIP** and almost certainly not completely bug free.

### Compile

Compile the engine via the CMakeLists.txt. I would recommend using GCC as
a compiler and make sure your compiler supports LTO.

### Technical Details

Search

- Null move pruning
- Transposition table
- Check search extension
- Reverse futility pruning
- Late move reduction
- Principle variation search
- Killer move heuristics
- History move heuristics
- Iterative deepening
- Aspiration windows
- MVV LVA & SEE move ordering
- SEE pruning
- Futility pruning
- Internal iterative reduction

Eval - HCE

- Material
- Piece square tables
- Mobility
- Bishop pair
- Basic king safety
- Isolated, doubled & passed pawns
- Passed pieces
- Tapered evaluation

### License

Breach is licensed under the GNU General Public License v3.0 (GPL-3.0), 
inherited from [BBC](https://github.com/maksimKorzh/bbc) by Maksim Korzh.