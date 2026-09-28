#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>

int main() {
    cv::VideoCapture camera(0);
    if (!camera.isOpened()) {
        std::cerr << "Could not open the camera.\n";
        return 1;
    }

    std::cout << "Press q or Esc to quit.\n";

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
