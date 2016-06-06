#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <stddef.h>
#include <signal.h>
#include <omp.h>
#include <math.h>
#include <pthread.h>


#define N 9
#define NO_T 4

typedef struct stack{
	int i;
	int j;
	int value;
	int level;
	struct stack * next;
	struct stack * prev;
}stack;

typedef struct stackNode{
	int i;
	int j;
	int value;
	int level;
}stackNode;


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int permission=0;
int solved=0;
int flagWeDoneIt = 0;
int shared_sudoku[N][N]={-1};
int busy[NO_T]={0};
stack * shared_s;

struct node{
	int t_id;
	int p_id;
	int size;
};


stack * push_stack(stack * s,int i,int j,int value,int level)
{
	stack * temp = (stack *)malloc(sizeof(stack));
	temp->i = i;
	temp->j = j;
	temp->value = value;
	temp->level = level;
	temp->next = NULL;
	temp->prev = s;
	
	if(s != NULL)
	{
		s->next = temp;
	}

	return temp;
}

stack * pop_stack(stack * s)
{
	if(s == NULL)
		return s;
	
	stack * temp = s->prev;
	
	if(temp != NULL)
	{
		temp->next = NULL;
	}

	free(s);
	return temp;
}

void print_stack(stack * s)
{
	while(s!=NULL)
	{
		printf("*(%d %d %d %d) ",s->i,s->j,s->value,s->level);
		s = s->prev;
	}
	printf("*\n");
}


int get_array_sum()
{
	int i, sum = 0;

	for (i = 0; i < NO_T; ++i)
	{
		sum += busy[i];
	}

	return sum;
}

//Function to display solved Sudoku Board


// Function to check if board is solved now i. e. it has no vacancies
// If the board is full then it is solved correctly
// This is true because, for each number we put in any cell,  we always check if all other vacant places can be
// filled by some numbers such that board is always in valid state

int isFull(int sudoku[N][N], int size)
{
    int i,j;
    for(i = 0; i < size; i++)
        for(j = 0; j < size; j++)
            if(!sudoku[i][j])
                return 0;
    return 1;
}

//Function gives all different possible numbers those can be put on board such that board will be in valid
// state, and this is done by checking numbers already appeared in row, column and block
//Function to find various possible values at position (r, c)
//Returns no. of possible values at that position
//and those values in array a[]
int findPossibleValues(int sudoku[N][N], int size, int a[], int r, int c)
{
    int n = 0;
    int i,j;
    int s = (int)(sqrt(size));
    int b[N+1] = {0};

    //Note no.s appeared in current row
    for(i = 0; i < size; i++)
        b[sudoku[r][i]] = 1;

    //Note no.s appeared in current column
    for(i = 0; i < size; i++)
        b[sudoku[i][c]] = 1;

    //Note no.s appeared in current block
    r = (r/s)*s, c = (c/s)*s;
    
    for(i = r; i < (r+s); i++)
        for(j = c; j < (c+s); j++)
            b[sudoku[i][j]] = 1;

    //Fill array a[] with no.s unappeared in current row, column and block
    for(i = 1; i <= size; i++)
        if(!b[i])
            a[n++] = i;

    return n;
}

void sig_func(int sig_no)
{
	perror("SEGFAULT ");
	//pthread_exit(0);
}

void * f(void * arg){
	signal(SIGSEGV,sig_func);
	struct node * ar=(struct node *)arg;
	int t_id=ar->t_id;
	int p_id=ar->p_id;
	int no_of_processors = ar->size;
	int i,j,k;
	int count, source, random_dest, busy2idle = 0, some_tag = 1;
	MPI_Request request;
    MPI_Status status_busy, status_idle, st1;
    MPI_Datatype outtype;

	//number of items inside structure stackNode
	const int nitems = 4;

	//count of item of each type inside stackNode in order
	int blocklengths[4] = {1, 1, 1, 1};

	//data types present inside stackNode in order
	MPI_Datatype types[4] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT};

	//name of derived data type
	MPI_Datatype mpi_stackNode_type;

	//array to store starting address of each item inside stackNode
	MPI_Aint offsets[4];

	//offset of each item in stackNode with respect to base address of stackNode
	offsets[0] = offsetof(stackNode, i);
	offsets[1] = offsetof(stackNode, j);
	offsets[2] = offsetof(stackNode, value);
	offsets[3] = offsetof(stackNode, level);

	//create the new derived data type
	MPI_Type_create_struct(nitems, blocklengths, offsets, types, &mpi_stackNode_type);

	//commit the new data type
	MPI_Type_commit(&mpi_stackNode_type);

	int request_for_work_idle, ack_idle, request_for_work_busy, ack_busy, request_for_work_idle_2;
	int data_present_idle[2], board_received_idle[N*N], data_present_busy[2], board_busy[N*N];
	stackNode data_received_idle[N];
	MPI_Request sendReq;
	int sendFlag = 0;

	int sentOrNot = 0;
	srand(time(NULL) + p_id);
	if(t_id == 0)
	{
		while(!solved)
		{
			if (get_array_sum() == 0)
	    	{
				//printf("idle proc\n");
	    		// idle processor

				int checkTrue;
				MPI_Iprobe(MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &checkTrue, &st1);
				//printf("MAIN %d:%d\n", t_id, p_id);
				if(checkTrue==1)
				{
					MPI_Recv(&request_for_work_idle_2, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &st1);
					if(request_for_work_idle_2==0)
					{
						printf("%d:%d IDLE THREAD RCVD EXIT FROM %d\n", t_id, p_id, st1.MPI_SOURCE);
						solved = 1;
						break;
					}
					// send message indicating there is no data to send
                    data_present_idle[0] = 0;
                    data_present_idle[1] = 0;
	                source = st1.MPI_SOURCE;
                    
                    MPI_Send(data_present_idle, 2, MPI_INT, source, 2, MPI_COMM_WORLD);
				}
   			
    			// request a random processor for work
	    		//random_dest = 0;
	    		//random_dest = 0;
	    		request_for_work_idle = 1;
	    		
	    		if(sentOrNot == 0)
	    		{
	    			random_dest = rand() % no_of_processors;
		    		while(random_dest == p_id)
		    		{
		    			random_dest = rand() % no_of_processors;
		    		}
	    			printf("randomly selected : %d\n", random_dest);

	    			MPI_Issend(&request_for_work_idle, 1, MPI_INT, random_dest, 1, MPI_COMM_WORLD, &sendReq);
	    			printf("IDLE THREAD %d queries %d\n", p_id, random_dest);
	    			sentOrNot = 1;
	    		}

	    		if(sentOrNot == 1)
	    		{
	    			//Test if send has occured
	    			MPI_Test(&sendReq, &sendFlag, MPI_STATUS_IGNORE);
	    			if(sendFlag == 1)
	    			{
	    				sentOrNot = 0;
	    				printf("STARTED RECV\n");
	    				MPI_Recv(data_present_idle, 2, MPI_INT, random_dest, 2, MPI_COMM_WORLD, &status_idle);
	    				printf("FINISHED RECV\n");
			    		// check if the processor actually has work to donate
			    		if (data_present_idle[0] == 1)
			    		{
	    					//printf("%d GOT REPLY FOR WORK REQUEST FROM %d\n", p_id, random_dest);
			    			// receive stack contents
			    			MPI_Recv(data_received_idle, data_present_idle[1], mpi_stackNode_type, random_dest, 3, MPI_COMM_WORLD, &status_idle);
			    			
			    			printf("SOMEONE TOOK WORK in PROC %d\n", p_id);
			    			int g;
			    			for (g = 0; g < data_present_idle[1]; ++g)
			    			{
			    				shared_s = push_stack(shared_s, data_received_idle[g].i, data_received_idle[g].j, data_received_idle[g].value, data_received_idle[g].level);
			    			}
			    			// send ack
			    			ack_idle = 1;
			    			MPI_Send(&ack_idle, 1, MPI_INT, random_dest, 1, MPI_COMM_WORLD);
			    			
			    			// receive board
			    			MPI_Recv(&shared_sudoku[0][0], N*N, MPI_INT, random_dest, N*N, MPI_COMM_WORLD, &status_idle);

			    			/*for(i = 0; i < N; i++)
					    	{
					    		printf("\n");
					    		for(j = 0; j < N; j++)
					    		{
					    			printf("%d  ", shared_sudoku[i][j]);
					    		}
					    	}printf("\n");
					    	print_stack(shared_s);*/

			    			// set busy bit to 1
			    			pthread_mutex_lock(&mutex);
			    			permission = 1;
			    			pthread_mutex_unlock(&mutex);
			    			
			    			while(get_array_sum()==0)
			    			{

			    			}
			    			//break;
			    		}
			    		
			    		else
			    		{
			    			//choose another processor i.e. continue
			    		}
	    			}
	    			
	    		}
	    	}

	    	else
	    	{
	    		//printf("busy proc\n");
	    		//busy processor
		        
	            //MPI_Irecv(&request_for_work_busy, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);

	            while((!solved) && (get_array_sum() != 0))
	            {
		            int checkTrue;
					MPI_Iprobe(MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &checkTrue, &st1);
		            if(checkTrue == 1)
		            {
		            	pthread_mutex_lock(&mutex);		    
						MPI_Recv(&request_for_work_busy, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &st1);
						if(request_for_work_busy==0)
						{
							printf("%d:%d BUSY THREAD RCVD EXIT FROM %d\n", t_id, p_id, st1.MPI_SOURCE);
							solved = 1;
							pthread_mutex_unlock(&mutex);
							break;
						}
			            source = st1.MPI_SOURCE;
		                // get source
		                
			            printf("%d:%d BUSY THREAD RCVD WORK REQUEST FROM %d\n", t_id, p_id, st1.MPI_SOURCE);
		                // check sudoku[0][0] if -1 no data to send else send shared stack and shared board
		                if(shared_sudoku[0][0] == -1)
		                {
		                    // send message indicating there is no data to send
		                    data_present_busy[0] = 0;
		                    data_present_busy[1] = 0;

		                    MPI_Send(data_present_busy, 2, MPI_INT, source, 2, MPI_COMM_WORLD);
		                    pthread_mutex_unlock(&mutex);
		                }

		                else
		                {
		                    //check if error max limit N?
		                    stackNode temp[N];
		                    stack *temp2 = shared_s;
		                    k = 0;

		                    // populate contents of stack to be sent into an array
		                    while(temp2 != NULL)
		                    {
		                        temp[k].i = temp2->i;
		                        temp[k].j = temp2->j;
		                        temp[k].value = temp2->value;
		                        temp[k].level = temp2->level;
		                        k++;
		                        temp2 = temp2->prev;
		                    }

		                    data_present_busy[0] = 1;
		                    data_present_busy[1] = k;

		                    MPI_Send(data_present_busy, 2, MPI_INT, source, 2, MPI_COMM_WORLD);
		                    MPI_Send(temp, k, mpi_stackNode_type, source, 3, MPI_COMM_WORLD);
			                // receive ack
			                MPI_Recv(&ack_busy, 1, MPI_INT, source, 1, MPI_COMM_WORLD, &status_busy);
			                // variable name for board
			                MPI_Send(&shared_sudoku[0][0], N*N, MPI_INT, source, N*N, MPI_COMM_WORLD);

			                while(shared_s != NULL)
			                {
			                    shared_s = pop_stack(shared_s);
			                }
			                //set board to -1 indicating work has already been allotted
			                shared_sudoku[0][0] = -1;
			                //busy2idle = 2;
		            		pthread_mutex_unlock(&mutex);
		            		break;
		                }
		            }
		            else
		            {
		            	pthread_mutex_lock(&mutex);
		            	permission = 1;		            			     
		            	pthread_mutex_unlock(&mutex);
		            }
	            }

	            /*if (!solved && busy2idle != 2)
	            {
	            	busy2idle = 1;
	            }*/
	    	}
		}
		
	}
	else
	{
		int sudoku[N][N];
		stack * s = NULL,* bs = NULL,* recon_s = NULL;
		int no_top = 0, p_level = 0, curr_level = 0, break_flag, n;
		int i, j;
		int * a =(int *)malloc((N+1)*sizeof(int));
		if(t_id == 1)
		{
			if(p_id == 0)
			{
				FILE * fp;
				fp = fopen("s2.txt", "r");
				printf("Please enter Sudoku board:\n");
				if(fp == NULL)
				{
					perror("File error : ");
					exit(0);
					//TODO
				}

				for(i = 0; i < N; i++)
        			for(j = 0; j < N; j++)
            			fscanf(fp, "%d", &sudoku[i][j]);
            	fclose(fp);
				busy[t_id] = 1;
			}
		}
		while(!solved)
		{
			//printf("1\n");
			while(busy[t_id] && !solved)
			{
				//printf("%d %d ", p_id, t_id);
		        //print_stack(s);
		        //print_stack(recon_s);
				if(isFull(sudoku, N))
			    {
			    	printf("Sudoku\n");
			    	pthread_mutex_lock(&mutex);
			    	for(i = 0; i < N; i++)
			    	{
			    		printf("\n");
			    		for(j = 0; j < N; j++)
			    		{
			    			printf("%d  ", sudoku[i][j]);
			    			shared_sudoku[i][j] = sudoku[i][j];
			    		}
			    	}printf("\n");
			    	solved = 1;
			    	flagWeDoneIt = 1;
			    	pthread_mutex_unlock(&mutex);
			    	
			    	break;
			    }
			    //printf("@@reached here&&\n");


				break_flag = 0;
		    	for(i = 0; i < N; i++)
		    	{
		        	for(j = 0; j < N; j++)
		            {
		            	if(!sudoku[i][j])
		            	{
		                	break_flag = 1;
		                	break;
		            	}
		            }

		        	if(break_flag)
		            	break;
		    	}
		    	//printf("%d@@reached here&&\n",t_id);
		    	n = findPossibleValues(sudoku, N, a, i, j);
		    	int l;
		    	for(l = 0; l < n; l++)
		    	{
		        	s = push_stack(s,i,j,a[l],curr_level);
		        	
		        	if(s->prev == NULL)
		        	{
		        		bs = s;
		        	}
		    	}
		    	//printf("%d@@reached here&&\n",t_id);
		    	pthread_mutex_lock(&mutex);
		    	if(s != NULL && s != bs && shared_sudoku[0][0] == -1)
		    	{
		    		for(i = 0; i < N; i++)
		    		{
		    			for(j = 0;j < N; j++)
		    			{
		    				shared_sudoku[i][j] = sudoku[i][j];
		    			}
		    		}

		    		//push in shared stack;
		    		stack * temp;
		    		temp = bs;
		    		no_top = 1, p_level = bs->level;
		    		temp = bs->next;

		    		while(temp != NULL)
		    		{
		    			if(temp->level!=p_level)
		    			{
		    				break;
		    			}

		    			no_top++;
		    			temp = temp->next;
		    		}

		    		no_top = (no_top+1)/2;

		    		while(no_top--)
		    		{
		    			shared_s = push_stack(shared_s,bs->i,bs->j,bs->value,bs->level);
		    			bs = bs->next;
		    			free(bs->prev);
		    			bs->prev = NULL;
		    		}
		    		
		    		//reconstruct top level sudoku board
		    		temp = recon_s;
		    		i = s->level-p_level;

		    		while(temp != NULL && i--)
		    		{
		    			shared_sudoku[temp->i][temp->j] = 0;
		    			temp = temp->prev;
		    		}
				}
				//printf("**%d@@reached here&&\n",t_id);
				//permission=1;
				pthread_mutex_unlock(&mutex);
				//printf("*%d@@reached here&&\n",t_id);
			    if(n == 0 && s != NULL)
			    {
			    	while(recon_s != NULL && recon_s->level >= s->level)
			    	{
			    		sudoku[recon_s->i][recon_s->j] = 0;
			    		recon_s = pop_stack(recon_s);
			    	}
				}

				else
				{
					curr_level++;
				}

				if(s != NULL)
				{
					sudoku[s->i][s->j] = s->value;
					recon_s = push_stack(recon_s, s->i, s->j, s->value, s->level);
					curr_level = s->level + 1;
					s = pop_stack(s);
				}

				else
				{
					bs = NULL;
					while(recon_s != NULL)
					{
						recon_s = pop_stack(recon_s);
					}
			    	
			    	pthread_mutex_lock(&mutex);
			    	busy[t_id] = 0;
			    	pthread_mutex_unlock(&mutex);
			    }
			}
			if(!busy[t_id] && !solved)
			{
				//printf("%d im bored\n",t_id);
			}
			while(!busy[t_id] && !solved)
			{
				pthread_mutex_lock(&mutex);
				if(permission == 1 && !solved)
				{
					//printf("HIHI\n");
					if(shared_sudoku[0][0] != -1)
					{
						//fill up local stack;
						while(shared_s != NULL)
						{
			    			s = push_stack(s, shared_s->i, shared_s->j, shared_s->value, shared_s->level);
			    			
			    			if(s->prev == NULL)
			    			{
			    				bs = s;
			    			}
			    			
			    			shared_s = pop_stack(shared_s);
			    		}

						for(i = 0; i < N; i++)
						{
				    		for(j = 0;j < N; j++)
				    		{
				    			sudoku[i][j] = shared_sudoku[i][j];
				    		}
				    	}

				    	/*if(s == NULL)
				    	{
				    		
				    	}*/

				    	sudoku[s->i][s->j] = s->value;
				    	recon_s = push_stack(recon_s, s->i, s->j, s->value, s->level);
				    	curr_level = s->level+1;
				    	s = pop_stack(s);

				    	shared_sudoku[0][0] = -1;
				    	busy[t_id] = 1;
				    	//printf("GOT WORK %d:%d\n", t_id, p_id);
				    	permission = 0;
					}
				}
				pthread_mutex_unlock(&mutex);
			}
		}
	}
	printf("EXITING %d:%d\n", t_id, p_id);
	solved = 1;
	/*if(t_id==0)
	{
	}*/
	if(t_id == 0)
	{
		if(flagWeDoneIt == 1)
		{
			//Send the info to all processors
	    	MPI_Abort(MPI_COMM_WORLD, 1);
	    	int done = 0, g;
	    	for (g = 0; g < no_of_processors; ++g)
	    	{
	    		if(g != p_id)
	    		{
	    			printf("%d:%d SENT EXIT to %d\n", t_id, p_id, g);
	    			MPI_Ssend(&done, 1, MPI_INT, g, 1, MPI_COMM_WORLD);
	    		}	
	    	}
			printf("%d DONE SENDING EXIT MSGS\n", p_id);
		}
		/*else
		{
			while(1)
			{
				int checkTrue;
				MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &checkTrue, &st1);
				if(checkTrue==1)
				{
					MPI_Recv(&request_for_work_idle, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st1);
					//printf("EXITED PROC %d SEND EXIT MSG TO %d\n", p_id, st1.MPI_SOURCE);
					
					// send message indicating there is no data to send
		            data_present_idle[0] = 0;
		            data_present_idle[1] = 0;
		            source = st1.MPI_SOURCE;
		            
		            MPI_Send(data_present_idle, 2, MPI_INT, source, some_tag, MPI_COMM_WORLD);
				}
			}
		}*/
	}
	pthread_exit(0);
}

//Function to solve the sudoku board
//Gives solved_flag as true if at least one solution is possible, false otherwise
//Gives multiple solutions with corresponding soultion no.
//Returns when user does not want more solutions

// This is our key function which solves the board
// Initially it checks if the board is full now
// If board is not full, then it finds the first position/cell which is vacant.
// Then it finds all possible values at that position
// Tries all these possible values in that cell
// For each trial/guess/choice, it tries to solve the updated board
// If no choice come out to be correct one for that vacant place, then definitely some previous choices are
// wrong
// Hence it backtracks and tries to correct the previous choices
/*void SolveSudoku(int sudoku[MAX][MAX], int size, int &solution_num, bool &solved_flag, bool &enough)
{
    int i,j, a[MAX+1]={0}, n=0;

    if(enough) //true if user does not want more solutions
        return;

    if(isFull(sudoku, size))    //true if now sudoku board is solved completely
    {

        if(!solved_flag)
            cout<<"Sudoku Solved Successfully!"<<endl;
        solved_flag = 1;

        displaySolution(sudoku, size);

        if(more != '1')
            enough = true;
        return;
    }

    int break_flag = 0;
    for(i=0;i<size;i++)
    {
        for(j=0;j<size;j++)
            if(!sudoku[i][j])
            {
                break_flag = 1;
                break;
            }
        if(break_flag)
            break;
    }

    //check possibilities at that vacant place
    n = findPossibleValues(sudoku, size, a, i, j);
    for(int l=0;l<n;l++)
    {
        //put value at vacant place
        sudoku[i][j]=a[l];
        //now solve the updated board
        SolveSudoku(sudoku, size, solution_num, solved_flag, enough);
    }


    sudoku[i][j]=0;
}*/


int main(int argc, char *argv[])
{
	int x = 10, i;
	int l[N*N] = {-1};

	MPI_Init(NULL, NULL);

	pthread_t t[NO_T];
	struct node ax[NO_T];
	
	int rank = 0, size;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	if (rank == 0)
	{
		busy[1] = 1;
	}

	for(i = 0; i < NO_T; i++)
	{
		ax[i].t_id = i;
		ax[i].p_id = rank;
		ax[i].size = size;
		pthread_create(&t[i], NULL, f, (void *)&ax[i]);
	}

	for(i = 0; i < NO_T; i++)
	{
		pthread_join(t[i], NULL);
	}

	MPI_Finalize();
	return 0;
}
