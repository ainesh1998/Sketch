#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "display.h"

// The structure of coordinates, which tell the position of the pen on the screen.
struct coor {
    int x; int y;
};
typedef struct coor coor;

// State structure, which contains the previous position, the current position,
// the state of the pen
struct state {
    coor prev;
    coor current;
    bool penDown;
    int length;
    int opcode;
    int lengthToGet;
    int extendedOperand;
    bool isExtend;
    display *screen;
};
typedef struct state state;

//------------------------------------------------------------------------------
// Logic.

// Extracts the opcode from a byte. Opcode differs depending on whether extended
// or not.
int getOpcode(int byte, bool extend) {
    if (!extend) return (byte & 0xC0) >> 6;
    return byte & 0x0F;
}

// Extracts the operand from a byte.
int getOperand(int byte, bool extend, state *s) {
    int result = byte & 0x3F;
    if (!extend) return result;
    return s->extendedOperand;
}

// Checks if a sequence of bits should be converted to signed, and converts it.
int toSigned(int n, int length, bool extend) {
    if (extend) {
        int max = (1 << (length-1)) - 1;
        if (n >= max) n = n - (1 << (length));
    }
    else {
        if (n > 31) n = n - 64;
    }
    return n;
}

/*// Changes the extension of a .sketch file to .sk.
void title (char *input, char output[]) {
    strcpy(output, input);
    output[strlen(input)-4] = '\0';
}*/

//------------------------------------------------------------------------------
// Output.

// Draws a line on the screen.
void draw(state *s) {
    line(s->screen, s->prev.x, s->prev.y, s->current.x, s->current.y);
}

// Executes Opcode 0.
void opcodeZero(int operand, state *s) {
    operand = toSigned(operand, s->length*8, s->isExtend);
    s->current.x = s->current.x + operand;
}

// Executes Opcode 1.
void opcodeOne(int operand, state *s) {
    operand = toSigned(operand, s->length*8, s->isExtend);
    s->current.y = s->current.y + operand;
    if (s->penDown) draw(s);
    s->prev.x = s->current.x;
    s->prev.y = s->current.y;
}

// Executes Opcode 2.
void opcodeTwo(int operand, state *s) {
    pause(s->screen, operand*10);
}

// Executes Opcode 3.
void opcodeThree(state *s) {
    s->penDown = !(s->penDown);
}

// Executes Opcode 4.
void opcodeFour(state *s) {
    clear(s->screen);
}

// Executes Opcode 5.
void opcodeFive(state *s) {
    key(s->screen);
}

// Executes Opcode 6.
void opcodeSix(int operand, state *s) {
    colour(s->screen, operand);
}

void execute(int opcode, int operand, state *s);

// Handles the extended opcodes.
void extended(int operand, state *s) {
    s->isExtend = true;
    int length = (operand & 0x30) >> 4;
    if (length == 0) {
        execute(s->opcode, 0, s);
    }
    else {
        if (length == 3) s->lengthToGet = 4;
        else s->lengthToGet = length;
        s->length = s->lengthToGet;
    }
}

// Does a sketch, according to the instructions in the byte given.
void execute(int opcode, int operand, state *s) {
    if (opcode == 0) opcodeZero(operand, s);
    else if (opcode == 1) opcodeOne(operand, s);
    else if (opcode == 2) opcodeTwo(operand, s);
    else {
        if (!s->isExtend) {
            s->opcode = getOpcode(operand, 1);
            extended(operand, s);
        }
        else {
            if (s->opcode == 3) opcodeThree(s);
            else if (s->opcode == 4) opcodeFour(s);
            else if (s->opcode == 5) opcodeFive(s);
            else if (s->opcode == 6) opcodeSix(operand, s);
            s->isExtend = false;
        }
    }
}

// Handles output.
void giveOutput(int byte, state *s) {
    if (s->isExtend) {
        if (s->lengthToGet != 0) {
            s->extendedOperand = (s->extendedOperand << 8) | byte;
            s->lengthToGet--;
        }
        if (s->lengthToGet == 0) {
            int opcode = s->opcode;
            int operand = getOperand(byte, 1, s);
            s->extendedOperand = 0;
            execute(opcode, operand, s);
            s->isExtend = false;
        }
    }
    else {
        int opcode = getOpcode(byte, 0);
        int operand = getOperand(byte, 0, s);
        execute(opcode, operand, s);
    }
}

//------------------------------------------------------------------------------
// Input.

// Reads in bytes from an input file,
void readByte(char *input, state *s) {
    s->screen = newDisplay(input, 200, 200);
    FILE *in = fopen(input, "rb");
    unsigned char b = fgetc(in);
    while (! feof(in)) {
        giveOutput(b, s);
        b = fgetc(in);
    }
    end(s->screen);
    fclose(in);
}

//----------------------------------------------------------------------------
// Testing.

// Tests that the right opcode is extracted.
void testGetOpcode(state *s) {
    assert(getOpcode(0, 0) == 0);
    assert(getOpcode(86, 0) == 1);
    assert(getOpcode(157, 0) == 2);
    assert(getOpcode(255, 0) == 3);
}

// Tests that the right operand is extracted.
void testGetOperand(state *s) {
    assert(getOperand(0, 0, s) == 0);
    assert(getOperand(86, 0, s) == 22);
    assert(getOperand(157, 0, s) == 29);
    assert(getOperand(255, 0, s) == 63);
}

// Tests the toSigned function.
void testToSigned() {
    assert(toSigned(65396, 16, 1) == -140);
    assert(toSigned(2398, 12, 1) == -1698);
    assert(toSigned(14, 4, 1) == -2);
    assert(toSigned(5, 4, 1) == 5);
}

/*// Sets up testing for the title function.
void testOneTitle(int size, char *input, char *result) {
    char output[size]; title(input, output);
    assert(strcmp(output, result) == 0);
}

// Tests the title function.
void testTitle() {
    testOneTitle(12, "line.sketch", "line.sk");
    testOneTitle(14, "square.sketch", "square.sk");
    testOneTitle(11, "box.sketch", "box.sk");
}*/

// Runs all the tests.
void test(state *s) {
    testGetOpcode(s);
    testGetOperand(s);
    testToSigned();
    //testTitle();
}

// Main function, runs the tests and takes in the input.
int main(int n, char **args) {
    state *s = malloc(sizeof(state));
    if (n == 2) readByte(args[1], s);
    else if (n == 1) {
        test(s);
        printf("All tests pass.\n");
    }
}
