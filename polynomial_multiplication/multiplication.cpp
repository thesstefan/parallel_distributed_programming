#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#include <thread>
#include <future>

#include "multiplication.h"

boost::asio::thread_pool thread_pool(std::thread::hardware_concurrency());

Polynomial seq_multiplication(const Polynomial& lhs, const Polynomial &rhs) {
    std::vector<int> result_coefficients(lhs.degree() + rhs.degree() + 1, 0);

    for (int i = 0; i <= lhs.degree(); i++)
        for (int j = 0; j <= rhs.degree(); j++)
            result_coefficients[i + j] += lhs[i] * rhs[j];

    return Polynomial(result_coefficients);
}

Polynomial parallel_multiplication(const Polynomial& lhs, const Polynomial &rhs) {
    std::vector<int> result_coefficients(lhs.degree() + rhs.degree() + 1, 0);

    for (int exponent = 0; exponent <= lhs.degree() + rhs.degree(); ++exponent) {
        boost::asio::post(thread_pool, 
                [&lhs, &rhs, &result_coefficients, exponent]() {
            for (int lhs_exponent = std::max(0, exponent - rhs.degree()); 
                    lhs_exponent <= lhs.degree() 
                    && lhs_exponent <= exponent 
                    && exponent - lhs_exponent <= rhs.degree(); ++lhs_exponent) {

                int rhs_exponent = exponent - lhs_exponent;
                result_coefficients[exponent] += lhs[lhs_exponent] * rhs[rhs_exponent];
            }
        });
    }

    thread_pool.join();
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

Polynomial karatsuba_parallel_helper(const Polynomial& lhs, const Polynomial& rhs, 
                                     int depth, int max_depth = 4) {
    if (depth >= max_depth || (lhs.degree() < 2 || rhs.degree() < 2))
        return seq_multiplication(lhs, rhs);

    auto len = std::max(lhs.degree(), rhs.degree()) / 2;

    auto lhs_low = lhs.get_sub_polynomial(0, len);
    auto lhs_high = lhs.get_sub_polynomial(len, lhs.degree() + 1);
    auto rhs_low = rhs.get_sub_polynomial(0, len);
    auto rhs_high = rhs.get_sub_polynomial(len, rhs.degree() + 1);

    std::promise<Polynomial> promiseZ1;
    auto futureZ1 = promiseZ1.get_future();

    std::promise<Polynomial> promiseZ2;
    auto futureZ2 = promiseZ2.get_future();

    std::promise<Polynomial> promiseZ3;
    auto futureZ3 = promiseZ3.get_future();

    std::thread tZ1([&lhs_low, &rhs_low, &depth](std::promise<Polynomial>&& promise) {
        promise.set_value(karatsuba_parallel_helper(lhs_low, rhs_low, depth + 1));
    }, std::move(promiseZ1));

    std::thread tZ2([&lhs_low, &lhs_high, &rhs_low, &rhs_high, &depth](std::promise<Polynomial>&& promise) {
        promise.set_value(karatsuba_parallel_helper(lhs_low + lhs_high, rhs_low + rhs_high, depth + 1));
    }, std::move(promiseZ2));

    std::thread tZ3([&lhs_high, &rhs_high, &depth](std::promise<Polynomial>&& promise) {
        promise.set_value(karatsuba_parallel_helper(lhs_high, rhs_high, depth + 1));
    }, std::move(promiseZ3));

    tZ1.join();
    tZ2.join();
    tZ3.join();

    auto z1 = futureZ1.get();
    auto z2 = futureZ2.get();
    auto z3 = futureZ3.get();
    
    auto r1 = z3 >> 2 * len;
    auto r2 = ((z2 - z3) - z1) >> len;
    return (r1 + r2) + z1;
}

Polynomial karatsuba_parallel_multiplication(const Polynomial &lhs, const Polynomial &rhs) {
    return karatsuba_parallel_helper(lhs, rhs, 0);
}
