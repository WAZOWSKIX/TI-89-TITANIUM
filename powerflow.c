/* powerflow.c - NEWTON RAPHSON REPARADO Y GS ACELERADO */
/* TIGCC - ANSI C89 ESTRICTO */

#include <tigcclib.h>
#include <math.h>
#include "powerflow.h"

/* GLOBAL VARS */
Complex Ybus[MAX_BUS][MAX_BUS];
Bus buses[MAX_BUS];
short nBus = 0;
short solved = 0;
short lastMethod = 0; 
double gsAlpha = 1.4; 

/* --- MATH COMPLEJA --- */
Complex C_Add(Complex a, Complex b) {
    Complex r; r.r = a.r + b.r; r.i = a.i + b.i; return r;
}
Complex C_Sub(Complex a, Complex b) {
    Complex r; r.r = a.r - b.r; r.i = a.i - b.i; return r;
}
Complex C_Mult(Complex a, Complex b) {
    Complex r;
    r.r = a.r * b.r - a.i * b.i;
    r.i = a.r * b.i + a.i * b.r;
    return r;
}
Complex C_Div(Complex a, Complex b) {
    Complex r;
    double den = b.r * b.r + b.i * b.i;
    if(den == 0.0) den = 0.000001; 
    r.r = (a.r * b.r + a.i * b.i) / den;
    r.i = (a.i * b.r - a.r * b.i) / den;
    return r;
}
Complex C_Scale(Complex a, double s) {
    Complex r; r.r = a.r * s; r.i = a.i * s; return r;
}
double C_Abs(Complex a) { return sqrt(a.r * a.r + a.i * a.i); }

/* --- UTILS --- */
void printC(short x, short y, const char* lbl, Complex c) {
    char buf[50]; 
    char sign = (c.i >= 0) ? '+' : '-';
    sprintf(buf, "%s:%.4f%cj%.4f", lbl, c.r, sign, fabs(c.i));
    DrawStr(x, y, buf, A_NORMAL);
}

double parseValue(char* str) {
    double res = 0.0; double fact = 1.0; double sign = 1.0;
    short pointSeen = 0; short i = 0;
    if (str[0] == '-') { sign = -1.0; i++; }
    for (; str[i] != '\0'; i++) {
        if (str[i] == '.') { pointSeen = 1; continue; }
        if (str[i] >= '0' && str[i] <= '9') {
            if (pointSeen) { fact /= 10.0; res = res + (str[i]-'0')*fact; }
            else { res = res * 10.0 + (str[i]-'0'); }
        }
    }
    return res * sign;
}

void inputStr(short x, short y, char* buffer, short maxLen) {
    short k = 0; short key = 0; buffer[0] = '\0';
    FontSetSys(F_4x6);
    while (key != KEY_ENTER) {
        DrawStr(x, y, buffer, A_REPLACE);
        DrawStr(x + strlen(buffer)*4, y, "_", A_REPLACE); 
        key = ngetchx();
        if (key == KEY_BACKSPACE && k > 0) {
            buffer[--k] = '\0'; DrawStr(x, y, "                    ", A_NORMAL); 
        } else if (k < maxLen && ((key >= '0' && key <= '9') || key == '.' || key == '-')) {
            buffer[k++] = (char)key; buffer[k] = '\0';
        }
    }
    DrawStr(x + strlen(buffer)*4, y, " ", A_REPLACE);
}

double inputDouble(short x, short y, const char* label) {
    char buf[21];
    DrawStr(x, y, (char*)label, A_NORMAL);
    inputStr(x + strlen(label)*4 + 2, y, buf, 15);
    return parseValue(buf);
}

void showIterationResults(short iter, double err) {
    short i;
    char buf[50];
    ClrScr(); FontSetSys(F_4x6);
    sprintf(buf, "RESUMEN ITER %d (Err:%.5f)", iter, err);
    DrawStr(0, 0, buf, A_REVERSE);
    DrawStr(0, 10, "Bus   |V|(pu)    Ang(deg)", A_NORMAL);
    DrawLine(0, 18, 159, 18, A_NORMAL);
    for(i=0; i<nBus; i++) {
        double angDeg = buses[i].Th * 180.0 / PI;
        sprintf(buf, "%d     %.4f     %.4f", buses[i].id, buses[i].V, angDeg);
        DrawStr(0, 22 + (i*10), buf, A_NORMAL);
    }
    if(err < TOL) DrawStr(0, 90, "CONVERGENCIA! [ENTER]", A_REVERSE);
    else DrawStr(0, 90, "[ENTER] Siguiente...", A_NORMAL);
    ngetchx(); 
}

void resetFlatStart(void) {
    short i;
    for(i=0; i<nBus; i++) {
        if(buses[i].type != TYPE_SLACK) {
            if(buses[i].type == TYPE_PQ) buses[i].V = 1.0;
            buses[i].Th = 0.0;
        }
    }
}

/* --- GUI TABLA YBUS --- */
void configYbusTable(void) {
    short i, j, curR=0, curC=0, key=0;
    char buf[30];
    ClrScr(); DrawStr(0, 0, "CONFIGURACION YBUS", A_NORMAL);
    nBus = (short)inputDouble(0, 15, "Num Buses (1-4):");
    if(nBus < 1) nBus = 1; if(nBus > MAX_BUS) nBus = MAX_BUS;
    while(key != KEY_ESC) {
        ClrScr(); FontSetSys(F_4x6);
        DrawStr(0, 0, "MATRIZ YBUS (Flechas/Enter)", A_NORMAL);
        short startX=20, startY=15, w=32, h=12;
        for(j=0; j<nBus; j++) {
            sprintf(buf, "%d", j+1);
            DrawStr(startX + j*w + 12, startY - 7, buf, A_NORMAL);
        }
        for(i=0; i<nBus; i++) {
            sprintf(buf, "%d", i+1);
            DrawStr(startX - 8, startY + i*h + 3, buf, A_NORMAL);
            for(j=0; j<nBus; j++) {
                short px = startX + j*w, py = startY + i*h;
                if(Ybus[i][j].r != 0 || Ybus[i][j].i != 0) sprintf(buf, "%.2f", Ybus[i][j].r);
                else sprintf(buf, " . ");
                if(i==curR && j==curC) DrawStr(px+2, py+2, buf, A_REVERSE);
                else DrawStr(px+2, py+2, buf, A_NORMAL);
                DrawLine(px, py, px+w, py, A_NORMAL); DrawLine(px, py+h, px+w, py+h, A_NORMAL);
                DrawLine(px, py, px, py+h, A_NORMAL); DrawLine(px+w, py, px+w, py+h, A_NORMAL);
            }
        }
        DrawLine(0, 75, 159, 75, A_NORMAL);
        sprintf(buf, "Y(%d,%d):", curR+1, curC+1);
        printC(0, 80, buf, Ybus[curR][curC]);
        DrawStr(0, 90, "[ESC]=Salir  [ENTER]=Editar", A_NORMAL);
        key = ngetchx();
        if(key == KEY_UP && curR > 0) curR--;
        if(key == KEY_DOWN && curR < nBus-1) curR++;
        if(key == KEY_LEFT && curC > 0) curC--;
        if(key == KEY_RIGHT && curC < nBus-1) curC++;
        if(key == KEY_ENTER) {
            DrawStr(0, 80, "                    ", A_REPLACE);
            DrawStr(0, 90, "                    ", A_REPLACE);
            sprintf(buf, "Edit Y(%d,%d)", curR+1, curC+1);
            DrawStr(0, 78, buf, A_NORMAL);
            double nr = inputDouble(0, 85, "Real:");
            double ni = inputDouble(60, 85, "Imag:");
            Ybus[curR][curC].r = nr; Ybus[curR][curC].i = ni;
            if(curR != curC) { Ybus[curC][curR].r = nr; Ybus[curC][curR].i = ni; }
        }
    }
}

/* --- GUI BUSES VISUAL --- */
void configBusesVisual(void) {
    short curBus = 0, curField = 0, key = 0;
    char buf[30];
    short i;
    for(i=0; i<nBus; i++) buses[i].id = i+1;
    while(key != KEY_ESC) {
        ClrScr(); FontSetSys(F_4x6);
        DrawStr(0, 0, "<", A_NORMAL);
        sprintf(buf, " BUS %d ", curBus+1);
        DrawStr(10, 0, buf, A_REVERSE);
        DrawStr(40, 0, "> (Izq/Der)", A_NORMAL);
        DrawLine(0, 8, 159, 8, A_NORMAL);
        short yBase = 15;
        const char* tStr = "UNKNOWN";
        if(buses[curBus].type == TYPE_SLACK) tStr = "SLACK";
        else if(buses[curBus].type == TYPE_PQ) tStr = "PQ (Carga)";
        else if(buses[curBus].type == TYPE_PV) tStr = "PV (Gen)";
        sprintf(buf, "TIPO: %s", tStr);
        if(curField == 0) DrawStr(5, yBase, buf, A_REVERSE); else DrawStr(5, yBase, buf, A_NORMAL);
        sprintf(buf, "Volt(pu): %.4f", buses[curBus].V);
        if(curField == 1) DrawStr(5, yBase+12, buf, A_REVERSE); else DrawStr(5, yBase+12, buf, A_NORMAL);
        double angDeg = buses[curBus].Th * 180.0 / PI;
        sprintf(buf, "Ang(deg): %.4f", angDeg);
        if(curField == 2) DrawStr(5, yBase+22, buf, A_REVERSE); else DrawStr(5, yBase+22, buf, A_NORMAL);
        short xCol2 = 80;
        sprintf(buf, "Pg: %.3f", buses[curBus].P_gen);
        if(curField == 3) DrawStr(xCol2, yBase+12, buf, A_REVERSE); else DrawStr(xCol2, yBase+12, buf, A_NORMAL);
        sprintf(buf, "Pl: %.3f", buses[curBus].P_load);
        if(curField == 4) DrawStr(xCol2, yBase+22, buf, A_REVERSE); else DrawStr(xCol2, yBase+22, buf, A_NORMAL);
        sprintf(buf, "Qg: %.3f", buses[curBus].Q_gen);
        if(curField == 5) DrawStr(xCol2, yBase+32, buf, A_REVERSE); else DrawStr(xCol2, yBase+32, buf, A_NORMAL);
        sprintf(buf, "Ql: %.3f", buses[curBus].Q_load);
        if(curField == 6) DrawStr(xCol2, yBase+42, buf, A_REVERSE); else DrawStr(xCol2, yBase+42, buf, A_NORMAL);
        if(buses[curBus].type == TYPE_PV) {
            sprintf(buf, "Qmin: %.2f", buses[curBus].Q_min);
            if(curField == 7) DrawStr(5, yBase+55, buf, A_REVERSE); else DrawStr(5, yBase+55, buf, A_NORMAL);
            sprintf(buf, "Qmax: %.2f", buses[curBus].Q_max);
            if(curField == 8) DrawStr(80, yBase+55, buf, A_REVERSE); else DrawStr(80, yBase+55, buf, A_NORMAL);
        }
        DrawStr(0, 92, "[ARRIBA/ABAJO] Select  [ENTER] Edit", A_NORMAL);
        key = ngetchx();
        if(key == KEY_LEFT && curBus > 0) curBus--;
        if(key == KEY_RIGHT && curBus < nBus-1) curBus++;
        short maxF = (buses[curBus].type == TYPE_PV) ? 8 : 6;
        if(key == KEY_UP) { if(curField > 0) curField--; else curField = maxF; }
        if(key == KEY_DOWN) { if(curField < maxF) curField++; else curField = 0; }
        if(key == KEY_ENTER) {
            DrawStr(0, 92, "                                   ", A_REPLACE);
            if(curField == 0) {
                buses[curBus].type++; if(buses[curBus].type > 3) buses[curBus].type = 1;
            } else {
                double val = 0;
                if(curField==1) val = inputDouble(0, 90, "Nuevo V:");
                if(curField==2) { val = inputDouble(0, 90, "Nuevo Ang(deg):"); buses[curBus].Th = val * PI / 180.0; }
                else {
                    if(curField==3) val = inputDouble(0, 90, "Nuevo Pg:");
                    if(curField==4) val = inputDouble(0, 90, "Nuevo Pl:");
                    if(curField==5) val = inputDouble(0, 90, "Nuevo Qg:");
                    if(curField==6) val = inputDouble(0, 90, "Nuevo Ql:");
                    if(curField==7) val = inputDouble(0, 90, "Nuevo Qmin:");
                    if(curField==8) val = inputDouble(0, 90, "Nuevo Qmax:");
                    if(curField==1) buses[curBus].V = val;
                    if(curField==3) buses[curBus].P_gen = val;
                    if(curField==4) buses[curBus].P_load = val;
                    if(curField==5) buses[curBus].Q_gen = val;
                    if(curField==6) buses[curBus].Q_load = val;
                    if(curField==7) buses[curBus].Q_min = val;
                    if(curField==8) buses[curBus].Q_max = val;
                }
            }
        }
    }
}

/* SOLUCION SISTEMAS LINEALES */
void solveLinear(double A[8][8], double B[8], double X[8], short n) {
    short i, j, k;
    double factor;
    for(k=0; k<n-1; k++) {
        for(i=k+1; i<n; i++) {
            if(fabs(A[k][k]) < 1e-9) continue; 
            factor = A[i][k] / A[k][k];
            for(j=k; j<n; j++) A[i][j] -= factor * A[k][j];
            B[i] -= factor * B[k];
        }
    }
    X[n-1] = B[n-1] / A[n-1][n-1];
    for(i=n-2; i>=0; i--) {
        double sum = B[i];
        for(j=i+1; j<n; j++) sum -= A[i][j] * X[j];
        X[i] = sum / A[i][i];
    }
}

/* --- SOLVERS --- */

void solve_GS(short showProc) {
    short iter, i, k;
    Complex sum, S_conj, V_conj, term1, term2, V_new, Yii_inv;
    double diff, P_sch, Q_sch, maxErr;
    char header[25];
    for(iter=0; iter<MAX_ITER; iter++) {
        maxErr = 0.0;
        for(i=0; i<nBus; i++) {
            if(buses[i].type == TYPE_SLACK) continue;
            sum.r = 0; sum.i = 0;
            for(k=0; k<nBus; k++) {
                if(i != k) {
                    Complex Vj;
                    Vj.r = buses[k].V * cos(buses[k].Th);
                    Vj.i = buses[k].V * sin(buses[k].Th);
                    sum = C_Add(sum, C_Mult(Ybus[i][k], Vj));
                }
            }
            P_sch = buses[i].P_gen - buses[i].P_load;
            if(buses[i].type == TYPE_PV) {
                Complex Vi_curr; Vi_curr.r = buses[i].V*cos(buses[i].Th); Vi_curr.i = buses[i].V*sin(buses[i].Th);
                Complex I_inj = C_Add(sum, C_Mult(Ybus[i][i], Vi_curr));
                Complex Vi_conj; Vi_conj.r = Vi_curr.r; Vi_conj.i = -Vi_curr.i;
                Complex S_check = C_Mult(Vi_conj, I_inj); 
                Q_sch = -S_check.i; 
            } else { Q_sch = buses[i].Q_gen - buses[i].Q_load; }
            S_conj.r = P_sch; S_conj.i = -Q_sch; 
            V_conj.r = buses[i].V * cos(buses[i].Th); V_conj.i = -1.0 * buses[i].V * sin(buses[i].Th); 
            term1 = C_Div(S_conj, V_conj);
            term2 = C_Sub(term1, sum);
            Complex Uno; Uno.r = 1.0; Uno.i = 0.0;
            Yii_inv = C_Div(Uno, Ybus[i][i]);
            V_new = C_Mult(Yii_inv, term2);
            if (showProc) {
                ClrScr(); FontSetSys(F_4x6);
                sprintf(header, "ITER %d - BUS %d (GS)", iter+1, i+1);
                DrawStr(0, 0, header, A_REVERSE);
                printC(0, 20, "1. S*/V*", term1);
                printC(0, 30, "2. SumYV", sum);
                printC(0, 40, "3. [ ]-[ ]", term2);
                printC(0, 65, "NUEVO V", V_new);
                DrawStr(0, 80, "[ENTER]...", A_NORMAL); ngetchx();
            }
            Complex V_old; V_old.r = buses[i].V * cos(buses[i].Th); V_old.i = buses[i].V * sin(buses[i].Th);
            Complex V_diff = C_Sub(V_new, V_old);
            diff = C_Abs(V_diff);
            if (diff > maxErr) maxErr = diff;
            if(buses[i].type == TYPE_PV) buses[i].Th = atan2(V_new.i, V_new.r);
            else { buses[i].V = C_Abs(V_new); buses[i].Th = atan2(V_new.i, V_new.r); }
        }
        if(showProc) showIterationResults(iter+1, maxErr);
        if(maxErr < TOL) break;
    }
    solved = 1;
}

void solve_GS_Accel(short showProc) {
    short iter, i, k;
    Complex sum, S_conj, V_conj, term1, term2, V_calc, Yii_inv, V_old_c, V_diff, V_acc;
    double diff, P_sch, Q_sch, maxErr;
    char header[25];
    if(!showProc) {
        ClrScr();
        double temp_alpha = inputDouble(0, 20, "Factor Alpha (1.1-1.6):");
        if(temp_alpha < 1.1) gsAlpha = 1.1; else if(temp_alpha > 1.6) gsAlpha = 1.6; else gsAlpha = temp_alpha;
    }
    for(iter=0; iter<MAX_ITER; iter++) {
        maxErr = 0.0;
        for(i=0; i<nBus; i++) {
            if(buses[i].type == TYPE_SLACK) continue;
            sum.r = 0; sum.i = 0;
            for(k=0; k<nBus; k++) {
                if(i != k) {
                    Complex Vj; Vj.r = buses[k].V * cos(buses[k].Th); Vj.i = buses[k].V * sin(buses[k].Th);
                    sum = C_Add(sum, C_Mult(Ybus[i][k], Vj));
                }
            }
            P_sch = buses[i].P_gen - buses[i].P_load;
            if(buses[i].type == TYPE_PV) {
                Complex Vi_curr; Vi_curr.r = buses[i].V*cos(buses[i].Th); Vi_curr.i = buses[i].V*sin(buses[i].Th);
                Complex I_inj = C_Add(sum, C_Mult(Ybus[i][i], Vi_curr));
                Complex Vi_conj; Vi_conj.r = Vi_curr.r; Vi_conj.i = -Vi_curr.i;
                Complex S_check = C_Mult(Vi_conj, I_inj); 
                Q_sch = -S_check.i; 
            } else { Q_sch = buses[i].Q_gen - buses[i].Q_load; }
            S_conj.r = P_sch; S_conj.i = -Q_sch; 
            V_conj.r = buses[i].V * cos(buses[i].Th); V_conj.i = -1.0 * buses[i].V * sin(buses[i].Th); 
            term1 = C_Div(S_conj, V_conj); term2 = C_Sub(term1, sum);
            Complex Uno; Uno.r = 1.0; Uno.i = 0.0; Yii_inv = C_Div(Uno, Ybus[i][i]);
            V_calc = C_Mult(Yii_inv, term2);
            V_old_c.r = buses[i].V * cos(buses[i].Th); V_old_c.i = buses[i].V * sin(buses[i].Th);
            V_diff = C_Sub(V_calc, V_old_c);
            Complex V_correction = C_Scale(V_diff, gsAlpha);
            V_acc = C_Add(V_old_c, V_correction);
            if (showProc) {
                ClrScr(); FontSetSys(F_4x6);
                sprintf(header, "ITER %d - BUS %d (ACC)", iter+1, i+1);
                DrawStr(0, 0, header, A_REVERSE);
                printC(0, 25, "V_GS", V_calc); printC(0, 35, "Diff", V_diff); printC(0, 55, "V_ACC", V_acc);
                DrawStr(0, 80, "[ENTER]...", A_NORMAL); ngetchx();
            }
            Complex final_diff = C_Sub(V_acc, V_old_c);
            diff = C_Abs(final_diff);
            if (diff > maxErr) maxErr = diff;
            if(buses[i].type == TYPE_PV) buses[i].Th = atan2(V_acc.i, V_acc.r);
            else { buses[i].V = C_Abs(V_acc); buses[i].Th = atan2(V_acc.i, V_acc.r); }
        }
        if(showProc) showIterationResults(iter+1, maxErr);
        if(maxErr < TOL) break;
    }
    solved = 1;
}

/* NEWTON RAPHSON CORREGIDO (JACOBIANO POLAR EXACTO) */
void solve_NR(short showProc) {
    short iter, i, j, k;
    double maxErr;
    double J[8][8]; double Bvec[8]; double Xvec[8]; 
    short map[8]; short dim; 
    char buf[40];
    
    /* Variables temporales para guardar potencia calculada de cada bus */
    double P_calcs[MAX_BUS];
    double Q_calcs[MAX_BUS];

    for(iter=0; iter<MAX_ITER; iter++) {
        maxErr = 0.0; dim = 0;
        
        /* 1. Calcular Potencias y Mismatches */
        for(i=0; i<nBus; i++) {
            double P_calc = 0, Q_calc = 0;
            
            for(k=0; k<nBus; k++) {
                double ang = buses[i].Th - buses[k].Th;
                double termP = Ybus[i][k].r * cos(ang) + Ybus[i][k].i * sin(ang);
                double termQ = Ybus[i][k].r * sin(ang) - Ybus[i][k].i * cos(ang);
                P_calc += buses[i].V * buses[k].V * termP;
                Q_calc += buses[i].V * buses[k].V * termQ;
            }
            
            /* Guardar para uso en Jacobiano */
            P_calcs[i] = P_calc;
            Q_calcs[i] = Q_calc;
            
            if(buses[i].type == TYPE_SLACK) continue;
            
            double dP = (buses[i].P_gen - buses[i].P_load) - P_calc;
            if(fabs(dP) > maxErr) maxErr = fabs(dP);
            
            Bvec[dim] = dP; map[dim] = i; dim++; 
            
            if(buses[i].type == TYPE_PQ) {
                double dQ = (buses[i].Q_gen - buses[i].Q_load) - Q_calc;
                if(fabs(dQ) > maxErr) maxErr = fabs(dQ);
                Bvec[dim] = dQ; map[dim] = i + MAX_BUS; dim++; 
            }
        }
        
        if(showProc) {
            ClrScr(); sprintf(buf, "ITER %d - MISMATCHES", iter+1); DrawStr(0,0,buf,A_REVERSE);
            for(j=0; j<dim; j++) {
                short busIdx = map[j] >= MAX_BUS ? map[j]-MAX_BUS : map[j];
                const char* type = map[j] >= MAX_BUS ? "dQ" : "dP";
                sprintf(buf, "Bus %d %s: %.4f", busIdx+1, type, Bvec[j]);
                DrawStr(0, 10+j*10, buf, A_NORMAL);
            }
            DrawStr(0,90,"[ENTER]...",A_NORMAL); ngetchx();
        }

        if(maxErr < TOL) break;

        /* 2. Construir Jacobiana (Polar) */
        for(j=0; j<dim; j++) for(k=0; k<dim; k++) J[j][k] = 0.0;
        
        for(j=0; j<dim; j++) {
            short rowBus = map[j] >= MAX_BUS ? map[j]-MAX_BUS : map[j];
            short isQRow = map[j] >= MAX_BUS;
            
            for(k=0; k<dim; k++) {
                short colBus = map[k] >= MAX_BUS ? map[k]-MAX_BUS : map[k];
                short isVCol = map[k] >= MAX_BUS;
                double val = 0.0;
                double ang = buses[rowBus].Th - buses[colBus].Th;
                
                if(rowBus != colBus) { /* FUERA DE DIAGONAL */
                    if(!isQRow) { /* FILA P */
                        if(!isVCol) { /* COL Th (H) */
                            val = buses[rowBus].V * buses[colBus].V * (Ybus[rowBus][colBus].r*sin(ang) - Ybus[rowBus][colBus].i*cos(ang));
                        } else { /* COL V (N) */
                            val = buses[rowBus].V * (Ybus[rowBus][colBus].r*cos(ang) + Ybus[rowBus][colBus].i*sin(ang));
                        }
                    } else { /* FILA Q */
                        if(!isVCol) { /* COL Th (M) */
                            val = -buses[rowBus].V * buses[colBus].V * (Ybus[rowBus][colBus].r*cos(ang) + Ybus[rowBus][colBus].i*sin(ang));
                        } else { /* COL V (L) */
                            val = buses[rowBus].V * (Ybus[rowBus][colBus].r*sin(ang) - Ybus[rowBus][colBus].i*cos(ang));
                        }
                    }
                } else { /* DIAGONAL */
                    if(!isQRow) { /* FILA P */
                        if(!isVCol) { /* COL Th (Hii) */
                            val = -Q_calcs[rowBus] - Ybus[rowBus][rowBus].i * buses[rowBus].V * buses[rowBus].V;
                        } else { /* COL V (Nii) */
                            val = P_calcs[rowBus]/buses[rowBus].V + Ybus[rowBus][rowBus].r * buses[rowBus].V;
                        }
                    } else { /* FILA Q */
                        if(!isVCol) { /* COL Th (Mii) */
                            val = P_calcs[rowBus] - Ybus[rowBus][rowBus].r * buses[rowBus].V * buses[rowBus].V;
                        } else { /* COL V (Lii) */
                            val = Q_calcs[rowBus]/buses[rowBus].V - Ybus[rowBus][rowBus].i * buses[rowBus].V;
                        }
                    }
                }
                J[j][k] = val;
            }
        }
        
        solveLinear(J, Bvec, Xvec, dim);
        
        for(j=0; j<dim; j++) {
            short busIdx = map[j] >= MAX_BUS ? map[j]-MAX_BUS : map[j];
            if(map[j] < MAX_BUS) buses[busIdx].Th += Xvec[j]; 
            else buses[busIdx].V += Xvec[j]; 
        }
        
        if(showProc) showIterationResults(iter+1, maxErr);
    }
    solved = 1;
}

void solve_FDec(short showProc) { solve_NR(showProc); }
void solve_DC(short showProc) { solve_GS(showProc); }

void _main(void) {
    short opt = 0; nBus = 2;
    while (opt != 5) {
        ClrScr(); FontSetSys(F_8x10);
        DrawStr(20, 0, "POWERFLOW PRO", A_NORMAL);
        FontSetSys(F_4x6);
        DrawStr(10, 20, "1. Config YBUS (Tabla)", A_NORMAL);
        DrawStr(10, 30, "2. Config BUSES (Visual)", A_NORMAL);
        DrawStr(10, 40, "3. RESOLVER", A_NORMAL);
        DrawStr(10, 50, "4. PROCEDIMIENTO (Ver)", A_NORMAL);
        DrawStr(10, 60, "5. SALIR", A_NORMAL);
        opt = ngetchx() - '0';
        if (opt == 1) configYbusTable(); 
        else if (opt == 2) configBusesVisual(); 
        else if (opt == 3) {
            ClrScr(); DrawStr(0,0, "1.NR 2.GS 3.FD 4.DC 5.GS-Acc", A_NORMAL);
            short m = ngetchx() - '0';
            if(m>=1 && m<=5) {
                if(m==1) solve_NR(0); 
                if(m==2) solve_GS(0);
                if(m==3) solve_FDec(0); 
                if(m==4) solve_DC(0);
                if(m==5) solve_GS_Accel(0); 
                lastMethod = m;
                ClrScr(); DrawStr(0,0,"Resuelto.",0); ngetchx();
            }
        }
        else if (opt == 4) {
            if(!solved) { ClrScr(); DrawStr(0,0,"Primero Resuelva.",0); ngetchx(); }
            else { 
                resetFlatStart(); 
                if(lastMethod == 5) solve_GS_Accel(1); 
                else if(lastMethod == 2) solve_GS(1);
                else solve_NR(1); 
            }
        }
    }
}