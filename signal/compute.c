/*Author: KAI WANG*/
#include"head.h"
seg_t* seg;
int msqid, entry;
void handler(int signo);
pid_t tpid;
// one argument indicating the first number to test
int main(int argc, char** argv){
	int  shmid, i, n, start, sum;
	msg_t snd, rcv;
	pid_t pid;
	sigset_t mask;
	struct sigaction action;

	tpid= pid= getpid();
	
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGQUIT);
	action.sa_mask= mask;
	action.sa_flags= 0;
	action.sa_handler= handler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGHUP, &action, NULL);

	bit[0]=1;
	for(i=1; i<8; i++){
		bit[i]= bit[i-1]<<1;
	}

	printf("compute process %d start: \n", getpid());
	// check parameter
	if(argc==2){
		start= n= atoi(argv[1]);
		if(n<0){
			printf("input is negative!!\n");
			exit(EXIT_FAILURE);
		}
		printf("start number: %d\n", n);
	}else{
		perror("input the start number");
		exit(EXIT_FAILURE);
	}
	// get shared memory and attach, dont create
	if((shmid= shmget(KEY, sizeof(seg_t), 0))==-1){
		perror("shmget");
		exit(EXIT_FAILURE);
	}
	if((seg=(seg_t*) shmat(shmid, 0, 0))==(seg_t*)-1){
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	// get message queue, dont create
	if((msqid= msgget(KEY, 0))==-1){
		perror("msgget");
		exit(EXIT_FAILURE);
	}
	printf("message queue is %d\n", msqid);

	
	// register and get an entry in the process table
	snd.mtarget= MANAGE_PORT;
	snd.mtext.type= getent;
	snd.mtext.sender= pid;
	
	if(msgsnd(msqid, &snd, sizeof(msg_content), 0)==-1){
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
	
	if(msgrcv(msqid, &rcv, sizeof(msg_content), pid, 0)== -1){
		perror("msgrcv");
		exit(EXIT_FAILURE);
	}
	assert(rcv.mtext.type== getent);
	entry= rcv.mtext.data.entry;
	seg->proc[entry].pid= pid;
	printf("entry: %d is registered for process %d with manage %d\n", entry, seg->proc[entry].pid, rcv.mtext.sender);

	do {
		int ind= n/8;
		int rem= n%8;
		if((n!=0) && ((seg->bitmap[ind] & bit[rem])==0)){
			//update bitmap
 			seg->bitmap[ind]= seg->bitmap[ind] | bit[rem];

			sum=0;
			for (i=1;i<n;i++)
				if (!(n%i)) sum+=i;
		    
			if (sum==n){
				snd.mtarget= MANAGE_PORT;
				snd.mtext.type= pfnum;
				snd.mtext.sender= pid;
				snd.mtext.data.num= n;
				if(msgsnd(msqid, &snd, sizeof(msg_content), 0)== -1){
					perror("msgsnd");
					exit(EXIT_FAILURE);
				}

				seg->proc[entry].found++;
				printf("find %dth perfect number: %d\n",seg->proc[entry].found, n);
			}
			seg->proc[entry].tested++;
			//printf("%d\n", seg->proc[entry].tested);
		}else{
			seg->proc[entry].skipped++;
		}
		if(n==50)
			getchar();
		n= (n+1)%NUM2TEST;		
	}while(n!= start);
	
	// tell manage that compute is about to end, send entry number
	snd.mtarget= MANAGE_PORT;
	snd.mtext.type= end;
	snd.mtext.sender= pid;
	//snd.mtext.data.entry= entry;		// replace this line with following commented if requiring compute clear fields other than pid

	snd.mtext.data.num= seg->proc[entry].tested;
	seg->proc[entry].tested= seg->proc[entry].found= seg->proc[entry].skipped= 0;
	/**/
	if(shmdt((void*)seg)== -1){
		perror("shmdt");
		exit(EXIT_FAILURE);
	}
	if(msgsnd(msqid, &snd, sizeof(msg_content), 0)==-1){
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
	
	return 0;
}

void handler(int signo){
	printf("\nenter compute handler\n");
	print_sig(signo);
	
	// tell manage to record tested and clear entry
	msg_t snd;
	snd.mtarget= MANAGE_PORT;
	snd.mtext.type= end;
	snd.mtext.sender= getpid();
	assert(getpid()==tpid);
	//snd.mtext.data.entry= entry; 	// replace this with following commented if requiring compute clear fields other than pid
	snd.mtext.data.num= seg->proc[entry].tested;
	seg->proc[entry].tested= seg->proc[entry].found= seg->proc[entry].skipped= 0;
	/**/
	if(shmdt((void*)seg)== -1){
		perror("shmdt");
		exit(EXIT_FAILURE);
	}
	if(msgsnd(msqid, &snd, sizeof(msg_content), 0)==-1){
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
	
	exit(EXIT_SUCCESS);
}
