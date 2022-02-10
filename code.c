 #include <stdlib.h>
 #include "code.h"

 struct Code* loadFromFile(const char* path)
 {
     FILE * stream = fopen(path, "r");
     if (!stream)
     {
         fprintf(stderr, "Couldn't open file \"%s\"\n", path);
         return NULL;
     }

     struct Code * code = (struct Code*)calloc(1, sizeof(struct Code));
     if (!code)
     {
         fprintf(stderr, "Couldn't allocate %u bytes for Code struct\n", sizeof(struct Code));
         free(code);
         return NULL;
     }

     if (readMemoryAsFormat(code, stream))
     {
         codeDtor(code);
         return NULL;
     }

     while (!feof(stream))
     {
         if (readCommandLine(code, stream))
         {
            codeDtor(code);
            return NULL;
         }
     }

     if (checkCodeFormat(code))
     {
         codeDtor(code);
         return NULL;
     }

     fclose(stream);
     return code;
 }

 int readLineAsFormat(struct Command* cmd, FILE * stream, int line_ord)
 {
    const int WORD = 14;
    const int PTR_CAP = 4;

    int err = fscanf(stream, "%x%x%x%x\n", &cmd->key, &cmd->arg1, &cmd->arg2, &cmd->arg3);
    
    if (err != 4)
    {
        fprintf(stderr, "Couldn't read code line %d (from 0). ", line_ord);
        if (ferror(stderr))
            fprintf(stderr, "File reading error\n");
        else fprintf(stderr, "Invalid line format\n");
        return -5;
    }


    int keys[19] = {0x99, 0x00, 0x01, 0x02, 0x03, 0x13, 0x04, 0x14, 0x80, 0x81, 0x82, 0x83, 0x93, 0x84, 0x94, 0x85, 0x95, 0x86, 0x96};

    int i, key_valid = 0;
    for (i = 0; i < sizeof(keys); ++i)
    {
        if (cmd->key == keys[i])
        {
            key_valid = 1;
            break;
        }
    }

    if (!key_valid)
    {
        fprintf(stderr, "Invalid command key: %x\n", cmd->key);
        return -1;
    }


    if (cmd->arg1 < 0 || cmd->arg1 >= (1 << (PTR_CAP * 4)))
    {
        fprintf(stderr, "Invalid arg1 value: %x\n", cmd->arg1);
        return -2;
    }

    if (cmd->arg2 < 0 || cmd->arg2 >= (1 << (PTR_CAP * 4)))
    {
        fprintf(stderr, "Invalid arg2 value: %x\n", cmd->arg2);
        return -3;
    }

    if (cmd->arg3 < 0 || cmd->arg3 >= (1 << (PTR_CAP * 4)))
    {
        fprintf(stderr, "Invalid arg3 value: %x\n", cmd->arg3);
        return -4;
    }

    cmd->word_length = 1;

    return 0;
 }

 int checkCodeFormat(struct Code* code) 
 {
    const int WORD = 14;
    const int PTR_CAP = 4;

    int cmd_ptr = code->mem_start;

    int i;
    for (i = 0; i < code->length; ++i)
    {
        cmd_ptr += code->rows[i].word_length;
    }

    if (cmd_ptr >= (1 << (PTR_CAP * 4)))
    {
        fprintf(stderr, "Not enough memory for program\n");
        return -1;
    }

    for (i = 0; i < code->mem_cnt; ++i)
    {
        if (code->mem_ptrs[i] >= code->mem_start && code->mem_ptrs[i] < cmd_ptr)
        {
            fprintf(stderr, "Collision of memory for cells and program\n");
            return -1;
        }
    }

    return 0;
 }

 int readMemoryAsFormat(struct Code* code, FILE * stream)
 {
    const int WORD = 14;
    const int PTR_CAP = 4;

    int err = fscanf(stream, "%x", &code->mem_start);
    if (err != 1)
    {
        fprintf(stderr, "Couldn't read program memory start\n");
        return -1;
    }

    while (!feof(stream))
    {
        int file_pos = ftell(stream);

        unsigned int cell = 0;
        err = fscanf(stream, "%x", &cell);


        if (err != 1)
        {
            fprintf(stderr, "Invalid format or empty program\n");
            return -2;
        }

        char op = 0;
        do { 
            err = fscanf(stream, "%c", &op);
        } while (err == 1 && (op == ' ' || op == '\n' || op == '\t'));

        if (err != 1)
        {
            fprintf(stderr, "Invalid memory assignation of %x\n", cell);
            return -4;
        }

        if (op != '=' && op != '<')
        {
            fseek(stream, file_pos, SEEK_SET);
            return 0;
        }

        if (cell < 0 || cell >= (1 << (PTR_CAP * 4)))
        {
            fprintf(stderr, "Invalid pointer format: %x\n", cell);
            return -3;
        }

        int i;
        for (i = 0; i < code->mem_cnt; ++i)
        {
            if (code->mem_ptrs[i] == cell)
            {
                fprintf(stderr, "Double cell definition: %x\n", cell);
                return -4;
            }
        }

        int value = INPUT_FLAG;

        if (op == '=')
        {
            err = fscanf(stream, "%d", &value);
            if (err != 1)
            {
                fprintf(stderr, "Invalid memory assignation of %x\n", cell);
                return -5;
            }
        }


        if (code->mem_cnt == code->mem_cap)
        {
            if (code->mem_cap == 0)
                code->mem_cap = 16;
            else code->mem_cap *= 2;

            code->mem_ptrs = (unsigned int*)realloc(code->mem_ptrs, sizeof(unsigned int) * code->mem_cap);
            code->mem_vals =          (int*)realloc(code->mem_vals, sizeof(         int) * code->mem_cap);
        }

        code->mem_ptrs[code->mem_cnt] = cell;
        code->mem_vals[code->mem_cnt] = value;
        code->mem_cnt++;
    }

    return 0;
 }

 int readCommandLine(struct Code* code, FILE * stream)
 {
    if (code->length == code->capacity)
    {
        if (code->capacity == 0)
            code->capacity = 4;
        else code->capacity *= 2;

        code->rows = (struct Command*)realloc(code->rows, sizeof(struct Command) * code->capacity);
    }

    if (readLineAsFormat(&code->rows[code->length], stream, code->length))
        return -1;


    code->length++;
    return 0;
 }

 void printCommand(struct Command cmd, FILE * stream)
 {
     fprintf(stream, "\x1b[38;2;87;106;250m%02x\033[1;97m %04X %04X %04X\033[0m", cmd.key, cmd.arg1, cmd.arg2, cmd.arg3);
 }

 void codeDtor(struct Code* code)
 {
     free(code->mem_ptrs);
     free(code->mem_vals);
     free(code->rows);
     free(code);
 }