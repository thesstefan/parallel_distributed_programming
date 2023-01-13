#include "sequential_multiplication.h"

Polynomial seq_multiplication(const Polynomial& lhs, const Polynomial &rhs) {
    std::vector<int> result_coefficients(lhs.degree() + rhs.degree() + 1, 0);

    for (int i = 0; i <= lhs.degree(); i++)
        for (int j = 0; j <= rhs.degree(); j++)
            result_coefficients[i + j] += lhs[i] * rhs[j];

    return Polynomial(result_coefficients);
}

Polynomial karatsuba_seq_multiplication(const Polynomial &lhs, const Polynomial &rhs) {
    if (lhs.degree() < 2 || rhs.degree() < 2)
        return seq_multiplication(lhs, rhs);

    auto len = std::max(lhs.degree(), rhs.degree()) / 2;

    auto lhs_low = lhs.get_sub_polynomial(0, len);
    auto lhs_high = lhs.get_sub_polynomial(len, lhs.degree() + 1);
    auto rhs_low = rhs.get_sub_polynomial(0, len);
    auto rhs_high = rhs.get_sub_polynomial(len, rhs.degree() + 1);

    auto z1 = karatsuba_seq_multiplication(lhs_low, rhs_low);
    auto z2 = karatsuba_seq_multiplication(lhs_low + lhs_high, rhs_low + rhs_high);
    auto z3 = karatsuba_seq_multiplication(lhs_high, rhs_high);

    auto r1 = z3 >> 2 * len;
    auto r2 = ((z2 - z3) - z1) >> len;
    return (r1 + r2) + z1;
}
