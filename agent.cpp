#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>

using namespace std;

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[1000];
} segment;

int main(int argc, char *argv[]){
    int sockfd;
	int send_port = atoi(argv[1]);
	int agent_port = atoi(argv[2]);
	int recv_port = atoi(argv[3]);
	double loss_rate = atof(argv[4]);
	struct sockaddr_in recvaddr;
	struct sockaddr_in sendaddr;
	struct sockaddr_in agentaddr;
	struct sockaddr_in tmpaddr;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( sockfd < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	memset(&recvaddr, 0, sizeof(recvaddr));
	memset(&sendaddr, 0, sizeof(sendaddr));
	memset(&agentaddr, 0, sizeof(agentaddr));
	memset(&tmpaddr, 0, sizeof(agentaddr));
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_addr.s_addr = INADDR_ANY;
	sendaddr.sin_port = htons(send_port);
	agentaddr.sin_family = AF_INET;
	agentaddr.sin_port = htons(agent_port);
	agentaddr.sin_addr.s_addr = INADDR_ANY;
	recvaddr.sin_family = AF_INET;
	recvaddr.sin_port = htons(recv_port);
	recvaddr.sin_addr.s_addr = INADDR_ANY;
	
	if ( bind(sockfd, (const struct sockaddr *)&agentaddr, sizeof(agentaddr)) < 0 ) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	segment tmp_seg;
	int total_data = 0;
    int drop_data = 0;
	while(true) {
		recvfrom(sockfd, &tmp_seg, sizeof(segment), MSG_WAITALL, (struct sockaddr *) &tmpaddr, (unsigned int*)sizeof(tmpaddr));
		if(tmpaddr.sin_port == sendaddr.sin_port) { //from sender
			total_data++;
			if(tmp_seg.head.fin == 1) { //fin
				printf("get     fin\n");
				sendto(sockfd, &tmp_seg, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &recvaddr, sizeof(recvaddr));
				printf("fwd     fin\n");
			}
			else { //data
				int index = tmp_seg.head.seqNumber;
                if(rand() % 100 < 100 * loss_rate){
                    drop_data++;
                    printf("drop	data	#%d,	loss rate = %.4f\n", index, (float)drop_data/total_data);
                } else{ 
                    printf("get	data	#%d\n",index);
                    sendto(sockfd, &tmp_seg, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &recvaddr, sizeof(recvaddr));
                    printf("fwd	data	#%d,	loss rate = %.4f\n",index,(float)drop_data/total_data);
                }
			}
		}
		else { //from receiver

		}
	}
    return 0;
}