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
#define TIMEOUT 1
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

	cout << agentaddr.sin_port;

	if ( bind(sockfd, (const struct sockaddr *)&sendaddr, sizeof(sendaddr)) < 0 ) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	// some initial setting of sender side UDP socket

	Mat img;
	VideoCapture cap(argv[4]);
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
	deque<segment> queue;
	int window_size = 1;
	int threshold = 16;
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
	queue.push_back(tmp_seg);
    long long int last_send = -1, last_ack = -1;
	for(int f = 0; f < vid_length; f++) {
		//parase frame
		cap >> img;
		int iter = frame_size / SIZEBUFF;
		int leftover = frame_size % SIZEBUFF;
		for(int i = 0; i < iter; i++) {
			memset(&tmp_seg, 0, sizeof(segment));
			memcpy(&tmp_seg.data, img.data + i * SIZEBUFF, SIZEBUFF);
			tmp_seg.head.length = SIZEBUFF;
			tmp_seg.head.seqNumber = last_send + i + 1 + (f == 0 ? 1 : 0);
			queue.push_back(tmp_seg);
		}
		memset(&tmp_seg, 0, sizeof(segment));
		memcpy(&tmp_seg.data, img.data + iter * SIZEBUFF, leftover);
		tmp_seg.head.length = leftover;
		tmp_seg.head.seqNumber = last_send + iter + 1 + (f == 0 ? 1 : 0);
		queue.push_back(tmp_seg);

		int rtv;
		unsigned int len = sizeof(agentaddr); 
		//send each frame and get ACK
		while(!(queue.empty())) {
			window_size = min(window_size, int(queue.size()));
			for(int i = 0; i < window_size; i++) {
				sendto(sockfd, &(queue[i]), sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
				if(queue[i].head.seqNumber > last_send) {
					cout << "send	data	#" << queue[i].head.seqNumber <<",	winSize = " << window_size << endl;
					last_send = queue[i].head.seqNumber;
				}
				else cout << "resnd	data	#" << queue[i].head.seqNumber <<",	winSize = " << window_size << endl;
			}
			bool success = true;
			for(int i = 0; i < window_size; i++) {
				rtv = recvfrom(sockfd, &tmp_seg, sizeof(segment), MSG_WAITALL, (struct sockaddr *) &agentaddr, &len);
				if(rtv == -1) {
					cout << "time	out,		threshold = " << threshold << endl;
					success = false;
                    break;
				}
				else if(tmp_seg.head.ackNumber >= last_ack + 1) {
					while(queue.size() && tmp_seg.head.ackNumber >= queue.front().head.seqNumber) {
                        queue.pop_front();
                    }
                    cout << "recv	ack	#" << tmp_seg.head.ackNumber << endl;
					last_ack = tmp_seg.head.ackNumber;
				}
				else {
					cout << "recv	ack	#" << tmp_seg.head.ackNumber << endl;
					success = false;
				}
				if(queue.empty()) {
					break;
					cout << "empty\n";
				}
			}
			if(success) {
				if(window_size < threshold) window_size *= 2;
				else window_size++;
			}
			else {
				threshold = max(window_size / 2, 1);
				window_size = 1;
			}
		}
	}
	while(true) {
		memset(&tmp_seg, 0, sizeof(segment));
		tmp_seg.head.fin = 1;
		sendto(sockfd, &tmp_seg, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
		cout << "send	fin\n";
		recvfrom(sockfd, &tmp_seg, sizeof(segment), MSG_WAITALL, (struct sockaddr *) &agentaddr, (unsigned int*)sizeof(agentaddr));
		if(tmp_seg.head.fin == 1 && tmp_seg.head.ack == 1) break;
	}
	cout << "recv	finack\n";
	cap.release();
	return 0;
}