/* lc3.c */
/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>

#include <sys/termios.h>
#include <sys/mman.h>

int itob(int n)
{
  int c, k;

  /* printf("Enter an integer in decimal number system\n"); */
  /* scanf("%d", &n); */

  printf("%d in binary number system is:\n", n);

  for (c=31;c>=0;c--)
  {
    k = n >> c;
    if (k & 1)
      printf("1");
    else
      printf("0");
  }
  printf("\n");

  return 0;
}
/* Registers */
enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* program counter */
    R_COND,
    R_COUNT
};

/* Opcodes */
enum
{
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

/* Condition Flags */
enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

/* Memory Mapped Registers */
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

/* TRAP Codes */
enum
{
    TRAP_GETC = 0x20,  /* get character from keyboard */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* input a string */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};


/* Memory Storage */
/* 65536 locations */
uint16_t memory[UINT16_MAX];

/* Register Storage */
uint16_t reg[R_COUNT];


/* Functions */
/* Sign Extend */
uint16_t sign_extend(uint16_t x, int bit_count)
{
  if ((x >> (bit_count -1)) & 1) {
    x |= (0xFFFF << bit_count);
  }
  return x;
}

void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}
/* Input Buffering */

void mem_write(uint16_t address,  uint16_t val)
{
  memory[address] = val;
}

uint16_t check_key()
{
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO,  &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  return select(1,  &readfds, NULL, NULL, &timeout) != 0;
}

uint16_t mem_read(uint16_t address)
{
  if (address == MR_KBSR)
    {
      if (check_key())
        {
          memory[MR_KBSR] = (1 << 15);
          memory[MR_KBDR] = getchar();
        }
      else
        {
          memory[MR_KBSR] = 0;
        }
    }
  return memory[address];
}

void handle_interrupt() {}
void disable_input_buffering() {}

uint swap16(uint16_t x) {
  return (x << 8) |  (x >> 8);
}

void read_image_file(FILE* file) {
  uint16_t origin;
  fread(&origin,  sizeof(origin), 1, file);
  origin = swap16(origin);

  uint16_t max_read = UINT16_MAX - origin;
  uint16_t* p = memory + origin;
  size_t read = fread(p, sizeof(uint16_t),  max_read, file);

  while (read-- >0) {
    *p = swap16(*p);
    ++p;
  }
}

int read_image(const char *image_path) {
  FILE* file = fopen(image_path,  "rb");
  if (!file) { return 0; }
  read_image_file(file);
  fclose(file);
  return 1;
}
void restore_input_buffering(){}

/* Main Loop */

int main(int argc, const char* argv[])
{
    /* Load Arguments */
    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    /* Setup */
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();


    /* set the PC to starting position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;
        printf("%x\n", instr);
        itob(instr);
        /* if(op == 0) { exit(1); } */
        switch (op)
        {
        case OP_BR:
          {
            uint16_t r0 = (instr >> 9) & 0x7;
            uint16_t r1 = (instr >> 6) & 0x7;
            uint16_t imm_flag = (instr >> 5) & 0x1;

            if (imm_flag)
              {
                uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                reg[r0] = reg[r1] & imm5;
              }
            else
              {
                uint16_t r2 = instr & 0x7;
                reg[r0] = reg[r1] & reg[r2];
              }
            update_flags(r0);
            break;
          }
            case OP_ADD:
              {
                /* destination register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;
                /* first operand (SR1) */
                uint16_t r1 = (instr >> 6) & 0x7;
                /* whether we are in immediate mode */
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if (imm_flag)
                  {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] + imm5;
                  }
                else
                  {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] + reg[r2];
                  }

                update_flags(r0);
              }
              break;
        case OP_LDI:
          {
            uint16_t r0 = ((instr & 0x0FFF) >> 9);
            uint16_t pc_offset = (sign_extend(instr & 0x01FF, 9));


            reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
            update_flags(r0);
          }
          break;
        case OP_AND:
          {
            uint16_t r0 = ((instr & 0x0FFF) >> 9);
            uint16_t r1 = ((instr & 0x0000000111000000) >> 6);
            uint16_t imm_flag = ((instr & 32) >> 5);
            uint16_t r2;
            if (imm_flag) {
              r2 = sign_extend((instr & 0x0000000000011111), 5);
              r0 = r1 & r2;
            } else {
              r2 = (instr & 0x0000000000000111);
              r0 = r1 & r2;
            }
            update_flags(r0);
          }
          break;
        case OP_LD:     /* load */
          {
            uint16_t r0 = ((instr & 0xFFF) >> 9);
            uint16_t offset = sign_extend((instr & 0x11111111),  8);
            reg[r0] = mem_read(reg[R_PC] + offset);
            update_flags(r0);
            break;
          }
    case OP_ST:     /* store */
          {
            uint16_t r0 = ((instr & 0xFFF) >> 9);
            uint16_t offset = sign_extend((instr & 0x11111111),  8);
            mem_write(reg[R_PC] + offset,  reg[r0]);
            update_flags(r0);
            break;
          }
    case OP_JSR:    /* jump register */
      {
        reg[R_R7] = reg[R_PC];
        if (instr & 0b0000100000000000) {
          reg[R_PC] =reg[R_PC] + sign_extend((instr & 0b0000011111111111), 11);
        } else {
          reg[R_PC] = reg[(instr & 0b0000000111000000)];
        }
        break;
      }
    case OP_LDR:    /* load register */
      {
        uint16_t dr = (instr & 0b0000111000000000) >> 9;
        uint16_t baser = (instr & 0b0000000111000000) >> 6;
        reg[dr] = mem_read(
                           reg[baser] +
                           sign_extend((instr & 0b0000000000111111), 5));
        update_flags(dr);
        break;
      }

    case OP_STR:    /* store register */
      {
        uint16_t sr = (instr & 0b0000111000000000) >> 9;
        uint16_t baser = (instr & 0b0000000111000000) >> 6;
        mem_write(reg[baser] + sign_extend((instr & 0b0000000000111111), 5),
                  sr);
        update_flags(sr);
        break;
      }
    case OP_RTI:    /* unused */
      {
        break;
      }
    case OP_NOT:    /* bitwise not */
      {
        uint16_t dr = (instr & 0b0000111000000000) >> 9;
        uint16_t sr = (instr & 0b0000000111000000) >> 6;

        reg[dr] = ! reg[sr];
        break;
      }
    case OP_STI:    /* store indirect */
      {
        uint16_t sr = (instr & 0b0000111000000000) >> 9;
        mem_write(reg[R_PC] + sign_extend((instr & 0b0000000111111111), 9), reg[sr]);
        break;
      }
    case OP_JMP:    /* jump */
      {
        /* uint16_t br = (instr & 0b0000000111000000) >> 6; */
        uint16_t br = (instr >> 6) & 0x7;
        reg[R_PC] = reg[br];
        break;
      }
    case OP_RES:    /* reserved (unused) */
    case OP_LEA:    /* load effective address */
      {

        uint16_t r0 = (instr >> 9) & 0x7;
        uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
        reg[r0] = reg[R_PC] + pc_offset;
        update_flags(r0);
        break;
      }
    case OP_TRAP:   /* execute trap */
      {
        switch(instr & 0xFF) {
        case TRAP_GETC:
          /* read a single ASCII char */
          reg[R_R0] = (uint16_t)getchar();
          break;
        case TRAP_OUT:
          putc((char)reg[R_R0], stdout);
          fflush(stdout);
          break;
        case TRAP_PUTS:
          {
            uint16_t* c = memory + reg[R_R0];
            while (*c){
              putc((char)*c,  stdout);
              ++c;
            }
            fflush(stdout);
          }
          break;
        case TRAP_IN:
          break;
        case TRAP_PUTSP:
          break;
        case TRAP_HALT:
          puts("HALT");
          fflush(stdout);
          running = 0;
          break;
        }
      }
      break;
        default:
          /* BAD OPCODE */
          abort();

          break;
        }
    }
    /* Shutdown */
    restore_input_buffering();

}
