#include "polynomial.h"
#include "multiplication/sequential_multiplication.h"

#include <random>
#include <ostream>

Polynomial::Polynomial() {}

Polynomial::Polynomial(int degree) {
    coefficients.reserve(degree + 1);
    random_init(degree);

    multiplication_algorithm = seq_multiplication;
}

Polynomial::Polynomial(std::vector<int> coefficients) : coefficients(std::move(coefficients)) {
    multiplication_algorithm = seq_multiplication;
}

void Polynomial::random_init(int degree, int min_coefficient, int max_coefficient) {
    std::random_device random_device;
    std::mt19937 gen(random_device());
    std::uniform_int_distribution<> distribution(min_coefficient, max_coefficient);

    for (int i = 0; i <= degree; ++i) {
        coefficients.push_back(distribution(gen));
    }
}

const std::vector<int>& Polynomial::get_coefficients() const {
    return coefficients;
}

Polynomial Polynomial::operator+(const Polynomial& rhs) const {
    int minDegree = std::min(degree(), rhs.degree());
    int maxDegree = std::max(degree(), rhs.degree());
    
    std::vector<int> result_coefficients(maxDegree + 1, 0);

    for (int i = 0; i <= minDegree; ++i) {
        result_coefficients[i] = coefficients[i] + rhs[i];
    }

    if (maxDegree == degree()) {
        for (int i = minDegree + 1; i <= maxDegree; ++i) {
            result_coefficients[i] = coefficients[i];
        }
    }
    else {
        for (int i = minDegree + 1; i <= maxDegree; ++i) {
            result_coefficients[i] = rhs[i];
        }
    }

    int i = result_coefficients.size() - 1;
    while (result_coefficients[i] == 0 && i > 0) {
        result_coefficients.pop_back();
        i--;
    }

    return Polynomial(result_coefficients);
}

Polynomial Polynomial::operator-(const Polynomial& rhs) const {
    int minDegree = std::min(degree(), rhs.degree());
    int maxDegree = std::max(degree(), rhs.degree());

    std::vector<int> result_coefficients(maxDegree + 1, 0);

    for (int i = 0; i <= minDegree; ++i) {
        result_coefficients[i] = coefficients[i] - rhs[i];
    }

    if (maxDegree == degree()) {
        for (int i = minDegree + 1; i <= maxDegree; ++i) {
            result_coefficients[i] = coefficients[i];
        }
    }
    else {
        for (int i = minDegree + 1; i <= maxDegree; ++i) {
            result_coefficients[i] = -rhs[i];
        }
    }

    int i = result_coefficients.size() - 1;
    while (result_coefficients[i] == 0 && i > 0) {
        result_coefficients.pop_back();
        i--;
    }

    return Polynomial(result_coefficients);
}

Polynomial Polynomial::operator*(const Polynomial& rhs) const {
    return multiplication_algorithm(*this, rhs);
}

Polynomial Polynomial::operator>>(int shift) {
    std::vector<int> new_coefficients(shift, 0);
    new_coefficients.insert(new_coefficients.end(), coefficients.begin(), coefficients.end());
    return Polynomial(new_coefficients);
}

const Polynomial& Polynomial::set_multiplication_algorithm(
        MultiplicationAlgorithm<Polynomial> multiplication_algorithm) {
    this->multiplication_algorithm = std::move(multiplication_algorithm);

    return *this;
}

Polynomial Polynomial::get_sub_polynomial(int start_coefficient_index, int end_coefficient_index) const {
    return Polynomial(std::vector<int>(
                coefficients.begin() + start_coefficient_index, 
                coefficients.begin() + end_coefficient_index));
}

int Polynomial::degree() const {
    return coefficients.size() - 1;
}

const int& Polynomial::operator[](int index) const {
    return coefficients[index];
}

std::ostream& operator<<(std::ostream& os, const Polynomial& p) {
    for (std::size_t i = 0; i < p.coefficients.size(); ++i) {
        if (!p.coefficients[i])
            continue;

        os << p.coefficients[i] << " * x^" << i;
        if (i != p.coefficients.size() - 1) {
            os << " + ";
        }
    }

    return os;
}

bool Polynomial::operator==(const Polynomial& rhs) const {
    return coefficients == rhs.coefficients;
}
