#include <vector>
#include <limits>
#include <random>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <queue>

std::condition_variable condition;
std::mutex mutex;
std::queue<uint64_t> product_queue;
bool done = false;
uint64_t result;

void consumer() {
    for (;;) {
        std::unique_lock<std::mutex> lock(mutex);

        condition.wait(lock, []() { return !product_queue.empty() || done; });

        while (!product_queue.empty()) {
            result += product_queue.front();
            product_queue.pop();
        }

        if (done)
            break;
    }
}

void producer(const std::vector<uint32_t> a_vec,
              const std::vector<uint32_t> b_vec) {
    std::size_t current_index = 0;
    for (;;) {
        uint64_t product = a_vec[current_index] * b_vec[current_index];
        current_index++;

        std::lock_guard<std::mutex> lock(mutex);

        product_queue.push(product);
        condition.notify_one();

        if (current_index == a_vec.size()) {
            done = true;
            condition.notify_one();

            break;
        }
    }
}

uint32_t random_int(uint32_t lowerBound, uint32_t upperBound) {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(lowerBound, upperBound);

    return dist(rng);
}

std::vector<uint32_t> gen_random_uint_vec(std::size_t size) {
    std::vector<uint32_t> vec(size);
    std::generate(vec.begin(), vec.end(), []() {
        return random_int(0, std::numeric_limits<uint32_t>::max()); 
    });

    return vec;
}

int main() {
//    std::thread producer_thread(producer, gen_random_uint_vec(10000), gen_random_uint_vec(10000));
    std::thread producer_thread(producer, std::vector<uint32_t>(500, 2), std::vector<uint32_t>(500, 2));
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();

    std::cout << result << std::endl;

    return 0;
}
