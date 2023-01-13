#include <chrono>
#include <mpi.h>

#include "multiplication/sequential_multiplication.h"
#include "polynomial.h"

namespace multi {
    const int CHIEF_RANK = 0;

    const int LHS_SIZE_TAG = 0;
    const int LHS_COEF_TAG = 1;

    const int RHS_SIZE_TAG = 2;
    const int RHS_COEF_TAG = 3;

    const int RESULT_SIZE_TAG = 4;
    const int RESULT_COEF_TAG = 5;

    const int Z1_LHS_SIZE_TAG = 0;
    const int Z1_LHS_COEF_TAG = 1;
    const int Z1_RHS_SIZE_TAG = 2;
    const int Z1_RHS_COEF_TAG = 3;
    const int Z1_RESULT_COEF_TAG = 4;
    const int Z1_RESULT_SIZE_TAG = 5;

    const int Z2_LHS_SIZE_TAG = 6;
    const int Z2_LHS_COEF_TAG = 7;
    const int Z2_RHS_SIZE_TAG = 8;
    const int Z2_RHS_COEF_TAG = 9;
    const int Z2_RESULT_COEF_TAG = 10;
    const int Z2_RESULT_SIZE_TAG = 11;

    const int Z3_LHS_SIZE_TAG = 12;
    const int Z3_LHS_COEF_TAG = 13;
    const int Z3_RHS_SIZE_TAG = 14;
    const int Z3_RHS_COEF_TAG = 15;
    const int Z3_RESULT_COEF_TAG = 16;
    const int Z3_RESULT_SIZE_TAG = 17;
}

std::vector<std::unordered_map<std::string, std::pair<int, int>>> KARATSUBA_POLY_TAG_PAIRS = {
    { 
        {"lhs",     { multi::Z1_LHS_SIZE_TAG, multi::Z1_LHS_COEF_TAG }},
        {"rhs",     { multi::Z1_RHS_SIZE_TAG, multi::Z1_RHS_COEF_TAG }},
        {"res",  { multi::Z1_RESULT_SIZE_TAG, multi::Z1_RESULT_COEF_TAG }}
    },
    { 
        {"lhs",     { multi::Z2_LHS_SIZE_TAG, multi::Z2_LHS_COEF_TAG }},
        {"rhs",     { multi::Z2_RHS_SIZE_TAG, multi::Z2_RHS_COEF_TAG }},
        {"res",  { multi::Z2_RESULT_SIZE_TAG, multi::Z2_RESULT_COEF_TAG }}
    },
    { 
        {"lhs",     { multi::Z3_LHS_SIZE_TAG, multi::Z3_LHS_COEF_TAG }},
        {"rhs",     { multi::Z3_RHS_SIZE_TAG, multi::Z3_RHS_COEF_TAG }},
        {"res",  { multi::Z3_RESULT_SIZE_TAG, multi::Z3_RESULT_COEF_TAG }}
    }
};

void send_polynomial(const Polynomial& poly, int destination_rank, std::pair<int, int> poly_tags) {
    int size_tag = poly_tags.first;
    int coef_tag = poly_tags.second;
    int coef_size = poly.degree() + 1;

    MPI_Ssend(&coef_size, 1, MPI_INT, destination_rank,
            size_tag, MPI_COMM_WORLD);
    MPI_Ssend(poly.get_coefficients().data(), coef_size, MPI_INT, destination_rank, 
            coef_tag, MPI_COMM_WORLD);
}

Polynomial recv_polynomial(int source_rank, std::pair<int, int> poly_tags) {
    int size_tag = poly_tags.first;
    int coef_tag = poly_tags.second;

    MPI_Status status;

    int coef_size;
    MPI_Recv(&coef_size, 1, MPI_INT, source_rank, size_tag, MPI_COMM_WORLD, &status);

    std::vector<int> poly_coef(coef_size, 0);
    MPI_Recv(poly_coef.data(), coef_size, MPI_INT, source_rank, coef_tag, MPI_COMM_WORLD, &status);

    return Polynomial(poly_coef);
}

Polynomial chief_multiply(const Polynomial& lhs, const Polynomial& rhs, int cluster_size) {
    int elements_by_node = (lhs.degree() + 1) / (cluster_size - 1);
    int remaining_elements = (lhs.degree() + 1) % (cluster_size - 1);
    int used_elements = 0;

    int rhs_size = rhs.degree() + 1;

    for (int worker_node_rank = 1; worker_node_rank < cluster_size; worker_node_rank++) {
        int coef_start_index = (worker_node_rank - 1) * elements_by_node + used_elements;
        int coef_end_index = coef_start_index + elements_by_node;

        if (remaining_elements > 0) {
            coef_end_index++;
            remaining_elements--;
            used_elements++;
        }

        const Polynomial split_poly = 
            lhs.get_sub_polynomial(coef_start_index, coef_end_index) >> coef_start_index;
        int split_poly_size = split_poly.degree() + 1;

        send_polynomial(split_poly, worker_node_rank, {multi::LHS_SIZE_TAG, multi::LHS_COEF_TAG});
        send_polynomial(rhs, worker_node_rank, {multi::RHS_SIZE_TAG, multi::RHS_COEF_TAG});
    }

    Polynomial result;
    for (int worker_node_rank = 1; worker_node_rank < cluster_size; worker_node_rank++) {
        Polynomial partial_poly = recv_polynomial(worker_node_rank,
                {multi::RESULT_SIZE_TAG, multi::RESULT_COEF_TAG});

        result = result + partial_poly;
    }

    return result;
}

void worker_multiply() {
    MPI_Status status;

    Polynomial lhs = recv_polynomial(multi::CHIEF_RANK, {multi::LHS_SIZE_TAG, multi::LHS_COEF_TAG});
    Polynomial rhs = recv_polynomial(multi::CHIEF_RANK, {multi::RHS_SIZE_TAG, multi::RHS_COEF_TAG});

    const Polynomial result = lhs * rhs;

    send_polynomial(result, multi::CHIEF_RANK, {multi::RESULT_SIZE_TAG, multi::RESULT_COEF_TAG});
}

Polynomial karatsuba_multiply(const Polynomial& lhs, const Polynomial& rhs, int cluster_size, int rank) {
    if (lhs.degree() < 2 || rhs.degree() < 2)
        return seq_multiplication(lhs, rhs);

    auto len = std::max(lhs.degree(), rhs.degree()) / 2;

    auto lhs_low = lhs.get_sub_polynomial(0, len);
    auto lhs_high = lhs.get_sub_polynomial(len, lhs.degree() + 1);
    auto rhs_low = rhs.get_sub_polynomial(0, len);
    auto rhs_high = rhs.get_sub_polynomial(len, rhs.degree() + 1);

    Polynomial z1;
    Polynomial z2;
    Polynomial z3;

    if (rank * 3 + 1 < cluster_size) {
        send_polynomial(lhs_low, rank * 3 + 1, {multi::Z1_LHS_SIZE_TAG, multi::Z1_LHS_COEF_TAG});
        send_polynomial(rhs_low, rank * 3 + 1, {multi::Z1_RHS_SIZE_TAG, multi::Z1_RHS_COEF_TAG});
    } else
        z1 = karatsuba_seq_multiplication(lhs_low, rhs_low);

    if (rank * 3 + 2 < cluster_size) {
        send_polynomial(lhs_low + lhs_high, rank * 3 + 2, {multi::Z2_LHS_SIZE_TAG, multi::Z2_LHS_COEF_TAG});
        send_polynomial(rhs_low + rhs_high, rank * 3 + 2, {multi::Z2_RHS_SIZE_TAG, multi::Z2_RHS_COEF_TAG});
    } else
        z2 = karatsuba_seq_multiplication(lhs_low + lhs_high, rhs_low + rhs_high);

    if (rank * 3 + 3 < cluster_size) {
        send_polynomial(lhs_high, rank * 3 + 3, {multi::Z3_LHS_SIZE_TAG, multi::Z3_LHS_COEF_TAG});
        send_polynomial(rhs_high, rank * 3 + 3, {multi::Z3_RHS_SIZE_TAG, multi::Z3_RHS_COEF_TAG});
    } else
        z3 = karatsuba_seq_multiplication(lhs_high, rhs_high);

    if (rank * 3 + 1 < cluster_size)
        z1 = recv_polynomial(rank * 3 + 1, {multi::Z1_RESULT_SIZE_TAG, multi::Z1_RESULT_COEF_TAG});
    if (rank * 3 + 2 < cluster_size)
        z2 = recv_polynomial(rank * 3 + 2, {multi::Z2_RESULT_SIZE_TAG, multi::Z2_RESULT_COEF_TAG});
    if (rank * 3 + 3 < cluster_size)
        z3 = recv_polynomial(rank * 3 + 3, {multi::Z3_RESULT_SIZE_TAG, multi::Z3_RESULT_COEF_TAG});

    auto r1 = z3 >> 2 * len;
    auto r2 = ((z2 - z3) - z1) >> len;
    return (r1 + r2) + z1;
}

Polynomial chief_karatsuba(const Polynomial& lhs, const Polynomial& rhs, int cluster_size) {
    return karatsuba_multiply(lhs, rhs, cluster_size, multi::CHIEF_RANK);
}

void worker_karatsuba(int cluster_size) {
    int process_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);

    int parent_rank = (process_rank - 1) / 3;
    int z_index = (process_rank + 2) % 3;

    const Polynomial lhs = recv_polynomial(parent_rank, KARATSUBA_POLY_TAG_PAIRS[z_index]["lhs"]);
    const Polynomial rhs = recv_polynomial(parent_rank, KARATSUBA_POLY_TAG_PAIRS[z_index]["rhs"]);

    const Polynomial res = karatsuba_multiply(lhs, rhs, cluster_size, process_rank);

    send_polynomial(res, parent_rank, KARATSUBA_POLY_TAG_PAIRS[z_index]["res"]);
}

static constexpr bool USE_KARATSUBA = false; 

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int cluster_size;
    MPI_Comm_size(MPI_COMM_WORLD, &cluster_size);

    int process_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);

    if (process_rank == multi::CHIEF_RANK) {
        const Polynomial lhs(10000);
        const Polynomial rhs(10000);

        std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();

        Polynomial result;
        if (USE_KARATSUBA) {
            std::cout << "Karatsuba MPI" << std::endl;
            result = chief_karatsuba(lhs, rhs, cluster_size);
        } else {
            std::cout << "Simple MPI" << std::endl;
            result = chief_multiply(lhs, rhs, cluster_size);
        }

        if (!(result == lhs * rhs))
            throw std::runtime_error("Something went wrong :(");
        else
            std::cout << std::endl << "Correct :)" << std::endl;

        std::chrono::system_clock::time_point stopTime = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime).count();
        std::cout << "Execution time = " << duration << "ms" << std::endl;
    } else {
        if (USE_KARATSUBA)
            worker_karatsuba(cluster_size);
        else
            worker_multiply();
    }

    MPI_Finalize();

    return 0;
}
