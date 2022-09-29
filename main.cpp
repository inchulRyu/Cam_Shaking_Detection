#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/time.h>

// using namespace cv;
using namespace std;

#define BLOCK 60

bool blur_detect_fft(cv::Mat & frame, cv::Mat & final_img, double & res){

    int cx = frame.cols/2;
    int cy = frame.rows/2;

    cv::Mat grayImage;
    cv::cvtColor(frame, grayImage, cv::COLOR_BGR2GRAY);

    // Go float
    cv::Mat fImage;
    grayImage.convertTo(fImage, CV_32F);

    // FFT
    // cout << "Direct transform...\n";
    cv::Mat fourierTransform;
    cv::dft(fImage, fourierTransform, cv::DFT_SCALE|cv::DFT_COMPLEX_OUTPUT);

    //center low frequencies in the middle
    //by shuffling the quadrants.
    cv::Mat q0(fourierTransform, cv::Rect(0, 0, cx, cy));       // Top-Left - Create a ROI per quadrant
    cv::Mat q1(fourierTransform, cv::Rect(cx, 0, cx, cy));      // Top-Right
    cv::Mat q2(fourierTransform, cv::Rect(0, cy, cx, cy));      // Bottom-Left
    cv::Mat q3(fourierTransform, cv::Rect(cx, cy, cx, cy));     // Bottom-Right

    cv::Mat tmp;                                            // swap quadrants (Top-Left with Bottom-Right)
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    q1.copyTo(tmp);                                     // swap quadrant (Top-Right with Bottom-Left)
    q2.copyTo(q1);
    tmp.copyTo(q2);

    // Block the low frequencies
    // #define BLOCK could also be a argument on the command line of course
    fourierTransform(cv::Rect(cx-BLOCK,cy-BLOCK,2*BLOCK,2*BLOCK)).setTo(0);

    //shuffle the quadrants to their original position
    cv::Mat orgFFT;
    fourierTransform.copyTo(orgFFT);
    cv::Mat p0(orgFFT, cv::Rect(0, 0, cx, cy));       // Top-Left - Create a ROI per quadrant
    cv::Mat p1(orgFFT, cv::Rect(cx, 0, cx, cy));      // Top-Right
    cv::Mat p2(orgFFT, cv::Rect(0, cy, cx, cy));      // Bottom-Left
    cv::Mat p3(orgFFT, cv::Rect(cx, cy, cx, cy));     // Bottom-Right

    p0.copyTo(tmp);
    p3.copyTo(p0);
    tmp.copyTo(p3);

    p1.copyTo(tmp);                                     // swap quadrant (Top-Right with Bottom-Left)
    p2.copyTo(p1);
    tmp.copyTo(p2);

    // IFFT
    // cout << "Inverse transform...\n";
    cv::Mat invFFT;
    cv::Mat logFFT;
    double minVal,maxVal;

    cv::dft(orgFFT, invFFT, cv::DFT_INVERSE|cv::DFT_REAL_OUTPUT);

    //img_fft = 20*numpy.log(numpy.abs(img_fft))
    invFFT = cv::abs(invFFT);
    cv::minMaxLoc(invFFT,&minVal,&maxVal,NULL,NULL);
    
    //check for impossible values
    if(maxVal<=0.0){
        cerr << "No information, complete black image!\n";
        return false;
    }

    cv::log(invFFT,logFFT);
    logFFT *= 20;

    //result = numpy.mean(img_fft)
    cv::Scalar result= cv::mean(logFFT);
    res = result.val[0];

    // show if you like
    // logFFT.convertTo(final_img, CV_8U);    // Back to 8-bits
    // logFFT.convertTo(final_img, CV_32F);

    return true;
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        fprintf(stderr, "Error: missing image file\n");
        return 1;
    }
    string file_path = argv[1];

    cout << "Processing " << file_path << std::endl;

    double threshold = 15.f;

    if (file_path.substr(file_path.rfind(".") + 1) == "mp4"){
        // video
        cv::VideoCapture cap(file_path);

        cv::namedWindow("OG");
        // cv::namedWindow("Motion");

        double res = 0.f;
        double default_blur_val = 0;
        double tmp_default_blur_val = 0;
        double sum_tmp_default_blur_val = 0;
        int cnt_tmp_default_blur_val = 1;
        float margin_blur_val = 3.f;
        int interval = 2;
        int shaking_print = 0;

        bool first_turn = true;

        struct timeval start_t, end_t;
        double diff_t;
        double trial_period = 1.f;
        bool set_default_blur_val = false;

        while (true){
            cv::Mat frame;
            cv::Mat output;

            cap >> frame;
            if (frame.empty()) break;

            int frame_num = cap.get(cv::CAP_PROP_POS_FRAMES);

            if (frame_num % interval == 0){
                if (!blur_detect_fft(frame, output, res)){
                    printf("Result : failed blur detect fft\n");
                    continue;
                }
            }

            double blur_diff = tmp_default_blur_val - res;
            if (blur_diff < 0) blur_diff *= -1.f; // 차이 계산

            if (first_turn){
                tmp_default_blur_val = res;
                gettimeofday(&start_t, NULL); // 기본 값 측정 시작.
                set_default_blur_val = true;
                first_turn = false;
            }

            // cout << "left : " << diff_t << " set : " << set_default_blur_val << endl;

            // 만약 차이가 허용차이를 한번이라도 넘었을 때
            if (blur_diff >= margin_blur_val){ 
                // 현재 blur val 을 다시 tmp 에 넣음.
                tmp_default_blur_val = res;
                gettimeofday(&start_t, NULL); // 기본 값 측정 다시 시작.
                sum_tmp_default_blur_val = 0;
                cnt_tmp_default_blur_val = 0;
                cout << "Default Blur Value Setting Mode" << endl;
                set_default_blur_val = true;
            }

            gettimeofday(&end_t, NULL);

            diff_t = ( end_t.tv_sec - start_t.tv_sec ) + ((double)( end_t.tv_usec - start_t.tv_usec ) / 1000000);

            // 심판 기간일 때
            if (set_default_blur_val){
                // 평균 계산용
                sum_tmp_default_blur_val += res;
                cnt_tmp_default_blur_val ++;

                // 심판 기간동안 견디면 현재값이 기본값이라는 뜻. (허용차이 넘어갈 때마다 시작 시간이 초기화되므로)
                if (diff_t >= trial_period){
                    default_blur_val = sum_tmp_default_blur_val / cnt_tmp_default_blur_val; // 심판 기간 동안 값들의 평균을 기본값으로 넣음.
                    // default_blur_val = tmp_default_blur_val; // 임시값을 기본값으로 넣음.
                    cout << "Set Default Blur Value! : " << default_blur_val << endl;
                    set_default_blur_val = false;
                }
            }


            cout << "Result : "<< res << endl;

            int fontFace = cv::FONT_HERSHEY_PLAIN;
            double fontScale = 2;
            int thickness = 2;
            cv::Point text_coord(0, 0);

            int baseline=0;

            string text = to_string(res);

            cv::Size textSize = cv::getTextSize(text, fontFace,
                                fontScale, thickness, &baseline);

            baseline += thickness;

            // left top the text
            cv::Point textOrg(text_coord.x, text_coord.y + textSize.height);

            // // draw the box
            // cv::rectangle(frame, textOrg + cv::Point(0, baseline),
            //             textOrg + cv::Point(textSize.width, -textSize.height),
            //             cv::Scalar(0,0,0), -1);

            cv::putText(frame, text, textOrg + cv::Point(0, int(baseline/2)), 
                        fontFace, fontScale, cv::Scalar::all(255), thickness, 8);

            string default_blur_val_text = to_string(default_blur_val);

            cv::Scalar default_blur_val_text_color = cv::Scalar::all(255);
            if (set_default_blur_val){
                default_blur_val_text_color = cv::Scalar(0,0,255);
            }
            cv::putText(frame, default_blur_val_text, textOrg + cv::Point(0, baseline + textSize.height), 
                        fontFace, fontScale, default_blur_val_text_color, thickness, 8);

            // Cam Shaking 판단
            // 기본값에서 쓰레쉬홀드를 뺀거보다 작으면
            if (shaking_print == 0 && res <= default_blur_val - threshold){ 
                shaking_print += 30;
            }

            // cout << shaking_print << endl;

            if (shaking_print > 0){
                string shaking_text = "Cam Shaking!";
                double shaking_text_scale = 5;
                int shaking_text_thickness = 6;
                int shaking_text_fontFace = cv::FONT_HERSHEY_PLAIN;
                int shaking_baseline = 0;

                cv::Size shaking_textSize = cv::getTextSize(shaking_text, shaking_text_fontFace,
                                    shaking_text_scale, shaking_text_thickness, &shaking_baseline);

                shaking_baseline += shaking_text_thickness;

                cv::Point shaking_textOrg(( frame.cols - shaking_textSize.width )/2 , ( frame.rows + shaking_textSize.height )/2);

                cv::putText(frame, shaking_text, shaking_textOrg + cv::Point(0, int(shaking_baseline/2)), 
                            shaking_text_fontFace, shaking_text_scale, cv::Scalar(0,0,255), shaking_text_thickness, 8);
                shaking_print --;
            }

            cv::moveWindow("OG", 0, 0);
            // cv::moveWindow("Motion", 640, 0);
            
            cv::imshow("OG", frame);
            // cv::imshow("Motion", output);   
 
            if (cv::waitKey(1) == 27)
                break;                
        }

        cap.release();
        cv::destroyAllWindows();
    }else{
        // cv::Mat frame = cv::imread(file_path, cv::IMREAD_GRAYSCALE);
        cv::Mat frame = cv::imread(file_path);
        cv::Mat output;

        double res = 0.f;

        if (!blur_detect_fft(frame, output, res)){
            printf("failed blur detect fft\n");
        }

        cout << "Result : "<< res << endl;

        cv::imshow("Input", frame);
        // cv::imshow("Result", output);
        cv::waitKey(0);
    }


    return 0;
}