#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATTACKER 0
#define DEFENDER 1
#define MAX_MOVES_LEN 300

// enum of piece type (also equals to it's address offset from &Board)
// offsets of attacker's pieces: 0-6
// offsets of defender's pieces: 8-14
typedef enum piece { PAWN = 0, ROOK, BISHOP, SILVER, GOLD, KING } Piece;

typedef unsigned char Pos;
// type of move
// for example: 2334 attacker's move, 23 -> from, 34 -> to, 34 -> no promotion
//              235B attacker's move, 23 -> from, 5B -> to, 5B -> promoted
//              0021 attacker's placement, 00 -> pawn, 21 -> at
//              ABBC defender's move, AB -> from, BC -> to, BC -> no promotion
//              AB93 defender's move, AB -> from, 93 -> to, 93 -> promoted
//              01CD defender's placement, 01 -> rook, CD -> at
// methods to obtain some informations
//     first 2 digits: move >> 8
//     second 2 digits: move & 0xFF
//     placement: (move >> 8) < 0x5
typedef unsigned short Move;
// short for monochromatic board
// type of compressed board only represents the information of point without it's type and belongings
// for example: compress the beginning pattern of the board as 11111 00001 00000 10000 11111 (25bit)
typedef unsigned int MonoBoard;
typedef struct board
{
    unsigned long long attacker;
    unsigned long long defender;
} Board;

int convert2digit(int p) { return (p < 0x7) ? p : (p - 0x9); }
int convert2alpha(int p) { return (p > 0x7) ? p : (p + 0x9); }
// swich the half-pos expression between digit and alphabet: 1 -> A -> 1
int convert2opposite(int p) { return p + ((p < 0x7) ? 0x9 : -0x9); }

void initBoard(Board* board);
MonoBoard monoizedBoard(Board board, int hide);

Pos pos2digit(Pos pos);
Pos pos2alpha(Pos pos);
Pos pos2promoted(Pos pos);
int pos2idx(Pos pos);
Pos idx2pos(int idx, int player);
Pos posImport(Pos pos, int player);
Pos posExport(Pos pos);

int hashPiece(char* piece);
Move readMove(int player);
void printMove(Move move);

int isValidPos(Pos pos);
int isPromoted(Pos pos);
int isPromotableMove(Board board, Move move);
int isChecked(Board board, int player);
int isCheckedMove(Board board, Move move);
int isDecided(Board board, int player);
int isDecidedMove(Board board, Move move);

int getPlayer(Move move);
int getPos(Board board, Pos pos);
int getPiece(Board board, Piece piece);

MonoBoard makeStep(Board board, Pos pos, int direction);
MonoBoard getMoveMask(Pos pos, Piece piece, int promoted);
MonoBoard getMovableMap(Board board, Pos pos, Piece piece);
MonoBoard getPlacableMap(Board board, Piece piece, int player);
MonoBoard getDangerMap(Board board, int player);
int getMoveList(Board board, Move* moves, int player);

void setPos(Board* bp, int place, Pos to);
void setBoard(Board* bp, Move move);

void initBoard(Board* bp)
{
    //    PAWN ROOK BISHOP SILVER GOLD KING
    // bp +0   +1   +2     +3     +4   +5
    //    21   15   14     13     12   11 (belongs to attacker at first)
    // bp +8   +9   +10    +11    +12  +13
    //    DE   EA   EB     EC     ED   EE (belongs to defender at first)
    bp->attacker = 0x111213141521;
    bp->defender = 0xEEEDECEBEADE;
}

// short for monochromatize board
// hide: 0 -> mark all
//       1 -> hide attacker's piece
//       2 -> hide defender's piece
MonoBoard monoizedBoard(Board board, int hide)
{
    MonoBoard monoboard = 0x0;
    Pos* p = (Pos*)&board;

    for (int i = 0; i < 6; i++, p++)
    {
        if (!(hide && getPlayer(*p) == (hide - 1))) monoboard |= 1 << pos2idx(*p);
        if (!(hide && getPlayer(*(p + 8)) == (hide - 1))) monoboard |= 1 << pos2idx(*(p + 8));
    }

    return monoboard;
}

// return a pos-expression of given pos in digit
Pos pos2digit(Pos pos) { return convert2digit(pos >> 4) << 4 | convert2digit(pos & 0xF); }

// return a pos-expression of given pos in alphabet
Pos pos2alpha(Pos pos) { return convert2alpha(pos >> 4) << 4 | convert2alpha(pos & 0xF); }

// return a promoted pos-expression of given pos
Pos pos2promoted(Pos pos) { return (pos & 0xF0) | convert2opposite(pos & 0xF); }

// return the index of a given pos on the board
// 20 21 22 23 24
// 15 16 17 18 19
// 10 11 12 13 14
// 05 06 07 08 09
// 00 01 02 03 04
int pos2idx(Pos pos)
{
    pos = pos2digit(pos);
    return (int)(pos >> 4) * 5 + (int)(pos & 0xF) - 6;
}

// the inverse function of pos2idx
Pos idx2pos(int idx, int player)
{
    Pos row = convert2opposite(idx / 5) + 1, col = convert2opposite(idx % 5) + 1;
    return convert2opposite(row + player * 9) << 4 | convert2opposite(col + player * 9);
}

// return a formatted pos-expression for inner usage
// 1E -> 15 or AE
Pos posImport(Pos pos, int player) { return (player == ATTACKER) ? pos2digit(pos) : pos2alpha(pos); }

// return a formatted pos-expression for printout
// 15 -> 1E, EC -> 5C
Pos posExport(Pos pos) { return pos2promoted(pos2digit(pos)); }

int hashPiece(char* piece)
{
    int type = 0;
    while (*piece) type += *piece++;
    switch (type)
    {
        case 155: return 0; // pawn
        case 145: return 1; // rook
        case 150: return 2; // bishop
        case 144: return 3; // silver general
        case 148: return 4; // gold general
        default: return 5; // king (meaningless)
    }
}

// parse the input instruction
Move readMove(int player)
{
    char input[6], piece[3];
    int from, to;
    scanf("%s", input);

    if (strlen(input) == 5)
    {
        // movement with promotion
        sscanf(input, "%2X%2X%*c", &from, &to);
        return posImport(from, player) << 8 | pos2promoted(posImport(to, player));
    }
    else if (input[3] > 'E') 
    {
        // placement of a off-board piece
        sscanf(input, "%2X%s", &to, piece);
        return (Pos)hashPiece(piece) << 8 | posImport(to, player);
    }
    else
    {
        // movement without promotion
        sscanf(input, "%2X%2X", &from, &to);
        return posImport(from, player) << 8 | posImport(to, player);
    }
}

// print out the formal instruction of a move
void printMove(Move move)
{
    if ((move >> 8) < 0x5)
    {
        // placement of a off-board piece
        switch (move >> 8)
        {
            case 0: printf("%02XFU\n", posExport(move & 0xFF)); break;
            case 1: printf("%02XHI\n", posExport(move & 0xFF)); break;
            case 2: printf("%02XKK\n", posExport(move & 0xFF)); break;
            case 3: printf("%02XGI\n", posExport(move & 0xFF)); break;
            case 4: printf("%02XKI\n", posExport(move & 0xFF)); break;
        }
    }
    else if (isPromoted(move & 0xFF))
    {
        // movement with promotion
        printf("%02X%02XN\n", posExport(move >> 8), posExport(move & 0xFF));
    }
    else
    {
        // movement without promotion
        printf("%02X%02X\n", posExport(move >> 8), posExport(move & 0xFF));
    }
}

// return 1 when the given pos is in the board else 0
int isValidPos(Pos pos)
{
    pos = pos2digit(pos);
    Pos col = pos & 0xF;
    return (0x10 < pos) && (pos < 0x56) && (0x0 < col) && (col < 0x6);
}

// return 1 when the piece at the given pos is promoted else 0
int isPromoted(Pos pos) { return ((pos >> 4) < 0x7) ^ ((pos & 0xF) < 0x7); }

// return 1 when the given move is promotale basing on it's piece type else 0
int isPromotableMove(Board board, Move move)
{
    // for placement, getPos will return -1 because there is no piece at 00 01 02 03 04
    // -1 % 0x8 = 7 > 3, thus this function will not recognize placement as a promotable move
    if (getPos(board, move >> 8) % 0x8 > 3) return 0;
    // return 0 if the given move is already promoted
    if (isPromoted(move >> 8)) return 0;
    if (getPlayer(move) == ATTACKER)
    {
        return (move >> 12 == 0x5) || ((move & 0xF0) >> 4 == 0x5);
    }
    else
    {
        return (move >> 12 == 0xA) || ((move & 0xF0) >> 4 == 0xA);
    }
}

// return 1 when player's king has got checked (king might be taken next) else 0
// AKA 王手
int isChecked(Board board, int player)
{
    Pos king = (getPiece(board, KING) >> (player == ATTACKER ? 0 : 8)) & 0xFF;
    MonoBoard dangermap = getDangerMap(board, player);
    return !!(dangermap & (1 << pos2idx(king)));
}

// return 1 when the given move will make player's king get checked else 0
//        the one who made the given move ↵
// (legal move supposed)
int isCheckedMove(Board board, Move move)
{
    setBoard(&board, move);
    return isChecked(board, getPlayer(move));
}

// return 1 when competitor's king can not avoid being checked by any move else 0
// AKA 詰み
int isDecided(Board board, int player)
{
    Move moves[MAX_MOVES_LEN];
    return !getMoveList(board, moves, !player);
}

// return 1 when the given move will make competitor's king can not avoid being checked else 0
// competitor of the one who made the given move ↵
// (legal move supposed)
int isDecidedMove(Board board, Move move)
{
    setBoard(&board, move);
    return isDecided(board, getPlayer(move));
}

// return the ownership of the given move or pos
int getPlayer(Move move) {return (move & 0xFF) > 0x77; }

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
// piece: values in 0-5 representing different type of pieces
// player: attacker or defender
// for example:
// board = 0x111213141521, 0xEEEDECEB00DE45
// return 4521 when piece = 0(pawn)
// return 0014 when piece = 2(bishop)
// return EE11 when piece = 5(king)
int getPiece(Board board, Piece piece)
{
    Pos* p = (Pos*)&board + piece;
    return *(p + 8) << 8 | *p;
}

// rshift + 2
// 11100 11110 11111 01111 00111
MonoBoard helpermask[5] = {0x739CE7, 0xF7BDEF, 0x1FFFFFF, 0x1EF7BDE, 0x1CE739C};
// facing up (for attacker)
// pawn  rook  bishop silver gold   king
// 00000 ----- -----  00000  00000  00000
// 00100 ----- -----  01110  01110  01110
// 00000 ----- -----  00000  01010  01010
// 00000 ----- -----  01010  00100  01110
// 00000 ----- -----  00000  00000  00000
// facing down (for defender)
// pawn  rook  bishop silver gold   king
// 00000 ----- -----  00000  00000  00000
// 00000 ----- -----  01010  00100  01110
// 00000 ----- -----  00000  01010  01010
// 00100 ----- -----  01110  01110  01110
// 00000 ----- -----  00000  00000  00000
// masks of rook and bishop are useless, for normal shift is not suitable
MonoBoard piecemask[16] = {
    0x20000, 0x0, 0x0, 0x70140, 0x72880, 0x729C0, 0x0, 0x0,
    0x00080, 0x0, 0x0, 0x501C0, 0x229C0, 0x729C0, 0x0, 0x0
};
// list of directional offsets
// ↑: 0x10, ↓: -0x10, ←: -0x1, →: 0x1, ↗︎: 0x11, ↖︎: 0xF, ↘︎: -0xF, ↙︎: -0x11
// directions[0: 4] for rook, directions[4: 8] for bishop
int directions[8] = {0x10, -0x10, -0x1, 0x1, 0x11, 0xF, -0xF, -0x11};

// return a marked movable line on the given direction
// special treatment for rook and bishop for their movements will probably cross other pieces sometimes
MonoBoard makeStep(Board board, Pos pos, int direction)
{
    MonoBoard emptymap = ~monoizedBoard(board, 0), mask = 0x1;
    pos += direction;
    // out of board
    if (!isValidPos(pos)) return 0x0;
    mask <<= pos2idx(pos);
    // free pos
    if (emptymap & mask) return mask | makeStep(board, pos, direction);
    // takable competitor's piece
    if (getPlayer(pos) != getPlayer(((Pos*)&board)[getPos(board, pos)])) return mask;
    // own side piece
    return 0x0;
}

// movable mask
// not feasible for rook and bishop
MonoBoard getMoveMask(Pos pos, Piece piece, int promoted)
{
    // move -2 ≤ m ≤ 2 lines upwards(+) or downwards(-): << m * 5
    // move -2 ≤ n ≤ 2 lines right(+) or left(-): << n then & helpermask[n + 2]
    // shift: total shift = m + n
    // rshift = n
    int shift, rshift;
    MonoBoard mask = 0x0;
    pos = pos2digit(pos);

    // promoted silver general and pawn here, return move mask of gold general
    if (promoted && piece < 4) return getMoveMask(pos, GOLD, 0);

    rshift = (int)(pos & 0xF) - 3;
    shift = ((int)(pos >> 4) - 3) * 5 + rshift;
    if (shift < 0)
    {
        mask |= piecemask[piece + getPlayer(pos) * 8] >> -shift;
    }
    else
    {
        mask |= piecemask[piece + getPlayer(pos) * 8] << shift;
    }
    // remove dislocations
    mask &= helpermask[rshift + 2];
    return mask;
}

// movable map
// return a monoboard with movable pos marked
MonoBoard getMovableMap(Board board, Pos pos, Piece piece)
{
    // hide competitor's piece in order to show the pos that might take competitor's piece
    MonoBoard monoboard = ~monoizedBoard(board, !getPlayer(pos) + 1), movablemap = 0x0;
    int promoted = isPromoted(pos);

    if (piece == ROOK)
    {
        for (int i = 0; i < 4; i++) movablemap |= makeStep(board, pos, directions[i]);
        if (promoted) movablemap |= (monoboard & getMoveMask(pos, KING, 0));
    }
    else if (piece == BISHOP)
    {
        for (int i = 4; i < 8; i++) movablemap |= makeStep(board, pos, directions[i]);
        if (promoted) movablemap |= (monoboard & getMoveMask(pos, KING, 0));
    }
    else
    {
        movablemap = monoboard & getMoveMask(pos, piece, promoted);
    }
    
    return movablemap;
}

// placable map
// illegal placement handled here
// return a monoboard with placable pos marked
MonoBoard getPlacableMap(Board board, Piece piece, int player)
{
    MonoBoard placablemap = ~monoizedBoard(board, 0);
    // piece except pawn may place wherever empty
    if (piece != PAWN) return placablemap;
    // topmost horizontal line: 0x1F00000, bottommost horizontal line: 0x1F
    // leftmots vertival line: 0x108421
    int pos = getPiece(board, piece), shift;
    if ((pos >> 8) == player * 0xFF) pos &= 0xFF;
    else if ((pos & 0xFF) == player * 0xFF) pos >>= 8;
    // avoid attacker and defender's pawns at the same vertical line (二歩)
    if (getPlayer(pos) != player && !isPromoted(pos))
    {
        shift = (int)(pos & 0xF) - (player == ATTACKER ? 0xA : 0x1);
        placablemap &= ~(0x108421 << shift);
    }
    // avoid placing pawn at competitor's position (陣地)
    placablemap &= ~(player == ATTACKER ? 0x1F00000 : 0x1F);
    // avoid making decided move by placing a off-board pawn (打ち歩詰め)
    /* codes here */
    return placablemap;
}

// danger map
// return a monoboard with pos which is reachable for competitor marked
MonoBoard getDangerMap(Board board, int player)
{
    Pos* p = (Pos*)&board;
    MonoBoard dangermap = 0x0;

    for (int i = 0; i < 6; i++, p++)
    {
        for (int j = 0; j < 9; j += 8)
        {
            if (getPlayer(*(p + j)) == player) continue;
            if (*(p + j) == !player * 0xFF) continue;
            dangermap |= getMovableMap(board, *(p + j), i);
        }
    }

    return dangermap;
}

// move list
// moves: list of possible movements (abundant length supposed)
// player: attacker or defender
// return the number of possible movement
int getMoveList(Board board, Move* moves, int player)
{
    int counter = 0;
    Pos* p = (Pos*)&board, pos;
    Move move;
    MonoBoard markedmap;

    for (int i = 0; i < 6; i++, p++)
    {
        for (int j = 0; j < 9; j += 8)
        {
            pos = *(p + j);
            if (getPlayer(pos) != player) continue;

            if (pos == player * 0xFF)
            {
                // placement
                pos = i;
                markedmap = getPlacableMap(board, i, player);
            }
            else
            {
                // movement
                markedmap = getMovableMap(board, pos, i);
            }
            // traverse the marked map
            for (int k = 0; k < 25; k++)
            {
                if (!((markedmap >> k) & 1)) continue;
                move = pos << 8 | idx2pos(k, player);
                if (isCheckedMove(board, move)) continue;
                // skip move if it's pawn's promotable move
                if (!(isPromotableMove(board, move) && i == PAWN))
                {
                    *(moves + counter++) = move;
                }
                // add promotable move additionally
                if (isPromotableMove(board, move))
                {
                    *(moves + counter++) = pos << 8 | pos2promoted(move & 0xFF);
                }
            }
        }
    }

    return counter;
}

// revise the board in place
// place: values in 0-5, 8-D representing exact data place
// to: destined postion
void setPos(Board* bp, int place, Pos to) { *((Pos*)bp + place) = to; }

// revise the board in place (legal move supposed)
void setBoard(Board* bp, Move move)
{
    int piece;

    if ((move >> 8) < 0x5)
    {
        // placement of a off-board piece
        piece = getPiece(*bp, move >> 8);
        if ((piece >> 8) == getPlayer(move) * 0xFF) setPos(bp, (int)(move >> 8) + 8, move & 0xFF);
        else if ((piece & 0xFF) == getPlayer(move) * 0xFF) setPos(bp, move >> 8, move & 0xFF);
    }
    else 
    {
        // movement
        // take the piece at destination if exits
        piece = getPos(*bp, move & 0xFF);
        if (piece != -1) setPos(bp, piece, getPlayer(move) * 0xFF);
        // move the piece at start pos to destination
        piece = getPos(*bp, move >> 8);
        setPos(bp, piece, move & 0xFF);
    }
}
