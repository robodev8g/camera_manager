#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    const std::string device = argc > 1 ? argv[1] : "/dev/video0";
    cv::VideoCapture camera(device, cv::CAP_V4L2);

    if (!camera.isOpened()) {
        std::cerr << "Could not open camera: " << device << '\n';
        return 1;
    }

    std::cout << "Showing " << device << ". Press q or Esc to quit.\n";
    cv::Mat frame;
    while (camera.read(frame)) {
        cv::imshow("Live camera", frame);
        const int key = cv::waitKey(1);
        if (key == 'q' || key == 27) {
            return 0;
        }
    }

    std::cerr << "The camera stopped producing frames.\n";
    return 1;
}
