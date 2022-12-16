#include <thread>
#include <iostream>

#include "polynomial.h"
#include "multiplication.h"

static constexpr bool USE_DEFAULT_POLYNOMIALS = false;
static constexpr int RANDOM_POLYNOMIAL_MAX_DEGREE = 10000;

std::pair<Polynomial, Polynomial> setup_polynomials(bool use_default, bool print) {
    if (!use_default)
        return { Polynomial(RANDOM_POLYNOMIAL_MAX_DEGREE), Polynomial(RANDOM_POLYNOMIAL_MAX_DEGREE) };

    Polynomial p1( {5, 1, 4, 6} );
    Polynomial p2( {1, 2, 2, 1} );

    if (print) {
        std::cout << "P1: " << p1 << std::endl;
        std::cout << "P2: " << p2 << std::endl;
        std::cout << std::endl;
    }

    return { p1, p2 };
}

void run(Polynomial& p1, const Polynomial& p2, 
         MultiplicationAlgorithm<Polynomial> algorithm, const std::string& algorithm_name,
         bool print_product = false) {

    p1.set_multiplication_algorithm(algorithm);

    std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();

    std::cout << algorithm_name << std::endl;
    auto product = p1 * p2;
    if (print_product) {
        std::cout << "P1 * P2: " << product << std::endl;
    }

    std::chrono::system_clock::time_point stopTime = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stopTime - startTime).count();

    std::cout << "Execution time = " << duration << "us" << std::endl << std::endl;
}

int main() {
    auto [p1, p2] = setup_polynomials(USE_DEFAULT_POLYNOMIALS, USE_DEFAULT_POLYNOMIALS);

    run(p1, p2, seq_multiplication, "Simple sequential multiplication", USE_DEFAULT_POLYNOMIALS);
    run(p1, p2, karatsuba_seq_multiplication, "Karatsuba sequential multiplication", USE_DEFAULT_POLYNOMIALS);
    run(p1, p2, parallel_multiplication, "Simple parallel multiplication", USE_DEFAULT_POLYNOMIALS);
    run(p1, p2, karatsuba_parallel_multiplication, "Karatsuba parallel multiplication", USE_DEFAULT_POLYNOMIALS);

    return 0;
}
