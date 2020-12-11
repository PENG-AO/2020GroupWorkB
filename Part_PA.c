#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATTACKER 0
#define DEFENDER 1

// attacker's piece
// FU = 0, HI, KK, GI, KI, OU
// defender's piece
// FU = 8, HI, KK, GI, KI, OU

typedef unsigned char Pos;
typedef unsigned long long HalfBoard;
typedef struct board
{
    HalfBoard attacker;
    HalfBoard defender;
} Board;

// struct of move
// type: 0-5 -> placement of a off-board piece (5 is impossible)
//       -1 -> movement with promotion
//       -2 -> movement without promotion
// player: ATTACKER or DEFENDER
// from, to: move the piece from * to *
typedef struct move
{
    int type, player;
    Pos from, to;
} Move;

int max(int a, int b) { return (a > b) ? a : b; }
int min(int a, int b) { return (a < b) ? a : b; }
// swich the half-pos expression between digit and alphabet: 2 -> A -> 2
int convert(int p) { return (p + 0x8) % 0x10; }

void initBoard(Board* bp)
{
    bp->attacker = 0x111213141521;
    bp->defender = 0xDDDCDBDAD9CD;
}

// return a pos-expression of given pos in digit
Pos pos2digit(Pos pos)
{
    Pos row = pos >> 4, col = pos & 0xF;
    row = min(convert(row), convert(row + 8));
    col = min(convert(col), convert(col + 8));
    return row << 4 | col;
}

// return a pos-expression of given pos in alphabet
Pos pos2alpha(Pos pos)
{
    Pos row = pos >> 4, col = pos & 0xF;
    row = max(convert(row), convert(row + 8));
    col = max(convert(col), convert(col + 8));
    return row << 4 | col;
}

// return a promoted pos-expression of given pos
Pos pos2promoted(Pos pos)
{
    return pos & 0xF0 | convert(pos & 0xF);
}

// return a formatted pos-expression for inner usage
// 1E -> 15 or 9D
Pos posImport(Pos p, int player)
{
    p = p & 0xF0 | ((Pos)(p & 0xF) - 1);
    return (player == ATTACKER) ? pos2digit(p) : pos2alpha(p);
}

// return a formatted pos-expression for printout
// 15 -> 1E, DC -> 5C
Pos posExport(Pos p)
{
    p = pos2promoted(pos2digit(p));
    return p & 0xF0 | ((Pos)(p & 0xF) + 1);
}

unsigned int hash(char* s) { return *s ? *s + hash(s + 1) : 0; }

// parse the input instruction
void readMove(Move* mp, int player)
{
    char input[6], piece[3];
    int from, to;
    scanf("%s", input);

    mp->player = player;
    if (strlen(input) == 5)
    {
        // movement with promotion
        sscanf(input, "%2X%2X%*c", &from, &to);
        mp->from = posImport(from, player);
        mp->to = posImport(to, player);
        mp->type = -1;
    }
    else if (input[3] - 55 > 0xD)
    {
        // placement of a off-board piece
        sscanf(input, "%2X%s", &to, piece);
        mp->to = posImport(to, player);
        switch (hash(piece))
        {
            case 148: mp->type = 4; break; // KI
            case 144: mp->type = 3; break; // GI
            case 150: mp->type = 2; break; // KK
            case 145: mp->type = 1; break; // HI
            case 155: mp->type = 0; break; // FU
        }
    }
    else
    {
        // movement without promotion
        sscanf(input, "%2X%2X", &from, &to);
        mp->from = posImport(from, player);
        mp->to = posImport(to, player);
        mp->type = -2;
    }
}

// print out the formal instruction of a move
void printMove(Move move)
{
    switch (move.type)
    {
        case -2: printf("%02X%02X\n", posExport(move.from), posExport(move.to)); break;
        case -1: printf("%02X%02XN\n", posExport(move.from), posExport(move.to)); break;
        case 0: printf("%02XFU\n", posExport(move.to)); break;
        case 1: printf("%02XHI\n", posExport(move.to)); break;
        case 2: printf("%02XKK\n", posExport(move.to)); break;
        case 3: printf("%02XGI\n", posExport(move.to)); break;
        case 4: printf("%02XKI\n", posExport(move.to)); break;
    }
}

// return 1 when the given pos is in the board else 0
int isValidPos(Pos pos)
{
    pos = pos2digit(pos);
    Pos col = pos & 0xF;
    if (0x10 < pos && pos < 0x56 && 0x0 < col && col < 0x6) return 1;
    return 0;
}

// return 1 when the piece at the given pos is promoted
int isPromoted(Pos pos)
{
    return ((pos >> 4) < 0x7) ^ ((pos & 0xF) < 0x7);
}

// return 1 when the given move is promotale
int isPromotableMove(Move move)
{
    switch (move.player)
    {
        case ATTACKER: return (move.from >> 4 == 0x5) || (move.to >> 4 == 0x5);
        case DEFENDER: return (move.from >> 4 == 0x9) || (move.to >> 4 == 0x9);
    }
}

// return 1 when from -> to is reachable
int isReachable(Board board, Pos from, Pos to)
{
}

// return 1 when king is checked
int isChecked(Board board, int player)
{
}

// return the detail of certain pos (in-board supposed)
// -1 -> empty / free pos
// 0-5 -> in attacker's line
// 8-D -> in defender's line
int getPos(Board board, Pos pos)
{
    Pos* p =(Pos*)&board;
    pos = pos2digit(pos);

    for (int i = 0; i < 6; i++, p++)
    {
        if (pos == pos2digit(*p)) return i;
        if (pos == pos2digit(*(p + 8))) return i + 8;
    }

    return -1;
}

// return the position of certain piece
// piece: 0-5
// player: attacker or defender
// for example:
// board = 0x111213141521, 0xDDDCDBDAD945; piece = 0(FU); player = 0(ATTACKER)
// return 2145
int getPiece(Board board, int piece, int player)
{
    int res = 0x0;
    Pos* pos = (Pos*)&board;

    // pos > 0x77 -> this pos belongs to attacker
    // attacker? 0 0 1 1
    // player    0 1 0 1
    // xor       0 1 1 0
    // 0 -> not attacker + attacker's turn or attacker + defender's turn
    // 1 -> not attacker + defender's turn or attacker + attacker's turn
    pos += piece;
    if ((*pos < 0x77) ^ player) res = res << 8 | *pos;
    pos += 8;
    if ((*pos < 0x77) ^ player) res = res << 8 | *pos;

    return res;
}

// movable places
// dests: list of possible destinations (abundant length supposed)
// direction: up -> 0x10, down -> -0x10, left -> -0x1, right -> 0x1
//            up-right -> 0x11, up-left -> 0xF, down-left -> -0x11, down-right -> -0xF
// boundless: 1 for true, 0 for false
// return the number of movement

typedef enum direction
{
    UP = 0x10, DOWN = -0x10,
    LEFT = -0x1, RIGHT = 0x1,
    UP_RIGHT = 0x11, UP_LEFT = 0xF,
    DOWN_RIGHT = -0xF, DOWN_LEFT = -0x11
} Direction;

/* untested part
int makeStep(Board board, Pos* dests, Pos from, Direction direction, int boundless)
{
    int counter = 0;
    Pos try = from + direction;

    do
    {
        (isValidPos(try) && getPos(board, try) == -1) ? try = direction + (*dests++ = try) : break;
        // (isValidPos(try) && getPos(board, try) == -1) ? *(dests++) = try : break;
        // try += direction;
    } while (boundless && ++counter);
    
    return counter;
}

// variable promoted is meaningless, just keep unified with other serial functions
int getMovablePosOfKing(Board board, Pos* dests, Pos from, int promoted)
{
    int counter = 0;
    counter += makeStep(board, dests + counter, from, UP, 0);
    counter += makeStep(board, dests + counter, from, DOWN, 0);
    counter += makeStep(board, dests + counter, from, LEFT, 0);
    counter += makeStep(board, dests + counter, from, RIGHT, 0);
    counter += makeStep(board, dests + counter, from, UP_LEFT, 0);
    counter += makeStep(board, dests + counter, from, UP_RIGHT, 0);
    counter += makeStep(board, dests + counter, from, DOWN_LEFT, 0);
    counter += makeStep(board, dests + counter, from, DOWN_RIGHT, 0);
    return counter;
}

// variable promoted is meaningless, just keep unified with other serial functions
int getMovablePosOfGold(Board board, Pos* dests, Pos from, int promoted)
{
    int counter = 0;
    counter += makeStep(board, dests + counter, from, UP, 0);
    counter += makeStep(board, dests + counter, from, DOWN, 0);
    counter += makeStep(board, dests + counter, from, LEFT, 0);
    counter += makeStep(board, dests + counter, from, RIGHT, 0);
    counter += makeStep(board, dests + counter, from, UP_LEFT, 0);
    counter += makeStep(board, dests + counter, from, UP_RIGHT, 0);
    return counter;
}

int getMovablePosOfSilver(Board board, Pos* dests, Pos from, int promoted)
{
    int counter = 0;
    counter += makeStep(board, dests + counter, from, UP, 0);
    if (promoted)
    {
        counter += makeStep(board, dests + counter, from, DOWN, 0);
        counter += makeStep(board, dests + counter, from, LEFT, 0);
        counter += makeStep(board, dests + counter, from, RIGHT, 0);
    }
    counter += makeStep(board, dests + counter, from, UP_LEFT, 0);
    counter += makeStep(board, dests + counter, from, UP_RIGHT, 0);
    if (!promoted)
    {
        counter += makeStep(board, dests + counter, from, DOWN_LEFT, 0);
        counter += makeStep(board, dests + counter, from, DOWN_RIGHT, 0);
    }
    return counter;
}

int getMovablePosOfBishop(Board board, Pos* dests, Pos from, int promoted)
{
    int counter = 0;
    if (promoted)
    {
        counter += makeStep(board, dests + counter, from, UP, 0);
        counter += makeStep(board, dests + counter, from, DOWN, 0);
        counter += makeStep(board, dests + counter, from, LEFT, 0);
        counter += makeStep(board, dests + counter, from, RIGHT, 0);
    }
    // boundless move
    counter += makeStep(board, dests + counter, from, UP_LEFT, 1);
    counter += makeStep(board, dests + counter, from, UP_RIGHT, 1);
    counter += makeStep(board, dests + counter, from, DOWN_LEFT, 1);
    counter += makeStep(board, dests + counter, from, DOWN_RIGHT, 1);
    return counter;
}

int getMovablePosOfRook(Board board, Pos* dests, Pos from, int promoted)
{
    int counter = 0;
    // boundless move
    counter += makeStep(board, dests + counter, from, UP, 1);
    counter += makeStep(board, dests + counter, from, LEFT, 1);
    counter += makeStep(board, dests + counter, from, DOWN, 1);
    counter += makeStep(board, dests + counter, from, RIGHT, 1);
    if (promoted)
    {
        counter += makeStep(board, dests + counter, from, UP_LEFT, 0);
        counter += makeStep(board, dests + counter, from, UP_RIGHT, 0);
        counter += makeStep(board, dests + counter, from, DOWN_LEFT, 0);
        counter += makeStep(board, dests + counter, from, DOWN_RIGHT, 0);
    }
    return counter;
}

int getMovablePosOfPawn(Board board, Pos* dests, Pos from, int promoted)
{
    int counter = 0;
    counter += makeStep(board, dests + counter, from, UP, 0);
    if (promoted)
    {
        counter += makeStep(board, dests + counter, from, DOWN, 0);
        counter += makeStep(board, dests + counter, from, LEFT, 0);
        counter += makeStep(board, dests + counter, from, RIGHT, 0);
        counter += makeStep(board, dests + counter, from, UP_LEFT, 0);
        counter += makeStep(board, dests + counter, from, UP_RIGHT, 0);
    }
    return counter;
}
*/

// return the revised half-board
// piece: 0-5, 8-D
// to: destined postion
void setPos(Board* bp, int piece, Pos to)
{
    *((Pos*)bp + piece) = to;
}

// revise the board in place
void setBoard(Board* bp, Move move)
{
    int piece;
    
    switch (move.type)
    {
        // movement
        case -1:
            move.to = pos2promoted(move.to);
        case -2:
            // take the piece at to if exists
            piece = getPos(*bp, move.to);
            if (piece != -1) setPos(bp, piece, move.player * 0xFF);
            // move the piece at from to to
            piece = getPos(*bp, move.from);
            setPos(bp, piece, move.to);
            break;
        // placement
        default:
            setPos(bp, move.type + move.player * 8, move.to);
            break;
    }
}
