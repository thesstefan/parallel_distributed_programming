#pragma once

#include "polynomial.h"

Polynomial seq_multiplication(const Polynomial& lhs, const Polynomial& rhs);
Polynomial karatsuba_seq_multiplication(const Polynomial& lhs, const Polynomial& rhs);

Polynomial parallel_multiplication(const Polynomial& lhs, const Polynomial& rhs);
Polynomial karatsuba_parallel_multiplication(const Polynomial& lhs, const Polynomial& rhs);
