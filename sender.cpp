#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "opencv2/opencv.hpp"
#include <deque>
#include <string>
#include <chrono>
#include <ctime>
#define TIMEOUT 10

#define SIZEBUFF 1000

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
	char data[SIZEBUFF];
} segment;

typedef struct{
	deque<segment> queue;
	int sent_seq_num;
	int window_size;
	int threshold;
} Queue;

void init_queue(Queue* Q) {
	Q -> sent_seq_num = 0;
	Q -> window_size = 1;
	Q -> threshold = 16;
}

int main(int argc, char *argv[]) {
	int sockfd;
	int send_port = atoi(argv[1]);
	int agent_port = atoi(argv[2]);
	int recv_port = atoi(argv[3]);
	struct sockaddr_in sendaddr;
	struct sockaddr_in agentaddr;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if ( sockfd < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	memset(&sendaddr, 0, sizeof(sendaddr));
	memset(&agentaddr, 0, sizeof(agentaddr));
	
	sendaddr.sin_family = AF_INET;
	sendaddr.sin_addr.s_addr = INADDR_ANY;
	sendaddr.sin_port = htons(send_port);
	agentaddr.sin_family = AF_INET;
	agentaddr.sin_port = htons(agent_port);
	agentaddr.sin_addr.s_addr = INADDR_ANY;

	if ( bind(sockfd, (const struct sockaddr *)&sendaddr, sizeof(sendaddr)) < 0 ) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// some initial setting of sender side UDP socket

	Mat img;
	VideoCapture cap("./video.mpg");
	int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
	int vid_length = cap.get(CV_CAP_PROP_FRAME_COUNT);
	cout << "Video resolution: " << width << ", " << height << endl;
	cout << "Video length: " << vid_length << endl;
	img = Mat::zeros(height, width, CV_8UC3);
	if(!img.isContinuous()) {
         img = img.clone();
    }
	//some setting for playing video
	cap >> img; // first frame
	int frame_size = img.total() * img.elemSize(); // get frame size 
	cap.set(1, 0); // go back to first frame
	cout << "frame size: " << frame_size << endl;
	//start sending config of the video
	Queue* my_queue = (Queue *) malloc(sizeof(Queue));
	init_queue(my_queue);
	segment tmp_seg;
	memset(&tmp_seg, 0, sizeof(segment));

	//set socket timeout
	struct timeval timeout;      
  	timeout.tv_sec = TIMEOUT;
  	timeout.tv_usec = 0;
	setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	string input = to_string(width) + "," + to_string(height) + "," + to_string(vid_length) + "," + to_string(frame_size) + "\n";
	sprintf(tmp_seg.data, "%s", input.c_str());
	tmp_seg.head.seqNumber = 0;
	tmp_seg.head.length = strlen(input.c_str());
	my_queue -> queue.push_back(tmp_seg);

	for(int f = 0; f < frame_size; f++) {
		//parase frame
		cap >> img;
		int iter = frame_size / SIZEBUFF;
		int leftover = frame_size % SIZEBUFF;
		for(int i = 0; i < iter; i++) {
			memset(&tmp_seg, 0, sizeof(segment));
			memcpy(&tmp_seg.data, img.data + i * SIZEBUFF, SIZEBUFF);
			tmp_seg.head.length = SIZEBUFF;
			tmp_seg.head.seqNumber = i + 1;
			my_queue -> queue.push_back(tmp_seg);
		}
		memset(&tmp_seg, 0, sizeof(segment));
		memcpy(&tmp_seg.data, img.data + iter * SIZEBUFF, leftover);
		tmp_seg.head.length = leftover;
		tmp_seg.head.seqNumber = iter + 1;
		my_queue -> queue.push_back(tmp_seg);

		int last_send = -1, last_ack = ((f == 0) ? -1 : 0);
		int rtv;
		//send each frame and get ACK
		while(!(my_queue -> queue.empty())) {
			for(int i = 0; i < my_queue -> window_size; i++) {
				sendto(sockfd, &(my_queue -> queue[i]), sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
				if(my_queue -> queue[i].head.seqNumber > last_send) cout << "send	data	#" << my_queue -> queue[i].head.seqNumber <<",	winSize = " << my_queue -> window_size;
				else cout << "resnd	data	#" << my_queue -> queue[i].head.seqNumber <<",	winSize = " << my_queue -> window_size << endl;
				last_send = my_queue -> queue[i].head.seqNumber;
			}
			for(int i = last_ack + 1; i < last_ack + 1 + my_queue -> window_size; i++) {
				rtv = recvfrom(sockfd, &tmp_seg, sizeof(segment), MSG_WAITALL, (struct sockaddr *) &agentaddr, (unsigned int*)sizeof(agentaddr));
				if(rtv == -1) {
					cout << "time	out,		threshold = " << my_queue -> threshold << endl;
				}
				else if(tmp_seg.head.ackNumber == i) {
					my_queue -> queue.pop_front();
					cout << "recv	ack	#" << i << endl;
				}
				else {
					cout << "recv	ack	#" << tmp_seg.head.ackNumber << endl;
				}	
			}
		}
	}
	// 
	
	return 0;
}
