/*Author: KAI WANG*/
#include"head.h"

int fds;
int n;		// number currently testing
int finished;	// indicating whether finished testing n
struct sockaddr_in server_addr;

void sighandler(int);

int main(int argc,char** argv){
	int startNum, endNum, range_size,retval, counter;
	message_t msgsnd, msgrcv;
	time_t t1, t2;
	struct linger lg;

	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	nfds_t nfds;
	struct pollfd pfd;
	
	//xdr
	XDR handle;
	char done;
	FILE *stream;

	if(argc!= 4){
		fprintf(stderr, "usage: %s server name, port number, start number!\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sigset_t mask;
	struct sigaction action;
	
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGQUIT);
	action.sa_mask= mask;
	action.sa_flags= 0;
	action.sa_handler= sighandler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGHUP, &action, NULL);
	
	// set up connection
	if((fds= socket(AF_INET, SOCK_STREAM, 0))== -1){
		perror("socket");
		exit(EXIT_FAILURE);
	}

	lg.l_onoff = 1;
	lg.l_linger = 5;
        setsockopt(fds, SOL_SOCKET, SO_LINGER, (void *)&lg, sizeof(struct linger));

	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family= AF_INET;
	server_addr.sin_port= htons(atoi(argv[2]));
	
	if(( retval= inet_aton(argv[1],& server_addr.sin_addr))== 0){
		struct addrinfo *res, *p;
		if((retval = getaddrinfo(argv[1], NULL, &hints, &res) ) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval) );
			return 2;
		}

		//for(p = res; p != NULL; p = p->ai_next) {
			//memcpy(&server_addr.sin_addr, &p->ai_addr->sa_data[2], sizeof(server_addr.sin_addr));
			memcpy(&server_addr.sin_addr, &res->ai_addr->sa_data[2], sizeof(server_addr.sin_addr));
			char *ipstr = inet_ntoa(server_addr.sin_addr);
			printf("sIPv4: %s\n", ipstr);
		//}
		freeaddrinfo(res);
	}
	
	if((retval= connect(fds, (struct sockaddr*)&server_addr, sizeof(server_addr)))< 0){
		perror("connect");
		exit(EXIT_FAILURE);
	}
	

	msgsnd.type= STARTUP;
	msgsnd.start= msgsnd.end= -1;
	msgsnd.data= atoi(argv[3]);
	msgsnd.id= getpid();
	msgsnd.tm= (time_t)NULL;
	if(gethostname(msgsnd.hostname, HOSTNAMELEN)){
		perror("gethostname");	
		exit(EXIT_FAILURE);
	}
	
	do{	
		//request a range to compute in
		if(my_send(fds, (void*)&msgsnd, sizeof(message_t), 0)==-1){
			perror("send");
			close(fds);
			exit(EXIT_FAILURE);
		}
		//receive range information
		if(my_recv(fds, (void*)&msgrcv, sizeof(message_t), 0)==-1){
			perror("recv");
			close(fds);
			exit(EXIT_FAILURE);
		}
		//no more work
		if(msgrcv.start== -1 && msgrcv.end== -1){
			printf("no more computing work. Done\n");
			close(fds);
			return 0;
		}

		startNum= msgrcv.start;
		endNum= msgrcv.end;
		range_size= range_size2(startNum, endNum);
		
		printf("Proc %d starting to computing [ %d %d ], range size: %d\n", getpid(), startNum, endNum, range_size);
		time(&t1);

		counter= 0;
		n= startNum;
		while(counter< range_size){
			finished= FALSE;
			if(n!=0){
				int sum=0;
				int i;
				for (i=1;i<n;i++){
					if (!(n%i)){
						sum+=i;
					}
			    	}
				if (sum==n){
					printf("find perfect number: %d\n", n);
					msgsnd.type= PFFOUND;
					msgsnd.data= n;
					if(my_send(fds, (void*)&msgsnd, sizeof(struct message_t), 0)== -1){
						exit(EXIT_FAILURE);
					}
				}
			}
			finished= TRUE;

			//poll after testing each number
			pfd.fd = fds;
			pfd.events = POLLIN;
			nfds=1;
			if ((retval= poll(&pfd, nfds,1))== -1){
				perror ("poll");
				exit(EXIT_FAILURE);
			}
			if(retval>0){
				if(my_recv(fds, (void*)&msgrcv, sizeof(message_t), 0)==-1){
					close(fds);
					exit(EXIT_FAILURE);
				}
				if(msgrcv.type!= REPORT && msgrcv.type!= MKILL){
					fprintf(stderr, "unexpected message type %d\n",msgrcv.type );
					close(fds);
					exit(EXIT_FAILURE);
				}
				msgsnd.type= CREPORT;
				msgsnd.id= getpid();
				msgsnd.start=startNum;
				msgsnd.end= msgsnd.data= n;			//last number tested	
				
				if(my_send(fds, (void*)&msgsnd, sizeof(message_t), 0)==-1){
					close(fds);
					exit(EXIT_FAILURE);
				}
				
				if(msgrcv.type== RKILL){
					//wait for confirmation the reported info is received
					if(my_recv(fds, (void*)&msgrcv, sizeof(message_t), 0)==-1){
						fprintf(stderr, "my_recv");
						close(fds);
						exit(EXIT_FAILURE);
					}
					close(fds);
					exit(EXIT_SUCCESS);
				}else if(msgrcv.type== MKILL){
					printf("last number tested: %d\n", n);
					close(fds);
					return n;
				}
			}
			
			n= module(n+1);
			counter++;
		}
		
		time(&t2);
		t2= t2- t1+1;
		printf("time: %lu\n", t2);
		// request next range
		msgsnd.type= NEXTRANGE;
		msgsnd.data= -1;			//set to -1
		msgsnd.tm= t2;				//last running time
		
	}while(1);

	return 0;
}

void sighandler(int sig){
	message_t msgsnd, msgrcv;
	printf("\ncompute get signal ");
	print_sig(sig);
	
	msgsnd.type= CEND;
	msgsnd.id= getpid();
	msgsnd.data= (finished= TRUE? n: n-1);
	
	if(my_send(fds, &msgsnd, sizeof(message_t), 0)== -1){
		fprintf(stderr, "my_send\n");
		close(fds);	
		exit(EXIT_FAILURE);
	}
	if(my_recv(fds, &msgrcv, sizeof(message_t), 0)== -1){
		fprintf(stderr, "my_recv\n");
		close(fds);	
		exit(EXIT_FAILURE);
	}
	if(msgrcv.type!= CEND){
		fprintf(stderr, "unexpected type\n");
		close(fds);	
		exit(EXIT_FAILURE);
	}
	close(fds);
	exit(EXIT_SUCCESS);
}
