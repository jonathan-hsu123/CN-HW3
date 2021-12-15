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
#define current_time chrono::high_resolution_clock::now
#define time_stamp chrono::high_resolution_clock::time_point
#define elapsed_time(end, start) chrono::duration_cast<chrono::milliseconds>(end - start).count()

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
	deque<time_stamp> time_queue;
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

	string input = to_string(width) + "," + to_string(height) + "," + to_string(vid_length) + "," + to_string(frame_size) + "\n";
	sprintf(tmp_seg.data, "%s", input.c_str());
	tmp_seg.head.seqNumber = -1;
	tmp_seg.head.length = strlen(input.c_str());
	my_queue -> queue.push_back(tmp_seg);

	for(int f = 0; f < frame_size; f++) {
		//parase frame
		cap >> img;
		int iter = frame_size / SIZEBUFF;
		for(int i = 0; i < iter; i++) {
			memset(&tmp_seg, 0, sizeof(segment));
			memcpy(&tmp_seg.data, img.data + i * SIZEBUFF, SIZEBUFF);
		}
		//send each frame
	}
	// sendto(sockfd, stuff_to_send, sizeof(segment), MSG_CONFIRM, (const struct sockaddr *) &agentaddr, sizeof(agentaddr));
	
	return 0;
}
