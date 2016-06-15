# parallelSudoku
##A parallel backtracking algorithm to solve a sudoku puzzle

###About
The module is based on solving a sudoku puzzle using a **randomized parallel algorithm** which is based on the following paper - **"Randomized Parallel Algorithms for Backtrack Search and Branch-and-Bound Computation" by RICHARD M. KARP and YANJUN ZHANG**.

The module is a parallel program for hybrid systems i.e, mutli-core systems in a cluster. This requried the use of 2 libraries - **OpenMP**(within a node parallelism) and **MPI**(cluster level parallelism).

The design of the algorithm was modified for the hybrid systems, though the overall idea is as described in the paper mentioned above. The code for the knight's tour problem (similar to solving sudoku) mentioned in the design has not been uploaded.

For detailed description of the module, view the design document provided above. The design is based on Foster's design methodology for designing parallel algorithms.

###Setup
1. Run the `make` command on the command prompt.
2. The files s1.txt, s2.txt and s3.txt contain sample sudoku puzzles. [0s in the text file represent missing values; s1 and s2 are 9x9 puzzles, s3 is a 16x16 puzzle] 
3. The file name is mentioned in line number 422 of psudoku.c [Sorry for the hard-coding, you can change it]
4. How to run - [This might depend on your cluster configuration also]<br />mpirun -n 4 ./psudoku [Will update the exact format for various possibilities later].

A similar algorithm can be developed for other backtracking search problems by necessary modifications. Examples? - The knight's tour problem, solving a prolog query, and many more.
