#include <vector>
#include <unordered_map>
#include <set>
#include <iostream>
#include <cassert>
#include <functional>
#include <thread>

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

static constexpr unsigned int TASK_COUNT = 10;
static constexpr unsigned int THREAD_POOL_SIZE = 5;

class Matrix {
    private:
        std::unordered_map<std::size_t, std::vector<int32_t>> col_cache;

    public:
        std::vector<std::vector<int32_t>> rows;
        const std::size_t width;
        const std::size_t height;

        Matrix(std::vector<std::vector<int32_t>>&& rows)
            : rows(rows), width(rows[0].size()), height(rows.size()) {
        }

        Matrix(std::size_t width, std::size_t height, int32_t value = 0) 
            : rows(height, std::vector<int32_t>(width, value)), width(width), height(height) {}

        const std::vector<int32_t>& get_row(std::size_t row_index) const {
            return rows.at(row_index);
        }

        void set(std::size_t row_index, std::size_t col_index, int32_t value) {
            rows[row_index][col_index] = value;
        }

        std::vector<int32_t> get_col(std::size_t col_index) const {
            std::vector<int32_t> col;
            col.reserve(height);
            for (const auto& row : rows)
                col.push_back(row.at(col_index));

            return col;
        }
};

int32_t dot_product(const std::vector<int32_t>& a, const std::vector<int32_t>& b) {
    assert(a.size() == b.size());

    int32_t result = 0;
    for (std::size_t index = 0; index < a.size(); index++)
        result += a[index] * b[index];

    return result;
}

int32_t compute_element(std::size_t row_index, std::size_t col_index,
                        const Matrix& A, const Matrix& B) {
    assert(A.width == B.height);

    return dot_product(A.get_row(row_index), B.get_col(col_index));
}


class Task {
    const Matrix& A;
    const Matrix& B;

    Matrix& result;

    public:
        std::set<std::pair<std::size_t, std::size_t>> requested_indices;
        Task(const Matrix& A, const Matrix& B, Matrix& result) :
            A(A), B(B), result(result) {}

        void request_element(std::size_t row_index, std::size_t col_index) {
            requested_indices.insert({row_index, col_index});
        }

        void compute_and_write_element(std::size_t row_index, std::size_t col_index) {
            result.rows[row_index][col_index] = compute_element(row_index, col_index, A, B);
        }

        void execute() {
            for (auto &[row_index, col_index]: requested_indices)
                compute_and_write_element(row_index, col_index);
        }
};

std::vector<Task> multiply_matrix_row_tasks(const Matrix& A, const Matrix& B, Matrix& result) {
        std::vector<Task> tasks(TASK_COUNT, Task(A, B, result));

        std::size_t elements_computed = A.height * B.width;
        std::size_t op_per_task = elements_computed / TASK_COUNT;
        int remainder = elements_computed % TASK_COUNT;

        std::size_t op_index = 0;
        std::size_t current_task_index = 0;

        for (std::size_t row_index = 0; row_index < result.height; row_index++)
            for (std::size_t col_index = 0; col_index < result.width; col_index++, op_index++) {
                if (op_index == op_per_task) {
                    op_index = -1;
                    if (remainder > 0) {
                        tasks[current_task_index++].request_element(row_index, col_index);
                        remainder--;

                        continue;
                    }
                    current_task_index++;
                }

                tasks[current_task_index].request_element(row_index, col_index);
            }

        return tasks;
}

std::vector<Task> multiply_matrix_kth_tasks(const Matrix& A, const Matrix& B, Matrix& result) {
        std::vector<Task> tasks(TASK_COUNT, Task(A, B, result));

        std::size_t op_index = 0;
        for (std::size_t row_index = 0; row_index < result.height; row_index++)
            for (std::size_t col_index = 0; col_index < result.width; col_index++, op_index++)
                tasks[op_index % TASK_COUNT].request_element(row_index, col_index);

        return tasks;
}

std::vector<Task> multiply_matrix_col_tasks(const Matrix& A, const Matrix& B, Matrix& result) {
        std::vector<Task> tasks(TASK_COUNT, Task(A, B, result));

        std::size_t elements_computed = A.height * B.width;
        std::size_t op_per_task = elements_computed / TASK_COUNT;
        int remainder = elements_computed % TASK_COUNT;

        std::size_t op_index = 0;
        std::size_t current_task_index = 0;

        for (std::size_t col_index = 0; col_index < result.width; col_index++)
            for (std::size_t row_index = 0; row_index < result.height; row_index++, op_index++) {
                if (op_index == op_per_task) {
                    op_index = -1;
                    if (remainder > 0) {
                        tasks[current_task_index++].request_element(row_index, col_index);
                        remainder--;

                        continue;
                    }
                    current_task_index++;
                }

                tasks[current_task_index].request_element(row_index, col_index);
            }

        return tasks;
}

typedef std::function<std::vector<Task>(const Matrix&, const Matrix&, Matrix&)> TaskGenerator;
typedef std::function<Matrix(const Matrix&, const Matrix&, TaskGenerator)> MatrixMultiply;

Matrix multiply_matrix_seq(const Matrix& A, const Matrix& B, TaskGenerator task_generator) {
    Matrix result(A.width, B.height, 0);

    auto tasks = task_generator(A, B, result);
    for (auto task : tasks)
        task.execute();

    return result;
}

Matrix multiply_matrix_thread(const Matrix& A, const Matrix& B, TaskGenerator task_generator) {
    Matrix result(A.width, B.height, 0);
    std::vector<std::thread> threads;
    threads.reserve(TASK_COUNT);

    auto tasks = task_generator(A, B, result);
    for (auto &task : tasks)
        threads.emplace_back(&Task::execute, task);

    for (auto &thread : threads)
        thread.join();

    return result;
}

double multiply_matrix_pool(const Matrix& A, const Matrix& B, TaskGenerator task_generator) {
    boost::asio::thread_pool pool(THREAD_POOL_SIZE);
    const auto t_start = std::chrono::high_resolution_clock::now();

    Matrix result(A.width, B.height, 0);
    auto tasks = task_generator(A, B, result);

    for (auto &task : tasks)
        boost::asio::post(pool, std::bind(&Task::execute, task));

    pool.join();

    const auto t_end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(t_end - t_start).count();
}

double time_multiply_ms(MatrixMultiply multiply, const Matrix& A, const Matrix& B, TaskGenerator task_generator) {
    const auto t_start = std::chrono::high_resolution_clock::now();
    Matrix result = multiply(A, B, task_generator);
    const auto t_end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(t_end - t_start).count();
}

int main() {
    Matrix A = Matrix(500, 500, 5);
    Matrix B = Matrix(500, 500, 1);

    std::cout << "+++ MATRIX MULTIPLICATION +++" << std::endl << std::endl;
    std::cout << "MATRIX A: " << A.width << "x" << A.height << std::endl;
    std::cout << "MATRIX B: " << B.width << "x" << B.height << std::endl;
    std::cout << std::endl;
    std::cout << "TASK COUNT: " << TASK_COUNT << std::endl;
    std::cout << "THREAD POOL SIZE: " << THREAD_POOL_SIZE << std::endl;
    std::cout << std::endl;

    std::cout << "=== ROW STRATEGY ===" << std::endl << std::endl;
    std::cout << "SEQUENTIAL: \t\t" << time_multiply_ms(multiply_matrix_seq, A, B, multiply_matrix_row_tasks)
        << "ms" << std::endl;
    std::cout << "THREAD: \t\t" << time_multiply_ms(multiply_matrix_thread, A, B, multiply_matrix_row_tasks)
        << "ms" << std::endl;
    std::cout << "POOL: \t\t\t" << multiply_matrix_pool(A, B, multiply_matrix_row_tasks)
        << "ms" << std::endl;
    std::cout << std::endl;

    std::cout << "=== ROW STRATEGY ===" << std::endl << std::endl;
    std::cout << "SEQUENTIAL: \t\t" << time_multiply_ms(multiply_matrix_seq, A, B, multiply_matrix_row_tasks)
        << "ms" << std::endl;
    std::cout << "THREAD: \t\t" << time_multiply_ms(multiply_matrix_thread, A, B, multiply_matrix_row_tasks)
        << "ms" << std::endl;
    std::cout << "POOL: \t\t\t" << multiply_matrix_pool(A, B, multiply_matrix_row_tasks)
        << "ms" << std::endl;
    std::cout << std::endl;

    std::cout << "=== COLUMN STRATEGY ===" << std::endl << std::endl;
    std::cout << "SEQUENTIAL: \t\t" << time_multiply_ms(multiply_matrix_seq, A, B, multiply_matrix_col_tasks)
        << "ms" << std::endl;
    std::cout << "THREAD: \t\t" << time_multiply_ms(multiply_matrix_thread, A, B, multiply_matrix_col_tasks)
        << "ms" << std::endl;
    std::cout << "POOL: \t\t\t" << multiply_matrix_pool(A, B, multiply_matrix_col_tasks) 
        << "ms" << std::endl;
    std::cout << std::endl;

    std::cout << "=== K-TH STRATEGY ===" << std::endl << std::endl;
    std::cout << "SEQUENTIAL: \t\t" << time_multiply_ms(multiply_matrix_seq, A, B, multiply_matrix_kth_tasks)
        << "ms" << std::endl;
    std::cout << "THREAD: \t\t" << time_multiply_ms(multiply_matrix_thread, A, B, multiply_matrix_kth_tasks)
        << "ms" << std::endl;
    std::cout << "POOL: \t\t\t" << multiply_matrix_pool(A, B, multiply_matrix_kth_tasks)
        << "ms" << std::endl;
    std::cout << std::endl;


    return 0;
}
