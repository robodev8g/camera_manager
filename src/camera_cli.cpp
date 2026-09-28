#include "camera_cli.hpp"

#include <iostream>
#include <string>

namespace camera_manager {

CameraCli::CameraCli(ICameraAdapter& camera) : camera_(camera) {}

void CameraCli::print_menu() const {
    std::cout << "\nCamera Manager\n"
              << "1. Take snapshot\n"
              << "2. Start recording\n"
              << "3. Stop recording\n"
              << "4. Live view\n"
              << "0. Exit\n"
              << "> ";
}

void CameraCli::run() {
    while (true) {
        print_menu();

        int command = 0;
        std::cin >> command;

        switch (command) {
        case 1: {
            std::string output;
            std::cout << "Snapshot path: ";
            std::cin >> output;
            camera_.take_snapshot(output);
            std::cout << "Snapshot saved to " << output << '\n';
            break;
        }
        case 2: {
            std::string output;
            std::cout << "Recording path: ";
            std::cin >> output;
            camera_.start_record(output);
            std::cout << "Recording started\n";
            break;
        }
        case 3:
            camera_.stop_record();
            std::cout << "Recording stopped\n";
            break;
        case 4:
            std::cout << "Press q or Esc to close live view\n";
            camera_.live_view();
            break;
        case 0:
            return;
        }
    }
}

}  // namespace camera_manager
