/*Author: KAI WANG*/
#include"head.h"


int nhost= 0;
int shut_down= FALSE;
int nwait=0;
struct pollfd_wrapper polldata;
host_t *hostlist_h, *hostlist_e;
range *range_list_h;

void sighandler(int);
range* checkout_range(int start_req, int size_req, range* curRng);
void update_result(int type);
void my_clean();
void report();

int main(int argc,char** argv){
	message_t msgrcv, msgsnd;
	struct sockaddr_in s_addr;
	int opt=1;
	int i,  retval, lsport;
	
	
	range* rngp;
	host_t* hostp;
	process_t* procp;

	hostinfo_t hinfo;
	procinfo_t pinfo;

	if(argc!= 2){
		fprintf(stderr, "usage: manage portnumber!\n");
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

	//init
	hostlist_h= hostlist_e= (host_t*)NULL;
	
	range_list_h=  (range*)malloc(sizeof(range));
	if(range_list_h==(range*) NULL){
		fprintf(stderr, "malloc failure\n");
		exit(EXIT_FAILURE);
	}
	range_list_h->start=0;
	range_list_h->end= MAXNUM-1;			
	range_list_h->next= range_list_h->pre= range_list_h;
	range_list_size= 1;

	polldata.capacity= 20;	//set initial units for dfs to be 20
	polldata.nfds= 0;
	polldata.fds= (struct pollfd *)calloc(1, polldata.capacity*sizeof(struct pollfd));
	polldata.procptr= (struct process_t* *)calloc(1,polldata.capacity*sizeof(struct process_t*));

	//setup socket connection
	if ((lsport = socket (AF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	if (setsockopt(lsport,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0){
		perror("setsockopt\n");
		exit(EXIT_FAILURE);
	}
	
	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(atoi(argv[1]));
	s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind (lsport,(struct sockaddr *)&s_addr, sizeof s_addr)==-1){
		perror("bind");
		cleanup(FALSE, lsport,1);
		exit(EXIT_FAILURE);
	}

	if (listen (lsport, MAXPENDING)==-1){
		perror("listen");
		cleanup(FALSE, lsport,1);
		exit(EXIT_FAILURE);
	}

	
	//add listening socket for polling
	polldata.fds[0].fd = lsport;
	polldata.fds[0].events = POLLIN; 	// | POLLPRI;
	polldata.nfds= 1;

	while(1){
		//poll with indefinite timeout
		if ((retval= poll(polldata.fds,polldata.nfds,-1))== -1){
			perror ("poll");
			//exit(EXIT_FAILURE);
		}
		for (i=0;(i<polldata.nfds) && (retval);i++){
			if (! polldata.fds[i].revents){
				continue;
			}
			retval--;
			
			if((polldata.fds[i].revents & POLLIN)!=0){
				if(polldata.fds[i].fd == lsport){
					// events occured on listen socket
					printf("\tAccepting connection...\n");
		
					//reallocate when no more free space
					if(polldata.nfds== polldata.capacity){
						polldata.fds = (struct pollfd *)realloc(polldata.fds,(polldata.capacity *2)*sizeof(struct pollfd));
						polldata.procptr= (struct process_t* *)realloc(polldata.procptr, (polldata.capacity *2)* sizeof(struct process_t*));
						//init unused space	--- liuyi					 
						memset(&polldata.fds[polldata.nfds], 0, polldata.capacity*sizeof(struct pollfd));
						memset(&polldata.procptr[polldata.nfds], 0, polldata.capacity*sizeof(struct process_t*));
						polldata.capacity *=2;
					}

					socklen_t addrlen = sizeof (s_addr);
					if((polldata.fds[polldata.nfds].fd  = accept (lsport, (struct sockaddr *)&s_addr, &addrlen))==-1){
						perror ("accept");
						cleanup(FALSE, polldata.fds[polldata.nfds].fd, 0);
						continue;
					}
					polldata.fds[polldata.nfds].events = POLLIN;//procptr is set when receiving first msg
					polldata.nfds++;
				}else{
					//receive a message from compute or report
					if(my_recv(polldata.fds[i].fd, &msgrcv, sizeof(message_t), 0)==-1){
						fprintf(stderr, "\tmy_recv failure\n");
						close(polldata.fds[i].fd);
						memmove(polldata.fds+i,polldata.fds+i+1,polldata.nfds-i-1);
					 	polldata.nfds--;
						continue;
					}
					int curSize;
					switch(msgrcv.type){
						case STARTUP:
							// setup host process structure
							if(hostlist_h== NULL){
								hostp= hostlist_h= hostlist_e= (host_t*)malloc(sizeof(host_t));
								init_host_t(hostp, msgrcv.hostname);
								procp= (process_t*)malloc(sizeof(process_t));
								init_proc_t(procp, polldata.fds[i].fd, hostp, msgrcv.id);

								polldata.procptr[i]= procp;		//---
								hostp->phead= hostp->ptail= procp;
								hostp->hinfo.nproc= 1;
								nhost++;
							}else{
								hostp= hostlist_h;
								while(hostp!= NULL){
									if(strcmp(hostp->hinfo.hostname, msgrcv.hostname)==0){
										procp= (process_t*)malloc(sizeof(process_t));
										init_proc_t(procp, polldata.fds[i].fd, hostp,msgrcv.id);
										polldata.procptr[i]= procp;
										if(hostp->phead== NULL && hostp->ptail==NULL){
											fprintf(stderr, "shouldnt reach here\n");
											hostp->phead= hostp->ptail= procp;
										}else{
											hostp->ptail->next= procp;
											hostp->ptail= procp;
										}
										hostp->hinfo.nproc++;
										break;
									}
									hostp= hostp->next;
								}
								if(hostp==NULL){
									hostp= hostlist_e->next= (host_t*)malloc(sizeof(host_t));
									hostlist_e= hostlist_e->next;
									init_host_t(hostp, msgrcv.hostname);
									procp= (process_t*)malloc(sizeof(process_t));
									init_proc_t(procp, polldata.fds[i].fd, hostp, msgrcv.id);
									polldata.procptr[i]= procp;
									hostp->phead=hostp->ptail= procp;
									hostp->hinfo.nproc= 1;
									nhost++;
								}
							}
							
							rngp= checkout_range(msgrcv.data, INIT_RANGE_SIZE, NULL);
							memcpy(& procp->pinfo.rng,rngp, sizeof(range));		//record last range
							
							msgsnd.type= RANGE;
							if(rngp== (range*)NULL){
								msgsnd.start= msgsnd.end= -1;
							}else{
								msgsnd.start= rngp->start;
								msgsnd.end= rngp->end;	
							}
							if(my_send(polldata.fds[i].fd, &msgsnd, sizeof(message_t), 0)==-1){
								fprintf(stderr, "my_send\n");
								//
							}
							break;

						case NEXTRANGE:
							curSize= range_size1(& polldata.procptr[i]->pinfo.rng);
							rngp= checkout_range(msgrcv.data, (int)curSize*(TARGETT/msgrcv.tm), &(polldata.procptr[i]->pinfo.rng));
							msgsnd.type= RANGE;
							if(rngp== (range*)NULL){
								msgsnd.start= msgsnd.end= -1;
							}else{
								msgsnd.start= rngp->start;
								msgsnd.end= rngp->end;	
							}
							polldata.procptr[i]->pinfo.tested+= range_size1(&polldata.procptr[i]->pinfo.rng);
							memcpy(& polldata.procptr[i]->pinfo.rng, rngp, sizeof(range));//update range
							if(my_send(polldata.fds[i].fd, &msgsnd, sizeof(message_t), 0)==-1){
								fprintf(stderr, "my_send\n");
								exit(EXIT_FAILURE);			// not exit here?
							}
							break;
						case PFFOUND:
							//check initialization of npf
							polldata.procptr[i]->hostp->hinfo.pf[polldata.procptr[i]->hostp->hinfo.npf++]= msgrcv.data;	
							
							break;
				
						case CREPORT:
							
							break;
						case CEND:
							msgsnd.type= CEND;
							printf("process %d last tested number: %d\n", msgrcv.id, msgrcv.data);
						
							if(my_send(polldata.fds[i].fd, &msgsnd, sizeof(message_t), 0)==-1){
								perror("my_send");
								exit(1);
							}
							close(polldata.fds[i].fd);
							memmove(polldata.fds+i,polldata.fds+i+1,polldata.nfds-i-1);
							memmove(polldata.procptr+i,polldata.procptr+i+1,polldata.nfds-i-1);
					 		polldata.nfds--;
									//count the number of replied compute
							break;
						case REPORT:
							update_result(REPORT);
							//send back to report process
							report(i);
							
							//close socket for report
							close(polldata.fds[i].fd);
							memmove(polldata.fds+i,polldata.fds+i+1,polldata.nfds-i-1);
							memmove(polldata.procptr+i,polldata.procptr+i+1,polldata.nfds-i-1);
					 		polldata.nfds--;

							//my_clean();

							break;
						case RKILL:
							update_result(MKILL);
							
							report(i);
							//close 
							close(polldata.fds[i].fd);
							memmove(polldata.fds+i,polldata.fds+i+1,polldata.nfds-i-1);
							memmove(polldata.procptr+i,polldata.procptr+i+1,polldata.nfds-i-1);
					 		polldata.nfds--;

							my_clean();
							break;
						default:
							fprintf(stderr, "unexpected message type\n");
							break;
					}
				}
				continue;
		  	}
			/* fd not open*/
			if (((polldata.fds+i)->revents & POLLNVAL)!=0 ){
				polldata.nfds--;
				memmove(polldata.fds+i,polldata.fds+i+1,polldata.nfds-i);
				memmove(polldata.procptr+i,polldata.procptr+i+1,polldata.nfds-i);
				continue;
			}
			/* hang up and error*/
			if (((polldata.fds+i)->revents & POLLHUP) || ((polldata.fds+i)->revents & POLLERR)){
				cleanup(FALSE,(polldata.fds+i)->fd,0);
				polldata.nfds--;
				memmove(polldata.fds+i,polldata.fds+i+1,polldata.nfds-i);
				memmove(polldata.procptr+i,polldata.procptr+i+1,polldata.nfds-i);
				continue;
			}
		}
	}

	return 0; 
}

void report(int i){
	message_t msgsnd, msnrcv;	
	host_t* hostp;
	process_t* procp;
	hostinfo_t hinfo;
	procinfo_t pinfo;

	msgsnd.type= REPORT;
	//send number of hosts
	if(my_send(polldata.fds[i].fd, &nhost, sizeof(int), 0)==-1){
		perror("send");
		exit(1);
	}

	for(hostp= hostlist_h; hostp!=NULL; hostp= hostp->next){
		//send relevant host info
		if(my_send(polldata.fds[i].fd, &(hostp->hinfo), sizeof(hostinfo_t), 0)==-1){
			fprintf(stderr, "my_send");
			continue;
			//exit(EXIT_FAILURE);			// not exit here?
		}
		//send process info on this host
		for(procp= hostp->phead; procp!= NULL;procp= procp->next){
			memcpy(&pinfo,&(procp->pinfo), sizeof(procinfo_t));
			pinfo.tested+= pinfo.rem;
			if(my_send(polldata.fds[i].fd, &pinfo, sizeof(procinfo_t), 0)==-1){
				fprintf(stderr, "my_send");
				continue;
				//exit(EXIT_FAILURE);			// not exit here?
			}	
		}
	}
}

void sighandler(int sig){
	int i;
	message_t msgsnd;
	msgsnd.type= MKILL;
	
	//send each compute a message to terminate and wait for response
	for(i= 1; i< polldata.nfds; i++){
		//skip if it's the socket fd for report
		if(polldata.procptr[i]== (process_t*)NULL){
			continue;
		}

		if(my_send(polldata.fds[i].fd, &msgsnd, sizeof(message_t),0)== -1){
			fprintf(stderr, "my_send\n");
			continue;	
		}
	}
	my_clean();
	exit(0);
}

void my_clean(){
	int i;
	/*close socket fds*/
	for(i=1; i< polldata.nfds; i++){
		cleanup(FALSE, polldata.fds[i].fd, 0);
	}
	polldata.nfds=1;

	/*free dynamically allocated memory*/
	if(nhost>0){
		host_t* hostp= hostlist_h;
		while(hostp!= NULL){
			//free process_t linklist
			process_t* procp= hostp->phead;
			while(procp!=NULL){
				process_t* tmp_p= procp->next;
				free(procp);
				procp= tmp_p;
			}
			hostp->phead= hostp->ptail= (process_t*)NULL;

			host_t* tmp_h= hostp->next;
			free(hostp);
			hostp= tmp_h;
		}
	}
	nhost=0;
	hostlist_h= hostlist_e= (host_t*)NULL;		

	/*range * rngp= range_list_h;
	while(rngp!= NULL){
		range* tmp_rng= rngp->next;
		free(rngp);
		rngp= tmp_rng;
		if(rngp== range_list_h){
			break;
		}
	}
	range_list_h= (range*)NULL;*/
}

//update for report, the rem field
void update_result(int type){
	int i;
	message_t msgsnd, msgrcv;
	msgsnd.type= type;
	
	//send each compute a message to terminate and wait for response
	for(i= 1; i< polldata.nfds; i++){
		//skip if it's the socket fd for report
		if(polldata.procptr[i]== (process_t*)NULL){
			continue;
		}

		if(my_send(polldata.fds[i].fd, &msgsnd, sizeof(message_t),0)== -1){
			fprintf(stderr, "my_send\n");
			continue;	
		}
		
		if(my_recv(polldata.fds[i].fd, &msgrcv, sizeof(message_t),0)== -1){
			fprintf(stderr, "my_recv\n");
			continue;
		}
		if(msgrcv.type != CREPORT){		
			fprintf(stderr, "unexpected message type\n");
			continue;
		}

		if(type == RKILL){
			//send confirmation to shutdown compute
			if(my_send(polldata.fds[i].fd, &msgsnd, sizeof(message_t),0)== -1){
				fprintf(stderr, "my_send\n");
				continue;
			}
		}

		//got msg saying last number tested
		polldata.procptr[i]->pinfo.rem = range_size2(msgrcv.start, msgrcv.end);
		printf("last tested number of proc %d : %d\n", msgrcv.id, msgrcv.data);
	}
}


//return a new range for compute
range* checkout_range(int start_req, int size_req, range* curRng){
	range* p;
	int shortage, size;

	if(start_req!= -1){
		//initial request with a starting point
		if(range_list_h == NULL){
			fprintf(stderr, "parameter error: range list is empty\n");
			exit(EXIT_FAILURE);
		}

		p= range_list_h;
		do{
			//the whole range is not used
			if(range_list_size==1){
				size= range_size1(p);
				if( size< size_req || (size-1)< start_req){
					fprintf(stderr, "bad compute request: out of range\n");
					exit(EXIT_FAILURE);
				}
				range* next= (range*)malloc(sizeof(range));
				next->start= module(start_req+ size_req);
				next->end= module(start_req-1);
				next->is_used= FALSE;
				p->start= module(start_req);
				p->end= module(p->start+ size_req-1);
				p->is_used= TRUE;
				insert_range(p, NULL, next);

				return p;
			}
			//find the range where the requested starting point is
			if((in_range(start_req, p)== TRUE)){
				//check whether this range is usable
				if(p->is_used== FALSE && (size= range_size1(p))>= size_req){
					p->is_used= TRUE;
					if(size> size_req){
						//return a sub-range starting from requested starting point	
						shortage= size_req- range_size2(start_req, p->end);
						if(shortage>=0){
							//return a range starting (shortage bytes) before requested starting point, no insert after
							range* pre= (range*) malloc(sizeof(range));
							pre->is_used= FALSE;
							pre->start= p->start;
							p->start= module(start_req- shortage);
							pre->end= module(p->start -1);
							
							insert_range(p, pre, NULL);
						}else{
							//insert on both sides
							range* pre= (range*) malloc(sizeof(range));
							range* next= (range*) malloc(sizeof(range));
							pre->is_used= FALSE;
							pre->start= p->start;
							pre->end=  module(start_req- 1);
							next->is_used= FALSE;
							next->start= module(start_req+ size_req);
							next->end= p->end;

							p->start= start_req;
							p->end= module(start_req+ size_req- 1);
							insert_range(p, pre, next);
						}
					}else{
						//add code if link by host
					}
					
					return p;
				}else{
					//return next range that can accomodate this range request
					p= p->next;
					while(p!= range_list_h){
						if(p->is_used== FALSE && (size= range_size1(p))>= size_req){
							p->is_used= TRUE;
							if(size> size_req){
								range* next= (range*)malloc(sizeof(range));
								next->start= module(p->start+ size_req);
								next->end= p->end;
								next->is_used= FALSE;
								p->end= module(p->start+ size_req-1);
								insert_range(p, NULL, next);
							}else{
								// if link by host
							}
							return p;
						}
						p= p->next;
					}
				}
			}
			p= p->next;
		}while(p!= range_list_h);
		//shoudnt reach here
		fprintf(stderr, "error in checkout_range\n");
	 	return NULL;
	}else{
		//successive request searching from current range of requesting process
		if(curRng== (range*)NULL){
			fprintf(stderr, "invalid parameter: pointer curRng is NULL\n");
			exit(EXIT_FAILURE);
		}
		p= curRng;
		while(p->next != curRng){
			p= p->next;
			if(p->is_used== FALSE && (size= range_size1(p))>= size_req){
				p->is_used= TRUE;
				if(size> size_req){
					range* next= (range*)malloc(sizeof(range));
					next->start= module(p->start+ size_req);
					next->end= p->end;
					next->is_used= FALSE;
					p->end= module(p->start+ size_req-1);
					insert_range(p, NULL, next);			
				}else{
					// if link by host
					
				}
				
				return p;			 
			}	
		}
		if(p->next){
			return NULL;
		}
	}
}

