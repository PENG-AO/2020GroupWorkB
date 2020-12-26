#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ATTACKER 0
#define DEFENDER 1
#define MAX_MOVES_LEN 300
#define MAX_TURNS_NUM 150
#define KEY_TABLE_ROW 20
#define KEY_TABLE_COL 27

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
//              ABA3 defender's move, AB -> from, A3 -> to, A3 -> promoted
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
// struct of board
typedef struct board
{
    unsigned long long attacker;
    unsigned long long defender;
} Board;
// a 64bit hashed key for a certain board (on-board state and off board state)
// or a single state basing on Zobrist Hashing
typedef unsigned long long Key;
typedef struct hashtable
{
    Key attacker, defender;
    Key keys[KEY_TABLE_ROW][KEY_TABLE_COL];
} HashTable;
// stuct of history boards and current turn number
// notice of usage: turn = len(past)
// turn % 2 represents the one who is about to make the next move
typedef struct history
{
    int turn;
    Key past[MAX_TURNS_NUM];
} History;

// in order to minimize the number of variables in some function, hashtable was defied globally
HashTable table;

int convert2digit(int p) { return (p < 0x7) ? p : (p - 0x9); }
int convert2alpha(int p) { return (p > 0x7) ? p : (p + 0x9); }
// swich the half-pos expression between digit and alphabet: 1 -> A -> 1
int convert2opposite(int p) { return p + ((p < 0x7) ? 0x9 : -0x9); }

Key genKey(void)
{
    return
        ((Key)rand() << 0x00) & 0x000000000000FFFF |
        ((Key)rand() << 0x10) & 0x00000000FFFF0000 |
        ((Key)rand() << 0x20) & 0x0000FFFF00000000 |
        ((Key)rand() << 0x30) & 0xFFFF000000000000;
}

void initBoard(Board* bp);
void initHashTable(HashTable* table);
void initHistory(History* hist);
MonoBoard monoizeBoard(Board board, int hide);
Key hashBoard(Board board, int player);
Key updateHash(Board board, Key hash, Move move);

Pos pos2digit(Pos pos);
Pos pos2alpha(Pos pos);
Pos pos2promoted(Pos pos);
int pos2idx(Pos pos);
Pos idx2pos(int idx, int player);
Pos posImport(Pos pos, int player);
Pos posExport(Pos pos);

int hashPiece(const char* piece);
Move readMove(Board board, int player);
void printMove(Move move);

int isValidPos(Pos pos);
int isPromoted(Pos pos);
int isPromotableMove(Board board, Move move);
int isChecked(Board board, int player);
int isCheckedMove(Board board, Move move);
int isDecidableMove(Board board, History hist, Move move);
int isRepetitiveMove(Board board, History hist, Move move);
int is4CheckableMove(Board board, History hist, Move move);

int getPlayer(Move move);
int getPos(Board board, Pos pos);
int getPiece(Board board, Piece piece);

MonoBoard makeStep(Board board, Pos pos, int direction);
MonoBoard getMoveMask(Pos pos, Piece piece, int promoted);
MonoBoard getMovableMap(Board board, Pos pos, Piece piece);
MonoBoard getPlacableMap(Board board, History hist, Piece piece, int player);
int getMoveList(Board board, History hist, Move* moves);

void setPos(Board* bp, int place, Pos to);
void setBoard(Board* bp, Move move);

// init board with default layout
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

// init keys for zobrist hashing table
// row index: attacker's pawn - king (0 - 5), promoted pawn - promoted silver (6 - 9)
//            defender's pawn - king (10 - 15), promoted pawn - promoted silver (16 - 19)
// col index: on-board state (0 - 24), off-board state (1 in hand -> 25, 2 in hand -> 26)
void initHashTable(HashTable* table)
{
    Key* k = (Key*)table->keys;
    srand((unsigned int)time(NULL));
    table->attacker = genKey();
    table->defender = genKey();
    for (int i = 0; i < KEY_TABLE_ROW * KEY_TABLE_COL; i++) *(k + i) = genKey();
}

// init history stuct
void initHistory(History* hist)
{
    hist->turn = 0;
    memset(hist->past, 0, sizeof(Key) * MAX_TURNS_NUM);
}

// short for monochromatize board
// hide: 0 -> mark all
//       1 -> hide attacker's piece
//       2 -> hide defender's piece
MonoBoard monoizeBoard(Board board, int hide)
{
    MonoBoard monoboard = 0x0;
    Pos* p = (Pos*)&board;

    for (int i = PAWN; i <= KING; i++, p++)
    {
        if (!(hide && getPlayer(*p) == (hide - 1))) monoboard |= 1 << pos2idx(*p);
        if (!(hide && getPlayer(*(p + 8)) == (hide - 1))) monoboard |= 1 << pos2idx(*(p + 8));
    }

    return monoboard;
}

// return the hashed value of board basing on Zobrist Hashing
Key hashBoard(Board board, int player)
{
    Key hash = (player == ATTACKER) ? table.attacker : table.defender;
    Pos* p = (Pos*)&board;

    for (int i = PAWN; i <= KING; i++, p++)
    {
        // 2 piece off-board
        if (getPiece(board, i) == getPlayer(*p) * 0xFFFF) { hash ^= table.keys[i + getPlayer(*p) * 10][26]; continue; }
        hash ^= table.keys[i + getPlayer(*p) * 10 + isPromoted(*p) * 6][pos2idx(*p)];
        hash ^= table.keys[i + getPlayer(*(p + 8)) * 10 + isPromoted(*(p + 8)) * 6][pos2idx(*(p + 8))];
    }

    return hash;
}

// return the updated hashed value of board after applying move
// board: before applying move
// this function is faster for there is no loop, even though it works the same as applying setBoard first then hashBoard
Key updateHash(Board board, Key hash, Move move)
{
    Pos a = move >> 8, b = move & 0xFF;
    int piece, player = getPlayer(move);

    if (a < KING)
    {
        // unbind the state of 2 off-board piece
        if (getPiece(board, a) == player * 0xFFFF) hash ^= table.keys[(int)a + player * 10][26];
        // bind the state of 1 off-board piece for 2 -> 1
        // unbind the state of 1 off-board piece for 1 -> 0
        hash ^= table.keys[(int)a + player * 10][25];
        // placemnt
        hash ^= table.keys[(int)a + player * 10][pos2idx(b)];
    }
    else
    {
        piece = getPos(board, b);
        // take competitor's piece away
        if (piece != -1)
        {
            // unbind its on-board state
            hash ^= table.keys[piece % 8 + !player * 10 + isPromoted(((Pos*)&board)[piece]) * 6][pos2idx(b)];
            // check the number of player's certain off-board piece
            setPos(&board, piece, player * 0xFF);
            // bind the state of 2 off-board piece
            if (getPiece(board, piece % 8) == player * 0xFFFF) hash ^= table.keys[piece % 8 + player * 10][26];
            // unbind the state of 1 off-board piece for 1 -> 2
            // bind the state of 1 oof-board piece for 0 -> 1
            hash ^= table.keys[piece % 8 + player * 10][25];
        }
        // movement
        piece = getPos(board, a) % 8;
        hash ^= table.keys[piece + player * 10 + isPromoted(a) * 6][pos2idx(a)];
        hash ^= table.keys[piece + player * 10 + isPromoted(b) * 6][pos2idx(b)];
    }

    return hash ^ table.attacker ^ table.defender;
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
    if (pos == getPlayer(pos) * 0xFF) return 25; // off-board (for hashing usage)
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

int hashPiece(const char* piece)
{
    int type = 0;
    while (*piece) type += *piece++;
    switch (type)
    {
        case 155: return PAWN;
        case 145: return ROOK;
        case 150: return BISHOP;
        case 144: return SILVER;
        case 148: return GOLD;
        default: return KING; // meaningless
    }
}

// parse the input instruction
Move readMove(Board board, int player)
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
        from = posImport(from, player); to = posImport(to, player);
        // revise pos if the moved piece is promoted
        if (isPromoted(((Pos*)&board)[getPos(board, from)]))
        {
            from = pos2promoted(from);
            to = pos2promoted(to);
        }
        return from << 8 | to;
    }
}

// print out the formal instruction of a move
void printMove(Move move)
{
    if ((move >> 8) < KING)
    {
        // placement of a off-board piece
        switch (move >> 8)
        {
            case PAWN: printf("%02XFU\n", posExport(move & 0xFF)); break;
            case ROOK: printf("%02XHI\n", posExport(move & 0xFF)); break;
            case BISHOP: printf("%02XKK\n", posExport(move & 0xFF)); break;
            case SILVER: printf("%02XGI\n", posExport(move & 0xFF)); break;
            case GOLD: printf("%02XKI\n", posExport(move & 0xFF)); break;
        }
    }
    else if (isPromoted(move >> 8) ^ isPromoted(move & 0xFF))
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
    // return 0 if the given move is not a movement but a placemet (00 - 04)
    if (move >> 8 <= KING) return 0;
    // return 0 if it is not a promotable piece (king, gold)
    if (getPos(board, move >> 8) % 0x8 > SILVER) return 0;
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
    Pos* p = (Pos*)&board;
    // a monoboard with competitor's reachable pos marked
    MonoBoard dangermap = 0x0;

    for (int i = PAWN; i <= KING; i++, p++)
    {
        for (int j = 0; j < 9; j += 8)
        {
            // skip player's piece
            if (getPlayer(*(p + j)) == player) continue;
            // skip competitor's off-board piece
            if (*(p + j) == !player * 0xFF) continue;
            dangermap |= getMovableMap(board, *(p + j), i);
        }
    }

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

// return 1 when the given move will make competitor's king can not avoid being checked else 0
// competitor of the one who made the given move ↵
// (legal move supposed)
// AKA 詰み
int isDecidableMove(Board board, History hist, Move move)
{
    setBoard(&board, move);
    hist.past[hist.turn] = hashBoard(board, hist.turn % 2); hist.turn++;
    // though it is feasible without this line
    // a pre-check might make this function faster
    // for getting checked is the prerequisite of 詰み
    if (!isChecked(board, hist.turn % 2)) return 0;
    Move moves[MAX_MOVES_LEN];
    return !getMoveList(board, hist, moves);
}

// return 1 when the same board has appeared 4 times after applying the given move else 0
// (legal move supposed)
// AKA 千日手
int isRepetitiveMove(Board board, History hist, Move move)
{
    int counter = 1;
    Key hash;
    setBoard(&board, move);
    hash = hashBoard(board, hist.turn % 2);

    for (int i = 0; i < hist.turn; i++)
    {
        counter += hash == hist.past[i];
        if (counter == 4) return 1;
    }

    return 0;
}

// return 1 when the same checked pattern has consecutively appeared for 4 times else 0
// suppose player as the one who made the given move, this function is for checking
// whether player has used 4 same pattern to make competitor get checked
// (player's turn currently suppposed)
// AKA 連続王手による千日手
int is4CheckableMove(Board board, History hist, Move move)
{
    int player = getPlayer(move);
    Key hash;
    setBoard(&board, move);
    if (!isChecked(board, !player)) return 0;

    hash = hashBoard(board, player);
    for (int i = 2; i <= 6; i += 2)
    {
        if (hash != hist.past[hist.turn - i]) return 0;
    }

    return 1;
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

    for (int i = PAWN; i <= KING; i++, p++)
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
    MonoBoard emptymap = ~monoizeBoard(board, 0), mask = 0x1;
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

    // promoted silver general and pawn here, return move mask of gold general
    if (promoted && piece < GOLD) return getMoveMask(pos, GOLD, 0);

    rshift = (int)(pos2digit(pos) & 0xF) - 3;
    shift = ((int)(pos2digit(pos) >> 4) - 3) * 5 + rshift;
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
    MonoBoard monoboard = ~monoizeBoard(board, !getPlayer(pos) + 1), movablemap = 0x0;
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
MonoBoard getPlacableMap(Board board, History hist, Piece piece, int player)
{
    MonoBoard placablemap = ~monoizeBoard(board, 0);
    // piece except pawn may place wherever empty
    if (piece != PAWN) return placablemap;
    // topmost horizontal line: 0x1F00000, bottommost horizontal line: 0x1F
    // leftmots vertival line: 0x108421
    int pos = getPiece(board, piece), shift;
    if ((pos >> 8) == player * 0xFF) pos &= 0xFF;
    else if ((pos & 0xFF) == player * 0xFF) pos >>= 8;
    // avoid attacker and defender's pawns at the same vertical line (二歩)
    if (getPlayer(pos) == player && !isPromoted(pos) && pos != player * 0xFF)
    {
        shift = (int)(pos & 0xF) - (player == ATTACKER ? 0x1 : 0xA);
        placablemap &= ~(0x108421 << shift);
    }
    // avoid placing pawn at competitor's position (陣地)
    placablemap &= ~(player == ATTACKER ? 0x1F00000 : 0x1F);
    // avoid making decided move by placing a off-board pawn (打ち歩詰め)
    // using function isDecidableMove is feasible even though it seems like there is an endless loop
    // (isDecidedMove⓪ -> isDecided① -> getMoveList② -> getPlacableMap③ -> isDecidedMove④)
    // for example basing on attacker's side, there are 3 typical conditions
    // (A) 00xx or xx00: attacker has a placable pawn (xx -> an on-board pawn whichever it belongs to)
    // (B) 00FF or FF00: both attacker and defender has a placable pawn
    // (C) 0000: attacker has 2 placable pawns
    // in condition (A), getPlacableMap③ will return immediately whenever it was called
    //     for there is not placable pawn any more, as a result no endless loop
    // in condition (B), after placing attacker's pawn, it will turn into xxFF or FFxx which is condition (A) for defender
    // in condition (C), after placing attacker's first pawn, it will turn into 00xx which exactly is condition (A)
    for (int i = 0; i < 25; i++)
    {
        if (!(placablemap & (1 << i))) continue;
        if (isDecidableMove(board, hist, idx2pos(i, player))) placablemap &= ~(1 << i);
    }

    return placablemap;
}

// move list
// moves: list of possible movements (abundant length supposed)
// player: attacker or defender
// return the number of possible movement
int getMoveList(Board board, History hist, Move* moves)
{
    int counter = 0, player = hist.turn % 2;
    Pos* p = (Pos*)&board, pos;
    Move move;
    MonoBoard markedmap;

    for (int i = PAWN; i <= KING; i++, p++)
    {
        for (int j = 0; j < 9; j += 8)
        {
            pos = *(p + j);
            // skip when the pos does not belong to player
            if (getPlayer(pos) != player) continue;

            if (pos == player * 0xFF)
            {
                // placement
                pos = i;
                markedmap = getPlacableMap(board, hist, i, player);
            }
            else
            {
                // movement
                markedmap = getMovableMap(board, pos, i);
            }
            // traverse the marked map
            for (int k = 0; k < 25; k++)
            {
                if (!(markedmap & (1 << k))) continue;
                move = pos << 8 | (isPromoted(pos) ? pos2promoted(idx2pos(k, player)) : idx2pos(k, player));
                // skip when the checked state was unsolved or this move will lead to a checked state
                if (isCheckedMove(board, move)) continue;
                // skip when attacker will make a repetitive move
                if ((player == ATTACKER) && isRepetitiveMove(board, hist, move)) continue;
                // skip when player has using the same check pattern consecutively for 4 times including this move
                if (is4CheckableMove(board, hist, move)) continue;
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
    Pos a = move >> 8, b = move & 0xFF;

    if (a < KING)
    {
        // placement of a off-board piece
        piece = getPiece(*bp, a);
        if ((piece >> 8) == getPlayer(move) * 0xFF) setPos(bp, (int)a + 8, b);
        else if ((piece & 0xFF) == getPlayer(move) * 0xFF) setPos(bp, a, b);
    }
    else 
    {
        // movement
        piece = getPos(*bp, b);
        // take the piece at destination if exists
        if (piece != -1) setPos(bp, piece, getPlayer(move) * 0xFF);
        // move the piece at start pos to destination
        piece = getPos(*bp, a);
        setPos(bp, piece, b);
    }
}

// for debug
void showBit(MonoBoard monoboard)
{
    for (int i = 20; i >= 0; i -= 5)
    {
        for (int j = 0; j < 5; j++) printf("%d", (monoboard >> i >> j) & 1);
        printf("\n");
    }
    printf("\n");
}

// for debug
void showBoard(Board board)
{
    Pos* p =(Pos*)&board;
    printf("歩 飛 角 銀 金 王\n");
    for (int i = 0; i < 9; i += 8) { for (int j = PAWN; j <= KING; j++) printf("%02X ", *(p + i + j)); printf("\n"); }
}
