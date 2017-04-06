/***************************************************************************
 *
 * MPI-version of row-wise Matrix-Matrix multiplication
 *
 *             File : matmul_mpi.c
 *        Author(s) : Håkan Grahn
 *          Created : 2009-01-30
 *    Last Modified : 2009-01-30
 * Last Modified by : Håkan Grahn
 *
 * (c) 2009-2017 by Håkan Grahn, Blekinge Institute of Technology.
 * All Rights Reserved
 ***************************************************************************/

/*
 * Compile with:
 * mpicc -o mm matmul_mpi.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define SIZE 8	/* assumption: SIZE a multiple of number of nodes */
        /* SIZE should be 1024 in our measurements in the assignment */
        /* Hint: use small sizes when testing, e.g., SIZE 8 */
#define FROM_MASTER 1	/* setting a message type */
#define FROM_WORKER 2	/* setting a message type */
#define DEBUG 1		/* 1 = debug on, 0 = debug off */

MPI_Status status;

static double a[SIZE][SIZE];
static double b[SIZE][SIZE];
static double c[SIZE][SIZE];

static void
init_matrix(void)
{
    int i, j;

    for (i = 0; i < SIZE; i++)
        for (j = 0; j < SIZE; j++) {
            /* Simple initialization, which enables us to easy check
             * the correct answer. Given SIZE size of the matrices, then
             * the output should be
             *     SIZE ... 2*SIZE ...
             *     ...
             *     2*SIZE ... 4*SIZE ...
             *     ...
             */
            a[i][j] = 1.0;
            if (i >= SIZE/2) a[i][j] = 2.0;
            b[i][j] = 1.0;
            if (j >= SIZE/2) b[i][j] = 2.0;
        }
}

static void
transpose_second_matrix(void)
{
    int i, j;
    double tmp;
    for (i = 0; i < SIZE; i++) {
        for (j = i; j < SIZE; j++) {
            if (i == j) continue;
            tmp = b[i][j];
            b[i][j] = b[j][i];
            b[j][i] = tmp;
        }
    }
}

static void
print_matrix(void)
{
    int i, j;

    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++)
            printf(" %7.2f", c[i][j]);
        printf("\n");
    }
}

int
main(int argc, char **argv)
{
    int myrank, nproc;
    int num_rows; /* rows per worker */
    int num_cols; /* columns per worker */
    int mtype; /* message type: send/recv between master and workers */
    int offset_row; /* start row */
    int offset_col; /* start column */
    int dest, src;
    double start_time, end_time;
    int i, j, k;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    if (nproc % 4 != 0 && nproc != 1 && nproc != 2)
    {
        printf("Program was given %d cores, but execution only makes sense with 1, 2, or multiples of 4 cores!\n", nproc);
        return 1;
    }

    if (myrank == 0) {    /* Master task */
        /* Initialization */
        printf("SIZE = %d, number of nodes = %d\n", SIZE, nproc);
        init_matrix();
        start_time = MPI_Wtime();

        /* Transpose second matrix to allow more effective communication */
        transpose_second_matrix();

        /* Send part of matrix a and part of matrix b to workers */
        num_rows = nproc == 1 ? SIZE : SIZE / (nproc / 2);
        num_cols = nproc == 1 ? SIZE : SIZE / (nproc / 2);
        mtype = FROM_MASTER;
        offset_row = num_rows;
        offset_col = num_cols;

        for (dest = 1; dest < nproc; dest++) {
            if (DEBUG)
                printf("   sending %d rows and %d columns to task %d\n",num_rows,num_cols,dest);
            MPI_Send(&offset_row, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);
            MPI_Send(&offset_col, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

            MPI_Send(&num_rows, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);
            MPI_Send(&num_cols, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);

            MPI_Send(&a[offset_row][0], num_rows*SIZE, MPI_DOUBLE, dest, mtype, MPI_COMM_WORLD);
            MPI_Send(&b[offset_col][0], num_cols*SIZE, MPI_DOUBLE, dest, mtype, MPI_COMM_WORLD);

            offset_row += num_rows;
            offset_col += num_cols;
        }

        /* let master do its part of the work - TODO */
        for (i = 0; i < num_rows; i++) {
            for (j = 0; j < SIZE; j++) {
                c[i][j] = 0.0;
                for (k = 0; k < SIZE; k++)
                    c[i][j] = c[i][j] + a[i][k] * b[k][j];
            }
        }

        /* collect the results from all the workers - TODO */
        mtype = FROM_WORKER;
        for (src = 1; src < nproc; src++) {
            MPI_Recv(&offset_row, 1, MPI_INT, src, mtype, MPI_COMM_WORLD, &status);
            MPI_Recv(&num_rows, 1, MPI_INT, src, mtype, MPI_COMM_WORLD, &status);
            MPI_Recv(&c[offset_row][0], num_rows*SIZE, MPI_DOUBLE, src, mtype, MPI_COMM_WORLD, &status);
            if (DEBUG)
                printf("   recvd %d rows from task %d, offset = %d\n", num_rows, src, offset_row);
        }

        end_time = MPI_Wtime();
        if (DEBUG)
        {
            /* Prints the resulting matrix c */
            print_matrix();
        }
        printf("Execution time on %2d nodes: %f\n", nproc, end_time-start_time);

    } else {    /* Worker tasks */
        /* Receive data from master */
        mtype = FROM_MASTER;

        MPI_Recv(&offset_row, 1, MPI_INT, 0, mtype, MPI_COMM_WORLD, &status);
        MPI_Recv(&offset_col, 1, MPI_INT, 0, mtype, MPI_COMM_WORLD, &status);

        MPI_Recv(&num_rows, 1, MPI_INT, 0, mtype, MPI_COMM_WORLD, &status);
        MPI_Recv(&num_cols, 1, MPI_INT, 0, mtype, MPI_COMM_WORLD, &status);

        MPI_Recv(&a[offset_row][0], num_rows*SIZE, MPI_DOUBLE, 0, mtype, MPI_COMM_WORLD, &status);
        MPI_Recv(&b[offset_col][0], num_cols*SIZE, MPI_DOUBLE, 0, mtype, MPI_COMM_WORLD, &status);

        if (DEBUG)
            printf ("Rank=%d, offset_row=%d, offset_col=%d, num_rows=%d, num_cols=%d, a[offset_row][0]=%f, b[offset_col][0]=%f\n",
                myrank, offset_row, offset_col, num_rows, num_cols, a[offset_row][0], b[offset_row][0]);

        /* do the workers part of the calculation - TODO */
        for (i=offset_row; i<offset_row+num_rows; i++)
            for (j=0; j<SIZE; j++) {
                c[i][j] = 0.0;
                for (k=0; k<SIZE; k++)
                    c[i][j] = c[i][j] + a[i][k] * b[k][j];
            }
        if (DEBUG)
            printf("Rank=%d, offset=%d, row =%d, c[offset][0]=%e\n",
                myrank, offset_row, num_rows, c[offset_row][0]);

        /* send the results to the master - TODO */
        mtype = FROM_WORKER;
        MPI_Send(&offset_row, 1, MPI_INT, 0, mtype, MPI_COMM_WORLD);
        MPI_Send(&num_rows, 1, MPI_INT, 0, mtype, MPI_COMM_WORLD);
        MPI_Send(&c[offset_row][0], num_rows*SIZE, MPI_DOUBLE, 0, mtype, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
