all :
	mpicc -o psudoku psudoku.c -lpthread -lm
	mpicc -o pchess pchess.c -lpthread -lm
