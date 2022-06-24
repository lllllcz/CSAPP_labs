// 520021911275 李忱泽

/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;
    int tmp1;
    int tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;

    if (M == 64 && N == 64) {
        for (i = 0; i < N-3; i+=4) {
            for (j = 0; j < M-1; j+=2) {
                tmp1 = A[i][j];
                tmp2 = A[i+1][j];
                tmp3 = A[i+2][j];
                tmp4 = A[i+3][j];
                tmp5 = A[i][j+1];
                tmp6 = A[i+1][j+1];
                tmp7 = A[i+2][j+1];
                tmp8 = A[i+3][j+1];
                B[j][i] = tmp1;
                B[j][i+1] = tmp2;
                B[j][i+2] = tmp3;
                B[j][i+3] = tmp4;
                B[j+1][i] = tmp5;
                B[j+1][i+1] = tmp6;
                B[j+1][i+2] = tmp7;
                B[j+1][i+3] = tmp8;
            }
        }
    }
    else if (M == 32 && N == 32) {
        int ii, jj;
        for (i = 0; i < N-7; i+=8) {
            for (j = 0; j < M-7; j+=8) {
                for (jj = j; jj < j+8; jj++) {
                    for (ii = i; ii < i+8; ii++) {
                        tmp1 = A[ii][jj];
                        B[jj][ii] = tmp1;
                    }
                }
            }
        }
    }
    else {
        for (i = 0; i < N-7; i+=8) {
            for (j = 0; j < M; j+=1) {
                tmp1 = A[i][j];
                tmp2 = A[i+1][j];
                tmp3 = A[i+2][j];
                tmp4 = A[i+3][j];
                tmp5 = A[i+4][j];
                tmp6 = A[i+5][j];
                tmp7 = A[i+6][j];
                tmp8 = A[i+7][j];
                B[j][i] = tmp1;
                B[j][i+1] = tmp2;
                B[j][i+2] = tmp3;
                B[j][i+3] = tmp4;
                B[j][i+4] = tmp5;
                B[j][i+5] = tmp6;
                B[j][i+6] = tmp7;
                B[j][i+7] = tmp8;
            }
        }
    }


    for (; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp1 = A[i][j];
            B[j][i] = tmp1;
        }
    }
    
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

