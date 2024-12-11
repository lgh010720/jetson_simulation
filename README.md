## line detect out

https://youtu.be/9oeD3s59akI

## line detect in

https://youtu.be/vza8JRLOCS0

밝기조절 함수:

    void adBright(Mat &frame, double target_brightness)

영상 그레이스케일 이후, 밝기차이 계산, 밝기더하기

라인찾기 함수:

    void findLines(Mat& thr_cut, Mat& thres_cut)

면적을 기준으로 라인 후보 영역을 색출, 영역의 무게중심 계산

이미지의 중심을 계산, 이미지 중심과 영역의 무게중심의 차이 계산
