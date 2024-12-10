#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "dxl.hpp"
#include "opencv2/opencv.hpp"
using namespace cv;
using namespace std;

Point previousLineCenter(-1, -1);  // 이전 라인 중심 위치를 저장
bool ctrl_c_pressed = false;
void ctrlc(int)
{
    ctrl_c_pressed = true;
}

int lineError = 0;

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
void findLines(Mat& thr_cut, Mat& thres_cut)
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

        // 영역 필터링: 면적을 기준으로 진짜 라인으로 추정
        if (area > 100 && area < 5000) {
            trueLines.push_back(Rect(left, top, width, height));
            // 라인 중심 계산
            Point center(left + width / 2, top + height / 2);
            lineCenters.push_back(center);
        }
    }

    // 초기 라인 중심 설정
    Point imageCenter(thr_cut.cols / 2, thr_cut.rows / 2);  // 영상 중심
    if (previousLineCenter.x == -1 && previousLineCenter.y == -1) {//변수가 초기상태인지 확인 
        // 초기에 영상 중심에서 가장 가까운 라인을 선택
        double minDistance = 10000;

        for (const Point& center : lineCenters) {
            double distance = norm(center - imageCenter) ; //영상중심, 라인중심 거리계산
            if (distance < minDistance) {
                minDistance = distance;
                previousLineCenter = center;
            }
        }
    }

    // 현재 프레임에서 라인을 선택
    double maxAllowedDistance = 80.0;  // 최대 이동 거리 제한
    Point closestLineCenter(-1, -1); 
    double minDistance = 10000;

    for (const Point& center : lineCenters) {
        double distance = norm(center - previousLineCenter); //영상 중심, 이전 라인중심 거리계산
        if (distance < minDistance && distance <= maxAllowedDistance) {
            minDistance = distance;
            closestLineCenter = center;
        } 
    }

    // 선택된 라인 중심이 유효한 경우만 업데이트
    if (minDistance <= maxAllowedDistance) {
        previousLineCenter = closestLineCenter;
    }

    // 위치 오차 계산 및 전역 변수에 저장
    if (previousLineCenter.x != -1) {
        lineError = imageCenter.x - previousLineCenter.x;  // 위치 오차 계산
        cout << "lineError: " << lineError << endl;  // 터미널에 출력
    }
    else {
        cout << "no line detected!" << endl;
    }

    // 모든 라인 후보에 파란색 바운딩 박스 표시
    for (size_t i = 0; i < trueLines.size(); ++i) {
        Rect rect = trueLines[i];
        rectangle(thr_cut, rect, Scalar(255, 0, 0), 2);  // 파란색 바운딩 박스
    }

    // 선택된 라인 중심에 빨간색 표시
    if (closestLineCenter.x != -1 && closestLineCenter.y != -1) {
        for (size_t i = 0; i < trueLines.size(); ++i) {
            Rect rect = trueLines[i];
            if (lineCenters[i] == closestLineCenter) {
                rectangle(thr_cut, rect, Scalar(0, 0, 255), 2);  // 빨간색 바운딩 박스
                circle(thr_cut, closestLineCenter, 2, Scalar(0, 0, 255), 2);  // 빨간색 점
            }
        }
    }
}

int main(void)
{
    VideoCapture source("7_lt_ccw_100rpm_in.mp4"); 
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

    Dxl dxl;
    struct timeval start, end1;
    double time1;

    double target_brightness = 90.0;

    bool mode = false;
    double k = 0.15;

    signal(SIGINT, ctrlc);  // 시그널 핸들러 지정
    if (!dxl.open()) { cout << "dxl open error" << endl; return -1; } // 장치 열기
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

        // 라인 검출 후 lineError 값 사용
        if (dxl.kbhit()) { // 없으면 제어 멈춤
            char ch = dxl.getch();
            if (ch == 'q'){
                dxl.setVelocity(0, 0);
                break;
            } 
            else if (ch == 's') mode = true;
        }

        double leftvel = 0, rightvel = 0;
        // lineError를 기반으로 속도 조정
        leftvel = 100 - k * lineError;
        rightvel = -(100 + k * lineError);

        if (mode) dxl.setVelocity(leftvel, rightvel);
        if (ctrl_c_pressed){
                dxl.setVelocity(0, 0);
                break;
            } 

        gettimeofday(&end1, NULL);
        time1 = end1.tv_sec - start.tv_sec + (end1.tv_usec - start.tv_usec) / 1000000.0;
        cout << "Time: " << time1 << " seconds" << endl;
        cout << "left: " << leftvel << ", right: " << rightvel << endl;
    }

    return 0;
}
