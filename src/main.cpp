/**
 * @file main.cpp
 * @brief Консольная утилита: считает цены Call и Put по Блэку–Шоулзу.
 *
 * Использование: bs_app [S K r sigma T]
 * Без аргументов считает пример S=100, K=100, r=0.05, sigma=0.2, T=1.
 */
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "quant/black_scholes.hpp"

int main(int argc, char* argv[]) {
    quant::OptionParams p{100.0, 100.0, 0.05, 0.2, 1.0};

    if (argc == 6) {
        try {
            p = {std::stod(argv[1]), std::stod(argv[2]), std::stod(argv[3]), std::stod(argv[4]),
                 std::stod(argv[5])};
        } catch (const std::exception&) {
            std::cerr << "Error: arguments must be numbers\n";
            return EXIT_FAILURE;
        }
    } else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [S K r sigma T]\n";
        return EXIT_FAILURE;
    }

    double call = 0.0;
    double put = 0.0;
    try {
        call = quant::callPrice(p);
        put = quant::putPrice(p);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << std::fixed << std::setprecision(4) << "S=" << p.spot << " K=" << p.strike
              << " r=" << p.rate << " sigma=" << p.volatility << " T=" << p.maturity << '\n'
              << "Call: " << call << '\n'
              << "Put:  " << put << '\n';
    return EXIT_SUCCESS;
}
