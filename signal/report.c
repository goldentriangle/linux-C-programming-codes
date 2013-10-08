/*Author: KAI WANG*/
#include"head.h"
void handler(int signo);

int flag;
seg_t* seg;
pid_t mngpid;

int main(int argc, char** argv){	
	int shmid, msqid, found, tested, i, j;
	
	msg_t snd, rcv;
	sigset_t mask;
	struct sigaction action;

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

	printf("***************************\nreport process %d begins:\n", getpid());
	if(argc>2){
		fprintf(stderr, "number of arguments exceeds 2\n");
		exit(EXIT_FAILURE);
	}
	if(argc==2){
		
		if(strcmp(argv[1], "-k")==0){
			flag= true;
		}else{
			fprintf(stderr, "unexpected argument!! the argument can only be -k\n");
			exit(EXIT_FAILURE);	
		}
	}else{
		flag= false;	
	}

	if((shmid= shmget(KEY, sizeof(seg_t), 0))== -1){
		perror("shmget");
		exit(EXIT_FAILURE);
	}
	if((seg=shmat(shmid,0,0))== (seg_t*)-1){
		perror("shmat");	
		exit(EXIT_FAILURE);
	}
	
	if((msqid= msgget(KEY, 0))== -1){
		perror("msgget");	
		exit(EXIT_FAILURE);
	}
	// get pid of manage
	snd.mtarget= MANAGE_PORT;
	snd.mtext.type= mng_pid;
	snd.mtext.sender= getpid();
	
	if(msgsnd(msqid, &snd, sizeof(msg_content), 0)== -1){
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
	if(msgrcv(msqid, &rcv, sizeof(msg_content), getpid(), 0)== -1){
		perror("msgrcv");
		exit(EXIT_FAILURE);
	}
	mngpid= rcv.mtext.sender;


	snd.mtarget= MANAGE_PORT;
	snd.mtext.type= report_test;
	snd.mtext.sender= getpid();
	if(msgsnd(msqid, &snd, sizeof(msg_content), 0)== -1){
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
	if(msgrcv(msqid, &rcv, sizeof(msg_content), (long)getpid(), 0)== -1){
		perror("msgrcv");
		exit(EXIT_FAILURE);
	}
	printf("number tested in total: %d\n", rcv.mtext.data.num);  
/**/

	for(i=0; i< NUM_PF; i++){
		if(seg->pf[i]==0){
			break;
		}
	}
	found= i;
	printf("perfect number found totally: %d\n", found);
	for(i=0; i<found; i++)
		printf("%d\t", seg->pf[i]);
	printf("\n");
	
	printf("process info:\n");
	printf("%-12s%-12s%-12s%-12s\n", "pid", "tested", "skipped", "found");
	for(i=0; i< NUM_PROC; i++){			
		if(seg->proc[i].pid!= 0){
			printf("%-12d%-12d%-12d%-12d\n", seg->proc[i].pid, seg->proc[i].tested,seg->proc[i].skipped, seg->proc[i].found);
		}
	}
	printf("\n");
	
	if(shmdt((void*)seg)== -1){
		perror("shmdt");	
		exit(EXIT_FAILURE);
	}
	if(flag== true){
		printf("kill manage: %d\n", mngpid);
		kill(mngpid, SIGHUP);
	}
	return 0;
}
void handler(int signo){
	printf("\nenter report handler\n");
	print_sig(signo);
	if(shmdt((void*)seg)== -1){
		perror("shmdt");	
		exit(EXIT_FAILURE);
	}
	if(flag== true){
		kill(mngpid, SIGHUP);
	}
	exit(EXIT_SUCCESS);
}

