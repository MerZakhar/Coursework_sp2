#define _CRT_SECURE_NO_WARNINGS

#include "stdio.h"

int data[8192] = {0};
int contextStack[8192] = {0}, contextStackIndex = 0;
int opStack[8192] = {0}, opStackIndex = 0, opTemp = 0;
int lastBindDataIndex = 0;

int main() {
    contextStackIndex = 0;
    opStackIndex = 0;
    opTemp = 0;
    lastBindDataIndex = 0;

    //";"

    //"16"
    opStack[++opStackIndex] = opTemp = 0x00000010;

    //"Read"
    (void)scanf_s("%d", &opTemp);
    data[opStack[opStackIndex]] = opTemp, opStackIndex = 0;

    //";"

    //"0"
    opStack[++opStackIndex] = opTemp = 0x00000000;

    //"8"
    opStack[++opStackIndex] = opTemp = 0x00000008;

    //"->"
    lastBindDataIndex = opStack[opStackIndex];
    data[lastBindDataIndex] = opTemp = opStack[opStackIndex - 1], opStackIndex = 0;

    //";"

    //"For"

    //"_0NN"
    opStack[++opStackIndex] = opTemp = data[0x00000010];

    //"12"
    opStack[++opStackIndex] = opTemp = 0x0000000C;

    //"->"
    lastBindDataIndex = opStack[opStackIndex];
    data[lastBindDataIndex] = opTemp = opStack[opStackIndex - 1], opStackIndex = 0;

    //"Downto" (after "For")
    ++data[lastBindDataIndex];
    contextStack[++contextStackIndex] = lastBindDataIndex;
LABEL__AFTER_DOWNTO_00007FF76467B4A0:

    //"0"
    opStack[++opStackIndex] = opTemp = 0x00000000;

    //"Do" (after "Downto" after "For")
    if (data/*OLD: opStack*/[contextStack[contextStackIndex]] <= opTemp) goto LABEL__EXIT_FOR_00007FF76467A400;
    --data/*OLD: opStack*/[contextStack[contextStackIndex]];

    //"20"
    opStack[++opStackIndex] = opTemp = 0x00000014;

    //"Read"
    (void)scanf_s("%d", &opTemp);
    data[opStack[opStackIndex]] = opTemp, opStackIndex = 0;

    //";"

    //"_0XX"
    opStack[++opStackIndex] = opTemp = data[0x00000014];

    //"4"
    opStack[++opStackIndex] = opTemp = 0x00000004;

    //"_0II"
    opStack[++opStackIndex] = opTemp = data[0x0000000C];

    //"INDEX"
    opTemp = opStack[opStackIndex - 1] += 128 * opStack[opStackIndex--]);

    //"->"
    lastBindDataIndex = opStack[opStackIndex];
    data[lastBindDataIndex] = opTemp = opStack[opStackIndex - 1], opStackIndex = 0;

    //";"

    //"If"

    //"4"
    opStack[++opStackIndex] = opTemp = 0x00000004;

    //"_0II"
    opStack[++opStackIndex] = opTemp = data[0x0000000C];

    //"INDEX_TO_VALUE"
    opTemp = opStack[opStackIndex - 1] = data[opStack[opStackIndex - 1] += (128 * opStack[opStackIndex--])];

    //"1"
    opStack[++opStackIndex] = opTemp = 0x00000001;

    //">"
    opTemp = opStack[opStackIndex - 1] = opStack[opStackIndex - 1] > opStack[opStackIndex]; --opStackIndex;

    //after cond expresion (after "If")
    if (opTemp == 0) goto LABEL__AFTER_THEN_00007FF764680398;

    //"_0SS"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"4"
    opStack[++opStackIndex] = opTemp = 0x00000004;

    //"_0II"
    opStack[++opStackIndex] = opTemp = data[0x0000000C];

    //"INDEX_TO_VALUE"
    opTemp = opStack[opStackIndex - 1] = data[opStack[opStackIndex - 1] += (128 * opStack[opStackIndex--])];

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"8"
    opStack[++opStackIndex] = opTemp = 0x00000008;

    //"->"
    lastBindDataIndex = opStack[opStackIndex];
    data[lastBindDataIndex] = opTemp = opStack[opStackIndex - 1], opStackIndex = 0;

    //";"

    //"123"
    opStack[++opStackIndex] = opTemp = 0x0000007B;

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    //";" (after "then"-part of If-operator)
    opTemp = 1;
LABEL__AFTER_THEN_00007FF764680398:

    //"}" (after "For")
    goto LABEL__AFTER_DOWNTO_00007FF76467B4A0;
LABEL__EXIT_FOR_00007FF76467A400:
    --contextStackIndex;

    //"_0SS"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    return 0;
}