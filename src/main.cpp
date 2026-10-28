/**
 * @file main.cpp
 * @brief Точка входа: печатает приветствие в stdout.
 */
#include <iostream>

#include "hello/hello.hpp"

int main() {
    std::cout << hello::greeting() << '\n';
    return 0;
}
