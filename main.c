#include "simulator.c"

void printPiece(Piece piece)
{
    switch (piece)
    {
        case PAWN: printf("歩"); return;
        case ROOK: printf("飛"); return;
        case BISHOP: printf("角"); return;
        case SILVER: printf("銀"); return;
        case GOLD: printf("金"); return;
        case KING: printf("王"); return;
        default: return;
    }
}

void printBoard(Board board)
{
    int piece;
    Pos pos;
    // off-board piece of defender
    printf("▽ : ");
    for (int i = PAWN; i <= KING; i++)
    {
        piece = getPiece(board, i);
        if ((piece & 0xFF) == 0xFF || (piece >> 8) == 0xFF)
        {
            printPiece(i);
            printf(" ");
        }
    }
    printf("\n");
    // on-board state
    for (int row = 5; row > 0; row--)
    {
        printf("  -------------------------\n");
        printf("%d ", row);
        for (int col = 1; col <= 5; col++)
        {
            pos = row << 4 | col;
            printf("|");

            for (int i = PAWN; i <= KING; i++)
            {
                piece = getPiece(board, i);

                if (pos2digit(piece & 0xFF) == pos)
                {
                    printPiece(i);
                    if (isPromoted(piece & 0xFF))
                    {
                        (getPlayer(piece & 0xFF) == ATTACKER) ? printf("▲ ") : printf("▼ ");
                    }
                    else
                    {
                        (getPlayer(piece & 0xFF) == ATTACKER) ? printf("△ ") : printf("▽ ");
                    }
                    break;
                }
                else if (pos2digit(piece >> 8) == pos)
                {
                    printPiece(i);
                    if (isPromoted(piece >> 8))
                    {
                        (getPlayer(piece >> 8) == ATTACKER) ? printf("▲ ") : printf("▼ ");
                    }
                    else
                    {
                        (getPlayer(piece >> 8) == ATTACKER) ? printf("△ ") : printf("▽ ");
                    }
                    break;
                }

                if (i == KING) printf("    ");
            }

        }
        printf("|\n");
    }
    printf("  -------------------------\n");
    printf("   A    B    C    D    E\n");
	// off-board piece of attacker
    printf("△ : ");
    for (int i = PAWN; i <= KING; i++)
    {
        piece = getPiece(board, i);
        if ((piece & 0xFF) == 0x00 || (piece >> 8) == 0x00)
        {
            printPiece(i);
            printf(" ");
        }
    }
    printf("\n");
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage error: argc = %d\n", argc);
        return 1;
    }

    Board board;
    History hist;
    Move move, moves[MAX_MOVES_LEN];
    Key hash;
    int count = 0, isCpTurn = !strcmp(argv[1], "1");

    initBoard(&board);
    initHistory(&hist);
    initHashTable(&table); // globally defined

    printf("original board:\n");
    hash = hashBoard(board, DEFENDER);
    showBoard(board);
    printBoard(board);
    printf("hash = %016llX\n-----------------------\n", hash);

    srand((unsigned int)time(NULL));

    while (hist.turn < MAX_TURNS_NUM)
    {
        if (isCpTurn)
        {
            printf("Computer's turn:\n");
            count = getMoveList(board, hist, moves);
            if (count)
            {
                move = moves[rand() % count];
                printf("%s's input = ", (hist.turn % 2) ? "DEFENDER" : "ATTACKER");
            }
            else
            {
                printf("you win!\n");
                break;
            }
        }
        else
        {
            printf("player's turn:\n");
            count = getMoveList(board, hist, moves);
            if (!count)
            {
                printf("you lose!\n");
                break;
            }
            for (int i = 0; i < count; i++) printMove(moves[i]);
            printf("%s's input = ", (hist.turn % 2) ? "DEFENDER" : "ATTACKER");
            move = readMove(board, hist.turn % 2);

            for (int i = 0; i < count; i++)
            {
                if (move == moves[i]) break;
                if (i == count - 1)
                {
                    printf("you lose!(illegal move)\n");
                    break;
                }
            }
        }
        hash = updateHash(board, hash, move);
        hist.past[hist.turn] = hash;
        setBoard(&board, move);
        showBoard(board);
        printBoard(board);
        printf("hash = %016llX\n-----------------------\n", hash);

        hist.turn++;
        isCpTurn = 1 - isCpTurn;
    }

    printf("histories:\n");
    for (int i = 0; i < hist.turn; i++) printf("%03d %016llX\n", i, hist.past[i]);

    return 0;
}
