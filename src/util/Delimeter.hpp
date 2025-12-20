#pragma once

#include <string_view>

class Delimeter {
    std::string_view deli;
    bool             isFirstTime = true;

   public:
    Delimeter(std::string_view deli) : deli(deli) {}

    std::string_view operator()() {
        if (isFirstTime) {
            isFirstTime = false;
            return "";
        } else
            return deli;
    }

    void reset() {
        isFirstTime = true;
    }
};
