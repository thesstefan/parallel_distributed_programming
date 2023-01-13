#pragma once

#include <functional>
#include <vector>
#include <fstream>

template <typename T>
using MultiplicationAlgorithm = std::function<T(const T&, const T&)>;

class Polynomial {
    private:
        std::vector<int> coefficients;
        MultiplicationAlgorithm<Polynomial> multiplication_algorithm;

    public:
        Polynomial();

        explicit Polynomial(int degree);
        explicit Polynomial(std::vector<int> coefficients);

        int degree() const;

        const Polynomial& set_multiplication_algorithm(
                MultiplicationAlgorithm<Polynomial> multiplication_algorithm);

        Polynomial get_sub_polynomial(int start_coefficient_index, int end_coefficient_index) const;

        Polynomial operator+(const Polynomial& rhs) const;
        Polynomial operator-(const Polynomial& rhs) const;
        Polynomial operator*(const Polynomial& rhs) const;
        Polynomial operator>>(int shift);
        bool operator==(const Polynomial& rhs) const;

        const int& operator[] (int) const;

        const std::vector<int>& get_coefficients() const;

        friend std::ostream& operator<< (std::ostream& os, const Polynomial& p);

    private:
        void random_init(int degree, int min_coefficient = 0, int max_coefficient = 500);
};
