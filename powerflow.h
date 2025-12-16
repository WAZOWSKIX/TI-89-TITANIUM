// Header File
// Created 7/12/2025; 12:43:29 p. m.
/* powerflow.h - HEADER FINAL */
/* TIGCC - ANSI C89 ESTRICTO */

#ifndef _POWERFLOW_H_
#define _POWERFLOW_H_

#ifndef PI
#define PI 3.14159265
#endif

#define MAX_BUS 4
#define MAX_ITER 30
#define TOL 0.0001

#define TYPE_SLACK 1
#define TYPE_PQ 2
#define TYPE_PV 3

#ifndef KEY_ESC
#define KEY_ESC 264
#endif
#ifndef KEY_UP
#define KEY_UP 338
#endif
#ifndef KEY_DOWN
#define KEY_DOWN 344
#endif
#ifndef KEY_LEFT
#define KEY_LEFT 337
#endif
#ifndef KEY_RIGHT
#define KEY_RIGHT 340
#endif
#ifndef KEY_ENTER
#define KEY_ENTER 13
#endif

typedef struct {
    double r; 
    double i; 
} Complex;

typedef struct {
    short id;
    short type;     
    double V;       
    double Th;      /* Se almacena en RADIANES internamente */
    double P_gen;
    double Q_gen;
    double P_load;
    double Q_load;
    double Q_min;   
    double Q_max;
} Bus;

extern Complex Ybus[MAX_BUS][MAX_BUS];
extern Bus buses[MAX_BUS];
extern short nBus;
extern short solved; 
extern short lastMethod; 
extern double gsAlpha;

Complex C_Add(Complex a, Complex b);
Complex C_Sub(Complex a, Complex b);
Complex C_Mult(Complex a, Complex b);
Complex C_Div(Complex a, Complex b);
Complex C_Scale(Complex a, double s);
double C_Abs(Complex a);
void printC(short x, short y, const char* lbl, Complex c);

double parseValue(char* str);
void inputStr(short x, short y, char* buffer, short maxLen);
double inputDouble(short x, short y, const char* label);

void configYbusTable(void);
void configBusesVisual(void);
void resetFlatStart(void); 

/* Solvers */
void solve_GS(short showProc); 
void solve_GS_Accel(short showProc); 
void solve_NR(short showProc); 
void solve_FDec(short showProc); 
void solve_DC(short showProc);   

#endif