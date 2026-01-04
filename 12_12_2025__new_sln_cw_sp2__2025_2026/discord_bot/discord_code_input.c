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

    //"4"
    opStack[++opStackIndex] = opTemp = 0x00000004;

    //"Read"
    (void)scanf_s("%d", &opTemp);
    data[opStack[opStackIndex]] = opTemp, opStackIndex = 0;

    //";"

    //"8"
    opStack[++opStackIndex] = opTemp = 0x00000008;

    //"Read"
    (void)scanf_s("%d", &opTemp);
    data[opStack[opStackIndex]] = opTemp, opStackIndex = 0;

    //";"

    //"_0AA"
    opStack[++opStackIndex] = opTemp = data[0x00000004];

    //"_0BB"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    //"_0AA"
    opStack[++opStackIndex] = opTemp = data[0x00000004];

    //"_0BB"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    //"_0AA"
    opStack[++opStackIndex] = opTemp = data[0x00000004];

    //"_0BB"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    //"_0AA"
    opStack[++opStackIndex] = opTemp = data[0x00000004];

    //"_0BB"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    //"_0AA"
    opStack[++opStackIndex] = opTemp = data[0x00000004];

    //"_0BB"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    //"_0AA"
    opStack[++opStackIndex] = opTemp = data[0x00000004];

    //"_0BB"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"--"
    opTemp = opStack[opStackIndex - 1] -= opStack[opStackIndex]; --opStackIndex;

    //"10"
    opStack[++opStackIndex] = opTemp = 0x0000000A;

    //"**"
    opTemp = opStack[opStackIndex - 1] *= opStack[opStackIndex]; --opStackIndex;

    //"_0AA"
    opStack[++opStackIndex] = opTemp = data[0x00000004];

    //"_0BB"
    opStack[++opStackIndex] = opTemp = data[0x00000008];

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"10"
    opStack[++opStackIndex] = opTemp = 0x0000000A;

    //"Div"
    opTemp = opStack[opStackIndex - 1] /= opStack[opStackIndex]; --opStackIndex;

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"12"
    opStack[++opStackIndex] = opTemp = 0x0000000C;

    //"->"
    lastBindDataIndex = opStack[opStackIndex];
    data[lastBindDataIndex] = opTemp = opStack[opStackIndex - 1], opStackIndex = 0;

    //";"

    //"_0XX"
    opStack[++opStackIndex] = opTemp = data[0x0000000C];

    //"_0XX"
    opStack[++opStackIndex] = opTemp = data[0x0000000C];

    //"10"
    opStack[++opStackIndex] = opTemp = 0x0000000A;

    //"Mod"
    opTemp = opStack[opStackIndex - 1] %= opStack[opStackIndex]; --opStackIndex;

    //"++"
    opTemp = opStack[opStackIndex - 1] += opStack[opStackIndex]; --opStackIndex;

    //"16"
    opStack[++opStackIndex] = opTemp = 0x00000010;

    //"->"
    lastBindDataIndex = opStack[opStackIndex];
    data[lastBindDataIndex] = opTemp = opStack[opStackIndex - 1], opStackIndex = 0;

    //";"

    //"_0XX"
    opStack[++opStackIndex] = opTemp = data[0x0000000C];

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    //"_0YY"
    opStack[++opStackIndex] = opTemp = data[0x00000010];

    //"Write"
    (void)printf("%d\r\n", opTemp = opStack[opStackIndex]), opStackIndex = 0;

    //";"

    return 0;
}