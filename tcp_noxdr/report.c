/*Author: KAI WANG*/
#include"head.h"

int fds;
int flag= FALSE;
struct  sockaddr_in server_addr;
void handler(int);

int main(int argc,char** argv){
        int i, j, retval, nhost;
	message_t req;                    
	struct linger lg;
	hostinfo_t hinfo;
	procinfo_t pinfo;

	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

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

	if(argc==4){
		if(strcmp(argv[3], "-k")==0){
			flag= TRUE;
		}else{
			fprintf(stderr, "unexpected argument!! the third argument can only be -k\n");
			exit(EXIT_FAILURE);	
		}
	}else if(argc!= 3){
		fprintf(stderr, "usage: server portnumber [-k]!\n");
		exit(EXIT_FAILURE);
	}

	if ((fds = socket (AF_INET, SOCK_STREAM, 0)) == -1){
		perror( "socket");
		exit(EXIT_FAILURE);
        }
	lg.l_onoff = 1;
	lg.l_linger = 5;
        setsockopt(fds, SOL_SOCKET, SO_LINGER, (void *)&lg, sizeof(struct linger));
 
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[2]));

	if(( retval= inet_aton(argv[1],& server_addr.sin_addr))== 0){
		struct addrinfo *res, *p;
		if((retval = getaddrinfo(argv[1], NULL, &hints, &res) ) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval) );
			return 2;
		}

		memcpy(&server_addr.sin_addr, &res->ai_addr->sa_data[2], sizeof(server_addr.sin_addr));
		char *ipstr = inet_ntoa(server_addr.sin_addr);
		printf("IPv4: %s\n", ipstr);
 
		freeaddrinfo(res);
	}

        if ((retval=connect(fds, (struct sockaddr *)&server_addr, sizeof (server_addr)))<0){
		perror("connect");
		exit(EXIT_FAILURE);
        }

	
	if(flag== TRUE){
		req.type= RKILL;
	}else{
		req.type= REPORT;	
	}

	if(my_send(fds, &req, sizeof(message_t), 0)== -1){
		perror("send");
		exit(EXIT_FAILURE);
	}
	
	// get total number of hosts
	if(my_recv(fds, &nhost, sizeof(int), 0)==-1){
		perror("recv");
		exit(EXIT_FAILURE);
	}
	// print information per host
	for(i=0; i< nhost; i++){
		if(my_recv(fds, &hinfo, sizeof(hostinfo_t), 0)== -1){
			exit(EXIT_FAILURE);
		}

		printf("host :%s \n", hinfo.hostname);
		printf("\tperfect numbers found:\n\t");
		for(j=0; j< hinfo.npf; j++){
			printf("%d ", hinfo.pf[j]);
		}
		printf("\n\tthere are %d compute processes running on this host\n", hinfo.nproc);
		printf("\tprocess id\tnumber tested\tcurrent working range\n");
		for(j=0; j< hinfo.nproc; j++){
			if(my_recv(fds, &pinfo, sizeof(procinfo_t), 0) == -1){
				exit(EXIT_FAILURE);
			}
			printf("\t%d\t\t%d\t\t[ %d %d ]\n", pinfo.id, pinfo.tested, pinfo.rng.start, pinfo.rng.end);
		}
	}
	printf("******************************************\n");
        cleanup(FALSE, fds, 0);
        exit(EXIT_SUCCESS);	
	return 0;
}

// if signal comes before report send out request, shall report send terminate msg?
void handler(int sig){
	printf("\nenter report handler\n");
	print_sig(sig);
	
	message_t msgsnd;
	msgsnd.id= getpid();
	msgsnd.type= (flag==TRUE? RKILL: REPORT);

	if(my_send(fds, &msgsnd, sizeof(message_t), 0)!= sizeof(message_t)){
		exit(EXIT_FAILURE);
	}
	
	close(fds);
	exit(EXIT_SUCCESS);

}

