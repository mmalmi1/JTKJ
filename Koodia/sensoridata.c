/*#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include "testdata1.h"
#include "testdata2.h" 
#include "testdata3.h" 

float keskiarvo(float data[][7], int indeksi);

int main() {
    int j;
    for (j=1; j<7; j++) {
        keskiarvo(testdata2, j);
    }
    return 0;
}

float keskiarvo(float data[][7], int indeksi) {
    int i;
    int j;
    float summa = 0;
    float var_summa = 0;
    float varianssi = 0;
    int n = 0;
    
    for (i=0; i<100; i++) {
        summa += data[i][indeksi];
        n += 1;
    }
    summa = summa / n;
    
    for (j=0; j<n+1; j++) {
        var_summa += pow((data[j][indeksi] - summa), 2);
    }
    varianssi = var_summa / (n - 1);    
    printf("Keskiarvo on: %.4f\n", summa);
    printf("Varianssi on: %.4f\n\n", varianssi);
    
    return 0;
}*/