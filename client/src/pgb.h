// いらん
#pragma once
#include <iostream>
#include <string>
#include <vector>

class ProgressBar {
    size_t totalBytes;
    size_t barWidth = 40;

public:
    ProgressBar(size_t total) : totalBytes(total) {}

    void update(size_t downloaded) {
        double fraction = double(downloaded) / totalBytes;
        size_t pos = size_t(barWidth * fraction);
        std::cout << "\r[";
        for (size_t i = 0; i < barWidth; ++i)
            std::cout << (i < pos ? '=' : ' ');
        std::cout << "] " << int(fraction * 100.0) << "% " << downloaded << "/" << totalBytes << " bytes";
        std::cout.flush();
    }

    void finish() {
        std::cout << "\n";
    }
};
