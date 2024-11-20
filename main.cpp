#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "opencv2/opencv.hpp"
using namespace cv;
using namespace std;

void adBright(Mat &frame, double target_brightness)
{
    Mat gray, thres;
    cvtColor(frame, gray, COLOR_BGR2GRAY);
    threshold(gray, thres, 155, 255, THRESH_BINARY);

    Scalar mean_value = mean(thres);
    double current_brightness = mean_value[0];

    // 밝기 차이 계산
    double brightness_diff = target_brightness - current_brightness;

    // 영상의 밝기 조정 (전체 픽셀에 대해 차이만큼 더하기)
    frame = frame + Scalar(brightness_diff, brightness_diff, brightness_diff);
}

int main(void)
{
    struct timeval start, end1;
    double time1;

    VideoCapture source("5_lt_cw_100rpm_out.mp4"); 
    if (!source.isOpened()){ cout << "Camera error" << endl; return -1;}

    string dst = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! \
	nvvidconv ! nvv4l2h264enc insert-sps-pps=true ! \
	h264parse ! rtph264pay pt=96 ! \
	udpsink host=203.234.58.160 port=8001 sync=false";

    VideoWriter writer(dst,0, (double)30,Size(640,360),false);
    if(!writer.isOpened()) {cerr<<"Writer open failed!"<<endl; return -1;}

    VideoWriter save_video("output.mp4", VideoWriter::fourcc('M', 'J', 'P', 'G'), (double)30, Size(640, 360), false);
    if (!save_video.isOpened()) {cerr << "Writer open failed!" << endl; return -1;}

    Mat frame, gray, thres;

    double target_brightness = 80.0;

    while (true) {
        gettimeofday(&start, NULL);
        
        source >> frame;
        if (frame.empty()) {cerr << "Frame empty!" << endl; break;}
        
        adBright(frame, target_brightness);

        cvtColor(frame, gray, COLOR_BGR2GRAY);
        threshold(gray, thres, 155, 255, THRESH_BINARY);

        save_video << thres;
        writer << thres;

        gettimeofday(&end1, NULL);
        time1 = end1.tv_sec - start.tv_sec + (end1.tv_usec - start.tv_usec) / 1000000.0;
        cout << "Time: " << time1 << " seconds" << endl;
    }

    return 0;   
}
