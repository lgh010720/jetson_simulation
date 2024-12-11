## line detect out

https://youtu.be/9oeD3s59akI

## line detect in

https://youtu.be/vza8JRLOCS0

## motion out

https://youtube.com/shorts/0BaotghH3iE

## motion in

https://youtube.com/shorts/JJNrAo1CPtg

#### 밝기조절 함수:

    void adBright(Mat &frame, double target_brightness)

영상 그레이스케일 이후, 밝기차이 계산, 밝기더하기

#### 라인찾기 함수:

    void findLines(Mat& thr_cut, Mat& thres_cut)

면적을 기준으로 라인 후보 색출, 라인 후보 중심계산

초기 라인 중심 설정, 변수가 초기상태일 경우 가장 가까운 라인 선택, 영상 중심과 라인중심 거리를 계산

최대 이동거리 제한, 영상 중심과 이전라인의 중심 거리계산 최대 이동거리를 넘지 않을경우 업데이트

영상의 중심, 라인의 중심 x좌표 차이를 이용해 위치오차 LineError측정

모든 라인 후보에 파란색 바운딩박스, 선택된 라인에 빨간 바운딩박스, 선택된 라인의 무게중심에 빨간 점

#### mian함수:

원본 영상과 1/4 아래쪽 영상에 이진화, 바운딩박스 등 표시한 영상을 윈도우에 송출, 수신

코드 실행시간과, 모션제어 바퀴의 속도를 터미널창에 출력하는 코드

#### 모션제어 코드:

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
q 혹은 ctrl + c 입력시 로봇 멈춤, s 입력시 로봇 움직임

findline 함수에서 검출한 위치오차 LineError를 바탕으로 왼쪽, 오른쪽 바퀴 속도 제어
