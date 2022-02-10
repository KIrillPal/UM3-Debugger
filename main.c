#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "code.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define HASH_MOD 10007

const int MAX_STATE_LENGTH    = 1 << 20;
const int MAX_DISPLAYING_ROWS = 1000;

struct CodeRow {
    int cmd_ptr;
    int cmd_key;
    int * values;
};

struct State {
    char error[70];
    struct CodeRow * rows;
    int * col_sizes;
    int capacity;
    int length;

    unsigned int rowHashes[HASH_MOD];
};

struct Code* runLoad()
{
    char file_name[1024];
    printf("Select file: ");

    char* status = fgets(file_name, 1024, stdin);

    file_name[strlen(file_name) - 1] = '\0';

    if (status == NULL)
        return NULL;

    struct Code* code = loadFromFile(file_name);

    if (!code)
        return NULL;
    
    return code;
}

void printLine(unsigned char* s, int length)
{
    while (length-- > 0)
        printf(s);
}

void printTableBound(int ncols, int * col_sizes, unsigned char* s_start, unsigned char* s_mid, unsigned char* s_end, unsigned char* s_bnd)
{
    printf(s_start);
    int i, j;
    for (i = 0; i < ncols; ++i)
    {
        for (j = 0; j < col_sizes[i]; ++j)
            printf(s_bnd);
        if (i + 1 != ncols)
            printf(s_mid);
    }
    printf(s_end);
    printf("\n");
}

int getlen(int number)
{
    int len = number <= 0;
    while (number)
    {
        ++len;
        number /= 10;
    }
    return len;
}

int * findCell(struct Code * code, unsigned int ptr)
{
    int i;
    for (i = 0; i < code->mem_cnt; ++i)
    {
        if (code->mem_ptrs[i] == ptr)
            return code->mem_vals + i;
    }
    return NULL;
}

int findCommandKey(struct Code * code, int ptr)
{
    int i, cmd_ptr = code->mem_start;
    for (i = 0; i < code->length; ++i)
    {
        if (cmd_ptr == ptr)
            return i;

        cmd_ptr += code->rows[i].word_length;
    }

    if (cmd_ptr == ptr)
        return -2;

    return -1;
}

int runCommand(struct Code * code, char * err_str, int * cmd_i, int * cmd_ptr)
{
    struct Command * cmd = code->rows + *cmd_i;

    int * arg1_v = NULL;
    int * arg2_v = NULL;
    int * arg3_v = NULL;

    if (cmd->key != 0x99 && (cmd->key < 0x80 || cmd->key >= 0x96))
    {
        arg3_v = findCell(code, cmd->arg3);
        if (!arg3_v)
        {
            sprintf(err_str, "\x1b[38;2;205;49;49mundefined cell 0x%04X\033[0m", cmd->arg3);
            return -1;
        }
    }

    if (cmd->key != 0x99 && cmd->key != 0x80)
    {
        arg1_v = findCell(code, cmd->arg1);
        if (!arg1_v)
        {
            sprintf(err_str, "\x1b[38;2;205;49;49mundefined cell 0x%04X\033[0m", cmd->arg1);
            return -1;
        }
    }

    if (cmd->key != 0x99 && cmd->key != 0x80 && cmd->key != 0x00)
    {
        arg2_v = findCell(code, cmd->arg2);
        if (!arg2_v)
        {
            sprintf(err_str, "\x1b[38;2;205;49;49mundefined cell 0x%04X\033[0m", cmd->arg2);
            return -1;
        }
    }

    switch (cmd->key)
    {
        case 0x99:
        {
            sprintf(err_str, "\x1b[38;2;44;124;237msuccessfully finished\033[0m");
            return 1;
        }
        case 0x00:
        {
            *arg3_v = *arg1_v;
            (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x01:
        {
            *arg3_v = *arg1_v + *arg2_v;
            (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x02:
        {
            *arg3_v = *arg1_v - *arg2_v;
            (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x03:
        case 0x13:
        {
            *arg3_v = *arg1_v * *arg2_v;
            (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x04:
        case 0x14:
        {
            int * arg4_v = findCell(code, cmd->arg3 + 1);

            if (*arg2_v == 0)
            {
                sprintf(err_str, "\x1b[38;2;205;49;49mtrying to divide by zero\033[0m");
                return -1;
            }

            int del =  *arg1_v / *arg2_v;
            int mod =  *arg1_v - *arg2_v * del;

            *arg3_v = del;
            if (arg4_v)
                *arg4_v = mod;
            
            (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x80:
        {
            (*cmd_ptr) = cmd->arg3;
            break;
        }
        case 0x81:
        {
            if (*arg1_v == *arg2_v)
                (*cmd_ptr) = cmd->arg3;
            else (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x82:
        {
            if (*arg1_v != *arg2_v)
                (*cmd_ptr) = cmd->arg3;
            else (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x83:
        case 0x93:
        {
            if (*arg1_v < *arg2_v)
                (*cmd_ptr) = cmd->arg3;
            else (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x84:
        case 0x94:
        {
            if (*arg1_v >= *arg2_v)
                (*cmd_ptr) = cmd->arg3;
            else (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x85:
        case 0x95:
        {
            if (*arg1_v > *arg2_v)
                (*cmd_ptr) = cmd->arg3;
            else (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
        case 0x86:
        case 0x96:
        {
            if (*arg1_v <= *arg2_v)
                (*cmd_ptr) = cmd->arg3;
            else (*cmd_ptr) += code->rows[*cmd_i].word_length;
            break;
        }
    }

    *cmd_i = findCommandKey(code, *cmd_ptr);
    if (*cmd_i == -1)
    {
        sprintf(err_str, "\x1b[38;2;205;49;49mno command at cell 0x%04X\033[0m", *cmd_ptr);
        return -1;
    }
    else if (*cmd_i == -2)
    {
        sprintf(err_str, "\x1b[38;2;205;49;49mforced to terminate at 0x%04X\033[0m", *cmd_ptr);
        return -1;
    }

    return 0;
}

void drawCode(struct Code * code, struct State st, int is_full, int active_row, int is_err)
{
    const int HINT_SIZE = 7;
    char hint_array[7][70] = {
        "Use keyboard to:",
        "(1) <enter> - move forward",
        "(2) <backspace> move back",
        "(3) 'v' - to change view modes (default/full)",
        "(4) 'r' - to execute the whole program without stopping",
        "(5) 't' - to reset the program",
        "(6) 'e' - to exit the program"
    };

    system("clear");

    if (!is_full)
    {
        printf("\x1b[38;2;250;180;25mProgram:\033[1;97m\n");

        int i;
        printf("\n\x1b[38;2;250;180;25m");
        for (i = 0; i < code->length; ++i)
        {
            if (st.rows[active_row].cmd_key == i)
                printf("\033[1;97m");
            printf("%02X %04X %04X %04X", 
                code->rows[i].key,
                code->rows[i].arg1,
                code->rows[i].arg2,
                code->rows[i].arg3, stdout
            );

            if (i < HINT_SIZE)
                printf("\033[0m           %s\n\033[1;97m\x1b[38;2;250;180;25m", hint_array[i]);
            else printf("\n");

            if (st.rows[active_row].cmd_key == i)
                printf("\x1b[38;2;250;180;25m");
        }

        for (i = code->length; i < HINT_SIZE; ++i)
        {
            printf("\033[0m                            %s\n\033[1;97m\x1b[38;2;250;180;25m", hint_array[i]);
        }

        printf("\033[0m");
    }

    printf("\nDebugging: ");

    if (active_row + 1 == st.length)
        printf("%s", st.error);
    printf("\n\n");


    printTableBound(code->mem_cnt + 1, st.col_sizes, "╔", "╦", "╗", "═");
    printf("║ \033[1;97mCommand\033[0m");
    printLine(" ", st.col_sizes[0] - 9);

    int mem_i;
    for (mem_i = 0; mem_i < code->mem_cnt; ++mem_i)
    {
        printf(" ║ 0x%04X", code->mem_ptrs[mem_i]);
        printLine(" ", st.col_sizes[mem_i + 1] - 8);
    }
    printf(" ║\n");

    int row_i;

    static int last_bound = 0;

    if (active_row < last_bound)
        last_bound = active_row;
    else if (active_row >= last_bound + 10)
        last_bound = active_row - 9;

    int min_row = is_full ? 0 : MAX(last_bound, 0);
    int max_row = is_full ? st.length : MAX(10, MIN(st.length, last_bound + 10));

    for (row_i = min_row; row_i < max_row; row_i++)
    {
        printTableBound(code->mem_cnt + 1, st.col_sizes, "╠", "╬", "╣", "═");

        if (row_i < st.length)
        {
            printf("║ 0x%04X : ", st.rows[row_i].cmd_ptr);
            printCommand(code->rows[st.rows[row_i].cmd_key], stdout);
            printLine(" ", st.col_sizes[0] - 28);
            
            for (mem_i = 0; mem_i < code->mem_cnt; ++mem_i)
            {
                printf(" ║ ");
                int len = getlen(st.rows[row_i].values[mem_i]);

                if (row_i > 0 && st.rows[row_i].values[mem_i] != st.rows[row_i - 1].values[mem_i])
                     printf("\x1b[38;2;50;237;44m");
                else printf("\033[1;97m");


                printf("%d\033[0m", st.rows[row_i].values[mem_i]);
                printLine(" ", st.col_sizes[mem_i + 1] - len - 2);
            }
            printf(" ║");
            
            if (row_i == active_row)
                printf(" <-");
            if (is_err && row_i + 1== st.length)
                printf("\x1b[38;2;205;49;49m Error!\033[0m");
            printf("\n");
        }
        else printTableBound(code->mem_cnt + 1, st.col_sizes, "║", "║", "║", " ");
    }

    printTableBound(code->mem_cnt + 1, st.col_sizes, "╚", "╩", "╝", "═");
}

unsigned int getHash(struct CodeRow * row, int vals)
{
    unsigned int hash = (row->cmd_ptr * 997) % HASH_MOD;
    for (int i = 0; i < vals; ++i)
    {
        hash = (hash * 997 + 10 * row->values[i]) % HASH_MOD;
    }

    return hash % HASH_MOD;
}

int stateAddRow(struct State * st, struct Code * code, int cmd_key, int cmd_ptr)
{
    if (st->capacity == st->length)
    {
        st->capacity = (st->capacity == 0) ? code->length : st->capacity * 2;
        st->rows = (struct CodeRow *)realloc(st->rows, sizeof(struct CodeRow) * st->capacity);
    }
    st->rows[st->length].cmd_key = cmd_key;
    st->rows[st->length].cmd_ptr = cmd_ptr;

    st->rows[st->length].values = (int*) malloc(sizeof(int) * code->mem_cnt);
    memcpy(st->rows[st->length].values, code->mem_vals, sizeof(int) * code->mem_cnt);

    unsigned int h = getHash(&st->rows[st->length], code->mem_cnt);

    st->length++;

    if (st->length >= MAX_STATE_LENGTH)
    {
        sprintf(st->error, "\x1b[38;2;205;49;49mstopped after %d'th row\033[0m", st->length);
        return -1;
    }

    if (st->rowHashes[h])
    {
        int is_eq = st->rows[st->rowHashes[h] - 1].cmd_ptr == st->rows[st->length - 1].cmd_ptr;
        for (int i = 0; i < code->mem_cnt; ++i)
        {
            is_eq &= (st->rows[st->rowHashes[h] - 1].values[i] == st->rows[st->length - 1].values[i]);
        }

        if (is_eq)
        {
            sprintf(st->error, "\x1b[38;2;205;49;49minfinite loop found : rows [0x%04X - 0x%04X]\033[0m", 
            st->rows[st->rowHashes[h] - 1].cmd_ptr, 
            st->rows[st->length - 1].cmd_ptr
            );

            st->rowHashes[h] = st->length;
            return -1;
        }
    }

    st->rowHashes[h] = st->length;
    return 0;
}

int getKey()
{
    struct termios oldt,
    newt;
    int ch;
    tcgetattr( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}

int waitCommand()
{
    while (1)
    {
        int cmd_key = getKey();
        if (cmd_key == 10)
            return 1;
        if (cmd_key == 101 || cmd_key == 69)
            return 2;
        if (cmd_key == 127)
            return 3;
        if (cmd_key == 118 || cmd_key == 86)
            return 4;
        if (cmd_key == 114 || cmd_key == 82)
            return 5;
        if (cmd_key == 116 || cmd_key == 84)
            return 6;
    }

    return 0;
}

int runCode(struct Code * code)
{
    struct State st;
    st.length   = 0;
    st.capacity = 0;
    st.rows  = NULL;
    st.error[0] = '\0';

    memset(st.rowHashes, 0, sizeof(unsigned int) * HASH_MOD);
    
    int active_row  = -1;
    int is_full     = 0;
    int is_finished = 0;
    int is_running  = 0;

    st.col_sizes = (int*) malloc(sizeof(int ) * code->mem_cnt + 1);


    st.col_sizes[0] = 30;

    int i;
    for (i = 0; i < code->mem_cnt; ++i)
    {
        st.col_sizes[i + 1] = 8;

        if (code->mem_vals[i] == INPUT_FLAG)
        {
            printf("Input to 0x%04X: ", code->mem_ptrs[i]);
            scanf("%d", code->mem_vals + i);
        }
    }

    int cmd_key = 0, cmd_ptr = code->mem_start;

    stateAddRow(&st, code, cmd_key, cmd_ptr);

    while (1)
    {
        int cmd_code = is_running ? 1 : waitCommand();
        if (cmd_code != 1)
        {
            if (cmd_code == 2)
            {
                strcpy(st.error, "\x1b[38;2;205;49;49mstopped\033[0m");
                drawCode(code, st, is_full, active_row, 0);
                printf("exit\n");

                for (i = 0; i < st.length; ++i)
                    free(st.rows[i].values);
                free(st.rows);
                free(st.col_sizes);
                return 0;
            }
            if (cmd_code == 3)
            {
                if (active_row > 0)
                    active_row--;
            }
            if (cmd_code == 4)
            {
                if (!is_full && st.length > MAX_DISPLAYING_ROWS)
                {
                    printf("Are you sure to show %d rows? Press <y> or <n>\n", st.length);
                    int ans = getKey();

                    if (ans == 'y' || ans == 'Y')
                        is_full = 1;
                }
                else is_full = !is_full;
            }
            if (cmd_code == 5 && !is_finished)
            {
                is_running = 1;
                drawCode(code, st, is_full, active_row, is_finished == -1);
                printf("   running...\n");
            }
            if (cmd_code == 6)
            {
                printf("Are you sure to reset? Press <y> or <n>\n");
                int ans = getKey();

                if (ans == 'y' || ans == 'Y')
                {
                    for (i = 0; i < st.length; ++i)
                        free(st.rows[i].values);
                    free(st.rows);
                    free(st.col_sizes);
                    system("clear");
                    return 1;
                }
            }
        }
        else if (!is_finished || active_row + 1 < st.length)
            active_row++;

        if (!is_finished && active_row == st.length)
        {
            if (is_finished = runCommand(code, st.error, &cmd_key, &cmd_ptr))
            {
                is_running  = 0;
                active_row = st.length - 1;
            }
            else
            {
                if(is_finished = stateAddRow(&st, code, cmd_key, cmd_ptr))
                {
                    is_running = 0;
                }

                int mem_i;
                for (mem_i = 0; mem_i < code->mem_cnt; ++mem_i)
                {
                    if (getlen(st.rows[active_row].values[mem_i]) + 2 > st.col_sizes[mem_i + 1])
                    {
                        st.col_sizes[mem_i + 1] = getlen(st.rows[active_row].values[mem_i]) + 2;
                    }
                }
            }
        }

        if (!is_running)
            drawCode(code, st, is_full, active_row, is_finished == -1);

    }

    for (i = 0; i < st.length; ++i)
        free(st.rows[i].values);
    free(st.rows);
    free(st.col_sizes);
    return 0;
}

int codeCpy(struct Code * code, struct Code * dest)
{
    dest->capacity  = code->capacity;
    dest->length    = code->length;

    dest->mem_start = code->mem_start;
    dest->mem_cnt   = code->mem_cnt;
    dest->mem_cap   = code->mem_cap;

    dest->rows   = (struct Command*)malloc(sizeof(struct Command) * dest->capacity);
    dest->mem_ptrs = (unsigned int*)malloc(sizeof(unsigned int) * dest->mem_cap);
    dest->mem_vals =          (int*)malloc(sizeof(         int) * dest->mem_cap);

    if (!dest->rows || !dest->mem_ptrs || !dest->mem_vals)
        return -1;

    void* err = memcpy(dest->rows, code->rows, sizeof(struct Command) * dest->length);
    if (err != dest->rows) 
        return -1;

    err = memcpy(dest->mem_ptrs, code->mem_ptrs, sizeof(unsigned int) * dest->mem_cnt);
    if (err != dest->mem_ptrs) 
        return -1;

    err = memcpy(dest->mem_vals, code->mem_vals, sizeof(         int) * dest->mem_cnt);
    if (err != dest->mem_vals) 
        return -1;

    return 0;
}

int main()
{
    struct Code* loaded_code = runLoad();

    if (!loaded_code)
        return 0;

    printf("Successfully loaded\n\n");

    struct Code active_code;

    if (codeCpy(loaded_code, &active_code))
        return 0;

    while (runCode(&active_code))
    {
        if (codeCpy(loaded_code, &active_code))
        return 0;
    }

    codeDtor(loaded_code);
    return 0;
}