#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "opencv2/opencv.hpp"
using namespace cv;
using namespace std;

Point previousLineCenter(-1, -1);  // 이전 라인 중심 위치를 저장

void adBright(Mat &frame, double target_brightness)
{
    Mat gray;
    cvtColor(frame, gray, COLOR_BGR2GRAY);

    Scalar mean_value = mean(gray);
    double current_brightness = mean_value[0];

    // 밝기 차이 계산
    double brightness_diff = target_brightness - current_brightness;

    // 영상의 밝기 조정 (전체 픽셀에 대해 차이만큼 더하기)
    frame = frame + Scalar(brightness_diff, brightness_diff, brightness_diff);
}

// 라인 찾기 함수
void findLines(Mat &thr_cut, Mat &thres_cut)
{
    // 연결된 컴포넌트 찾기
    Mat labels, stats, centroids;
    int nLabels = connectedComponentsWithStats(thres_cut, labels, stats, centroids, 8, CV_32S);

    // 진짜 라인 후보를 필터링
    vector<Rect> trueLines;
    vector<Point> lineCenters;

    for (int i = 1; i < nLabels; i++) {  // 0은 배경이므로 제외
        int area = stats.at<int>(i, CC_STAT_AREA);
        int left = stats.at<int>(i, CC_STAT_LEFT);
        int top = stats.at<int>(i, CC_STAT_TOP);
        int width = stats.at<int>(i, CC_STAT_WIDTH);
        int height = stats.at<int>(i, CC_STAT_HEIGHT);

        // 영역 필터링: 면적, 비율 등을 기준으로 진짜 라인으로 추정
        if (area > 100 && area < 5000) {
            trueLines.push_back(Rect(left, top, width, height));
            // 라인 중심 계산
            Point center(left + width / 2, top + height / 2);
            lineCenters.push_back(center);
        }
    }

    // 라인 후보가 여러 개 있을 때, 이전 라인 중심과 가장 가까운 라인 선택
    if (lineCenters.size() > 0) {
        Point closestLineCenter;
        if (previousLineCenter.x == -1 && previousLineCenter.y == -1) {
            // 초기에는 첫 번째 라인을 선택
            closestLineCenter = lineCenters[0];
        } else {
            double minDistance = DBL_MAX;
            for (const Point &center : lineCenters) {
                double distance = norm(center - previousLineCenter);
                if (distance < minDistance) {
                    minDistance = distance;
                    closestLineCenter = center;
                }
            }
        }

        // 선택된 라인 중심을 이전 라인 중심으로 업데이트
        previousLineCenter = closestLineCenter;

        // 선택된 라인 중심 표시
        circle(thr_cut, closestLineCenter, 2, Scalar(0, 0, 255), 2);
    }
}

int main(void)
{
    struct timeval start, end1;
    double time1;

    VideoCapture source("5_lt_cw_100rpm_out.mp4"); 
    if (!source.isOpened()) { cout << "Camera error" << endl; return -1; }

    // 두 개의 출력 스트림 설정
    string dst1 = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! \
    nvvidconv ! nvv4l2h264enc insert-sps-pps=true ! \
    h264parse ! rtph264pay pt=96 ! \
    udpsink host=203.234.58.160 port=8001 sync=false";

    VideoWriter writer1(dst1, 0, (double)30, Size(640, 360), true);
    if (!writer1.isOpened()) { cerr << "Writer open failed!" << endl; return -1; }

    string dst2 = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! \
    nvvidconv ! nvv4l2h264enc insert-sps-pps=true ! \
    h264parse ! rtph264pay pt=96 ! \
    udpsink host=203.234.58.160 port=8002 sync=false";

    VideoWriter writer2(dst2, 0, (double)30, Size(640, 90), true);
    if (!writer2.isOpened()) { cerr << "Writer open failed!" << endl; return -1; }

    VideoWriter save_video("output.mp4", VideoWriter::fourcc('M', 'J', 'P', 'G'), (double)30, Size(640, 360), true);
    if (!save_video.isOpened()) { cerr << "Writer open failed!" << endl; return -1; }

    Mat frame, gray, thres, thr_bgr, thr_cut;

    double target_brightness = 90.0;

    while (true) {
        gettimeofday(&start, NULL);

        source >> frame;
        if (frame.empty()) { cerr << "Frame empty!" << endl; break; }

        adBright(frame, target_brightness);

        // 그레이스케일로 변환하고 이진화
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        threshold(gray, thres, 150, 255, THRESH_BINARY);
        cvtColor(thres, thr_bgr, COLOR_GRAY2BGR);

        Rect cut1(0, 270, 640, 90);
        thr_cut = thr_bgr(cut1);

        Mat thres_cut = thres(cut1);

        // 라인 찾기
        findLines(thr_cut, thres_cut);

        // 출력 영상에 라인 표시된 결과를 기록
        writer1 << frame;
        writer2 << thr_cut;
        save_video << thr_bgr;

        gettimeofday(&end1, NULL);
        time1 = end1.tv_sec - start.tv_sec + (end1.tv_usec - start.tv_usec) / 1000000.0;
        cout << "Time: " << time1 << " seconds" << endl;
    }

    return 0;
}
