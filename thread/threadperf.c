/*Author: KAI WANG*/
#include<stdio.h>
#include<unistd.h>
#include<pthread.h>
#include<errno.h>
#include<stdlib.h>
#include<ctype.h>
#include<string.h>
#include<sys/times.h>
#include<time.h>
#include<sys/resource.h>

#define START 0
#define IDLE 1
#define RESTART 2
#define WAIT 3
#define REPORT 4
#define QUIT 5
#define EMPTY 6
#define TRUE 1
#define FALSE 0
#define MAXNPF 10
#define IS_RUNNING 0
#define IS_IDLE 1
#define IS_OVER 2
#define CMDLENTH 128

typedef struct thread_t{
	int NO;
	pthread_t id;
	int state;
	int tested, skipped,found, blockNO;
	int pf[MAXNPF];
	struct thread_t* next;
}thread_t;

int min(int, int);
int getcmd(char*, char**, ssize_t);
void* perfect(void*);
thread_t* find_thread(int, thread_t*);
void init_thread_t(thread_t* th, int threadNO, int blockNO);
void quit_proc();
//global variables
pthread_mutex_t mt;
pthread_cond_t cond;
pthread_attr_t attr;
//only changed by main thread
char bit[8];
char** cmd;
int MAXN, BLOCK, NBLOCK,  bmbytes;			
thread_t* head;
thread_t* end;
time_t begtime;
// need lock
char* bitmap;
int NPF;
int pf[MAXNPF];
int NTHREAD;

int main(int argc, char** argv){
	int i;
	int thread_counter= 1;
	
	NTHREAD= 0;
	head= NULL;
	end= NULL;
	cmd= (char**)malloc(sizeof(char*)*2);
	cmd[0]= (char*) malloc(sizeof(char)*CMDLENTH);
	cmd[1]= (char*) malloc(sizeof(char)*CMDLENTH);
	char* line= NULL;
	size_t len= 0;
	ssize_t read=0;
	time(&begtime);
	NPF= 0;
	for(i=0; i<MAXNPF; i++){
		pf[i]= -1;
	}

	bit[0]=1;
	for(i=1; i<8; i++){
		bit[i]= bit[i-1]<<1;
	}
	
	//init thread attribute object
	
	if(pthread_attr_init(&attr)!= 0){
		perror("pthread_attr_init");
		exit(EXIT_FAILURE);
	}
	if(pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)!=0){
		perror("pthread_attr_setscope");
		exit(EXIT_FAILURE);
	}
	//init mutex
	if(pthread_mutex_init(&mt, NULL)!=0){
		perror("pthread_mutex_init");
		exit(EXIT_FAILURE);
	}
	//init condition
	if(pthread_cond_init(&cond, NULL)!=0){
		perror("pthread_cond_init");
		exit(EXIT_FAILURE);
	}

	if(argc!=3){
		fprintf(stderr, "wrong number of arguments!\n");
		fprintf(stderr, "FORMAT: threadperf MAXN BLOCK\n");
		exit(EXIT_FAILURE);	
	}
	MAXN= atoi(argv[1]);
	BLOCK= atoi(argv[2]);
	NBLOCK= MAXN/BLOCK + (MAXN%BLOCK==0 ?0:1);
	bmbytes= NBLOCK/8+ (NBLOCK%8==0? 0:1);

	if((bitmap=(char*)malloc(bmbytes))==NULL){
		fprintf(stderr, "malloc failure\n");	
		exit(EXIT_FAILURE);
	}
	memset(bitmap, 0, bmbytes);

	while(1){
		read=getline(&line, &len, stdin);

		int CMD= getcmd(line, cmd, read);
		int k; 
		thread_t* threadptr;
		switch(CMD){
			case START:
				k= atoi(cmd[1]);
				
				if((threadptr= (thread_t*)malloc(sizeof(thread_t)))== NULL){
					fprintf(stderr, "malloc failure");
					exit(EXIT_FAILURE);
				}
				init_thread_t(threadptr, thread_counter, k/BLOCK);
				// adjust thread list
				if( thread_counter==1){
					head= end= threadptr;
				}else{
					end->next= threadptr;
					end= threadptr;	
				}
				// create thread
				printf("thread %d started...\n", thread_counter++);
			 
				pthread_create(&(threadptr->id),&attr, perfect, (void*)threadptr);
				break;
			case IDLE:
				k= atoi(cmd[1]);
				if((threadptr=find_thread(k, head))!= (thread_t*)NULL){
					threadptr->state= IS_IDLE;
				}else{
					fprintf(stderr, "thread %d doesnt exist!\n", k);
					exit(EXIT_FAILURE);	
				}
				break;
			case RESTART:
				k= atoi(cmd[1]);
				if((threadptr=find_thread(k, head))!= (thread_t*)NULL){
					threadptr->state= IS_RUNNING;
					// may lock first
					printf("restarting thread %d ...\n", threadptr->NO);
					pthread_cond_broadcast(&cond);
				}else{
					fprintf(stderr, "thread %d doesnt exist!\n", k);
					exit(EXIT_FAILURE);	
				}
				break;
			case WAIT:
				k= atoi(cmd[1]);
				sleep(k);
				break;
			case REPORT:
				printf("***************report****************\n");
				threadptr= head;
				while(threadptr!= NULL){
					
					printf("thread %d:\n", threadptr->NO);
					printf("perfect numbers found:\n");
					for(i=0; threadptr->pf[i]!= -1; i++){
						printf("%d\t", threadptr->pf[i]);
					}		
					printf("\n");		
					printf("number tested: %d\n", threadptr->tested);
					printf("number skipped: %d\n", threadptr->skipped);
					printf("current working block is: %d \n", threadptr->blockNO);
					switch(threadptr->state){
						case IS_IDLE:
							printf("thread %d is idle\n", threadptr->NO);
							break;
						case IS_RUNNING:
							printf("thread %d is active\n", threadptr->NO);
							break;
						case IS_OVER:
							printf("thread %d is terminated\n", threadptr->NO);
							break;
						default:
							fprintf(stderr, "wrong thread state\n");
							exit(EXIT_FAILURE);
							break;
					}	
					printf("\n");
					threadptr= threadptr->next;
				}
				printf("*************************************\n");
				break;
			case QUIT:
TO_QUIT:
				quit_proc();
				break;
			case EMPTY:
				break;
			default:
				fprintf(stderr, "invalid command, input again\n");
				break;	
		}
	}
	return 0;
}

void init_thread_t(thread_t* th, int threadNO, int blockNO){
	int i;
	th->NO= threadNO;
	th->blockNO= blockNO;

	th->state= IS_RUNNING;
	th->tested= th->skipped= th->found= 0;
	th->next= (thread_t*)NULL;
	
	for(i=0; i<MAXNPF; i++){
		th->pf[i]=-1;
	}
}

int getcmd(char* line, char** cmd, ssize_t size){
	memset(cmd[0], '\0', CMDLENTH);
	memset(cmd[1], '\0', CMDLENTH);
	ssize_t i;
	int cnt=0;
	int state= 0;
	int end= FALSE;
	for( i=0;i< size && end==FALSE ;i++){
		switch(state){
			case 0:
				// filter preceding space
				if(isspace((int)line[i])==0){
					state= 1;
					cmd[0][cnt++]= line[i];
				}
				break;
			case 1:
				// first word
				if(isspace((int)line[i])==0){
					cmd[0][cnt++]= line[i];
				}else{
					state=2;
					cnt=0;
				}
				break;
			case 2:
				// filter space between
				if(isspace((int)line[i])==0){
					state= 3;
					cmd[1][cnt++]= line[i];
				}
				break;
			case 3:
				// second word
				if(isspace((int)line[i])==0){
					cmd[1][cnt++]= line[i];
				}else{
					end= TRUE;
				}
				break;
			default:
				break;
		}
	}
	if(strlen(cmd[0])!=0){
		printf("%s\t: %s\n", cmd[0], cmd[1]);
	}else{
		return EMPTY;
	}

	if(strcmp(cmd[0], "start")==0){
		return START;
	}else if(strcmp(cmd[0], "idle")==0){
		return IDLE;
	}else if(strcmp(cmd[0], "restart")==0){
		return RESTART;	
	}else if(strcmp(cmd[0], "wait")==0){
		return WAIT;
	}else if(strcmp(cmd[0], "report")==0){
		return REPORT;
	}else if(strcmp(cmd[0], "quit")==0){
		return QUIT;
	}else{
		return -1;
	} 
}

thread_t* find_thread(int k, thread_t* head){
	thread_t* threadptr= head;
	while(threadptr!=NULL){
		if(threadptr->NO==k){
			return threadptr;
		}
		threadptr= threadptr->next;
	}
	return (thread_t*)NULL;
}
int min(int a, int b){
	if(a<b){
		return a;
	}
	return b;
}
void quit_proc(){
	int i;
	thread_t* threadptr;
	time_t endtime;
	struct rusage usage;
	printf("****process is going to terminate****\nperfect numbers found:\n");
	for(i=0; i< NPF; i++){
		printf("%d\t", pf[i]);
	}
	
	int total_tested=0;
	threadptr= head;
	while(threadptr!= NULL){
		total_tested+=threadptr->tested;
		threadptr= threadptr->next;
	}		
	printf("\ntotal numbers tested: %d\n", total_tested);			// consider number 0 is skipped
	
	if(getrusage(RUSAGE_SELF, &usage)!=0){
		perror("getrusage");
		exit(EXIT_FAILURE);
	}
	double cputime= usage.ru_utime.tv_sec+ usage.ru_stime.tv_sec+ 
		(usage.ru_utime.tv_usec+ usage.ru_stime.tv_usec)/1000000.0;
	printf("total CPU time: %f seconds\n", cputime);
	time(&endtime);
	printf("elapsed time for the process: %ld seconds\n", endtime-begtime);

	threadptr= head;
	thread_t* t;
	while(threadptr!= NULL){
		t=  threadptr->next;
		free(threadptr);
		threadptr= t;
	}
	free(cmd[0]);
	free(cmd[1]);
	free(cmd);
	free(bitmap);
	
	if(pthread_attr_destroy(&attr)!=0){
		perror("pthread_mutexattr_destroy");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_destroy(&mt)!=0){
		perror("pthread_mutex_destroy");
		exit(EXIT_FAILURE);
	}
	if(pthread_cond_destroy(&cond)!=0){
		perror("pthread_cond_destroy");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
void* perfect(void* param){
	pthread_mutex_lock(&mt);
	NTHREAD++;
	pthread_mutex_unlock(&mt);

	thread_t* p= (thread_t*) param;
	int startBlock=  p->blockNO;
	int count=0;
	while(1){
		int ind= p->blockNO/8;
		int rem= p->blockNO%8;
		
		// lock to check bitmap finding a block to work on
		pthread_mutex_lock(&mt);
		while((bitmap[ind] & bit[rem])!=0 && count< NBLOCK){
			if(p->blockNO==(NBLOCK-1)){
				p->skipped+= MAXN%BLOCK;
			}else{
				p->skipped+= BLOCK;
			}
			p->blockNO= (p->blockNO+1)%NBLOCK;
			ind= p->blockNO/8;
			rem= p->blockNO%8;
			count++;
		}
		if(count==NBLOCK){
			//no remaining block to be check
			p->state= IS_OVER;
			NTHREAD--;
			if(NTHREAD==0){
				pthread_mutex_unlock(&mt);
				printf("thread %d is about to end\n", p->NO);
				quit_proc();
			}
			pthread_mutex_unlock(&mt);
			printf("thread %d is about to end\n", p->NO);
			pthread_exit(EXIT_SUCCESS);
		}else{
			//update bitmap
			bitmap[ind]= bitmap[ind] | bit[rem];	
			count++;
		}
		pthread_mutex_unlock(&mt);

		// do work here	
		int startNum = p->blockNO *BLOCK;
		int endNum= min(startNum+BLOCK, MAXN);
		int n;
		for (n=startNum; n< endNum; n++) {
			if(n!=0){
				int sum=0;
				int i;
				for (i=1;i<n;i++)
					if (!(n%i)) sum+=i;
			    	p->tested++;
				if (sum==n){
					p->pf[p->found]= n;
					p->found++;
					// lock to update global array pf
					pthread_mutex_lock(&mt);
					int ii;
					int exist= FALSE;
					for(ii=0; ii<NPF;ii++){
						if(pf[ii]==n){
							exist= TRUE;
							break;
						}
					}
					if(exist== FALSE){
						pf[NPF]= n;		
						NPF++;			//if race condition, print one less perfect number at worst
					}else{
						fprintf(stderr,"perfect number %d already found\n", n);	
						pthread_exit(NULL);
					}
					pthread_mutex_unlock(&mt);
				}
			}else{
				p->skipped++;
			}
			if(p->state==IS_IDLE){
				pthread_mutex_lock(&mt);
				do{
					printf("thread %d is suspended\n", p->NO);
					pthread_cond_wait(&cond, &mt);
					printf("thread %d is awaken\n", p->NO);
				}while(p->state==IS_IDLE);
				printf("thread %d is restarted\n", p->NO);
				pthread_mutex_unlock(&mt);
			}
		}
		p->blockNO= (p->blockNO+1)%NBLOCK;
	}
}
