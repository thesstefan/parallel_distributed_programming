#pragma once

#include "../polynomial.h"

Polynomial parallel_multiplication(const Polynomial& lhs, const Polynomial& rhs);
Polynomial karatsuba_parallel_multiplication(const Polynomial &lhs, const Polynomial &rhs);
