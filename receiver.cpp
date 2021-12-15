#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "opencv2/opencv.hpp"

#define MAXLINE 1024

using namespace std;
using namespace cv;

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

int main(int argc, char *argv[]) {
	int sockfd;
	int send_port = atoi(argv[1]);
	int agent_port = atoi(argv[2]);
	int recv_port = atoi(argv[3]);
	struct sockaddr_in recvaddr;
	struct sockaddr_in agentaddr;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( sockfd < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	memset(&recvaddr, 0, sizeof(recvaddr));
	memset(&agentaddr, 0, sizeof(agentaddr));

	agentaddr.sin_family = AF_INET;
	agentaddr.sin_port = htons(agent_port);
	agentaddr.sin_addr.s_addr = INADDR_ANY;
	recvaddr.sin_family = AF_INET;
	recvaddr.sin_port = htons(recv_port);
	recvaddr.sin_addr.s_addr = INADDR_ANY;
	
	if ( bind(sockfd, (const struct sockaddr *)&recvaddr, sizeof(recvaddr)) < 0 ) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	return 0;
}
