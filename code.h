#include <stdio.h> 
 
 struct Command {
     int key;
     int arg1, arg2, arg3;
     int word_length;
 };
 
 struct Code {
     struct Command* rows;
     int length;
     int capacity;
     int mem_start;
     int mem_cnt, mem_cap;

     unsigned int* mem_ptrs;
              int* mem_vals;
 };

 #define INPUT_FLAG 0xB0BACEBA

 struct Code* loadFromFile(const char* path);

 int readLineAsFormat(struct Command* cmd, FILE * stream, int line);

 int checkCodeFormat(struct Code* code);

 int readMemoryAsFormat(struct Code* code, FILE * stream);

 int readCommandLine(struct Code* code, FILE * stream);

 void printCommand(struct Command cmd, FILE * stream);

 void codeDtor(struct Code * code);