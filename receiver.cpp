#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "opencv2/opencv.hpp"
#include <iostream>

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

	unsigned int len = sizeof(agentaddr);
	
	if ( bind(sockfd, (const struct sockaddr *)&recvaddr, sizeof(recvaddr)) < 0 ) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	segment tmp_seg;
	int width, height, vid_length, frame_size;
	Mat img;
	int last_recv = 0, leftover, frame_seg;
	while(true) {
		memset(&tmp_seg, 0, sizeof(segment));
		recvfrom(sockfd, &tmp_seg, sizeof(segment), MSG_WAITALL, (struct sockaddr *) &agentaddr, &len);
		bool output = (tmp_seg.head.length == leftover);
		if(tmp_seg.head.fin == 1) {
			cout << "recv	fin\n"; 
			memset(&tmp_seg, 0, sizeof(segment));
			tmp_seg.head.ack = 1;
			tmp_seg.head.fin = 1;
			sendto(sockfd, &tmp_seg, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
			cout << "send	finack\n"; 
			break;
		}
		if(tmp_seg.head.seqNumber == 0) {
			sscanf(tmp_seg.data, "%d,%d,%d,%d", &width, &height, &vid_length, &frame_size);
			img = Mat::zeros(height, width, CV_8UC3);
			if(!img.isContinuous()) {
         		img = img.clone();
    		}
			leftover = frame_size % 1000;
            frame_seg = frame_size / 1000 + 1;
			// cout << width << " " << height << " " << vid_length << " " << frame_size << endl;
			cout << "recv	data	#0\n"; 
			memset(&tmp_seg, 0, sizeof(segment));
			tmp_seg.head.ack = 1;
			tmp_seg.head.ackNumber = 0;
			sendto(sockfd, &tmp_seg, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
			cout << "send	ack	#0\n"; 
		}
		else {
			if(tmp_seg.head.seqNumber == last_recv + 1) {
				memcpy(img.data + (last_recv % frame_seg) * 1000, &tmp_seg.data, tmp_seg.head.length);
				last_recv++;
				cout << "recv	data	#" << last_recv << endl;
				memset(&tmp_seg, 0, sizeof(segment));
				tmp_seg.head.ack = 1;
				tmp_seg.head.ackNumber = last_recv;
				sendto(sockfd, &tmp_seg, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
				cout << "send	ack	#" << last_recv << endl;
			}
			else {
				cout << "drop	data	#" << tmp_seg.head.seqNumber << endl;
				memset(&tmp_seg, 0, sizeof(segment));
				tmp_seg.head.ack = 1;
				tmp_seg.head.ackNumber = last_recv;
				sendto(sockfd, &tmp_seg, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
				cout << "send	ack	#" << last_recv << endl;
			}
		}
		if(output && (last_recv % frame_seg) == 0) {
			imshow("Video", img);
			char c = (char)waitKey(33.3333);
			if(c == 27) break;
			cout << "flush\n";
			sleep(0.1);
		}
	}
    destroyAllWindows();
	// char c = (char)waitKey(1);
	return 0;
}