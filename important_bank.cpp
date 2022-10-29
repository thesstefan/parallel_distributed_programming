#include <memory>
#include <cstdint>
#include <deque>
#include <random>
#include <thread>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <iostream>
#include <unordered_map>
#include <queue>

static constexpr unsigned int MAX_ACCOUNT_AMOUNT = 10000;
static constexpr unsigned int MAX_TRANSFERRED_AMOUNT = 1000;
static constexpr unsigned int WORKER_COUNT = 10;
static constexpr unsigned int ACCOUNT_COUNT = 500;
static constexpr unsigned int TRANSFER_COUNT = 10000;
static constexpr unsigned int CHECKER_SLEEP_DURATION_NS = 100000;

int random_int(int lowerBound, int upperBound) {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(lowerBound, upperBound - 1);

    return dist(rng);
}

class Account {
    public:
        const std::uint32_t uid;

        unsigned int balance;
        const unsigned int initial_balance; 

        mutable std::mutex mutex;
        std::vector<std::uint32_t> transfers_logged;

        Account(std::uint32_t uid, unsigned int balance) 
            : uid(uid), balance(balance), initial_balance(balance) {}
};

class Transfer {
    public:
        const std::uint32_t uid;
        const unsigned int amount_transferred;
        bool executed = false;

        const std::uint32_t payee_uid;
        const std::uint32_t payer_uid;

        Transfer(std::uint32_t uid, 
                 unsigned int amount_transferred, 
                 std::shared_ptr<Account> payee, 
                 std::shared_ptr<Account> payer) 
            : uid(uid), amount_transferred(amount_transferred),
            payee_uid(payee->uid), payer_uid(payer->uid), payee(payee), payer(payer) {}

        void execute() {
            if (executed)
                throw std::runtime_error("Transaction with ID " + std::to_string(uid) + " already executed!");

            executed = true;

            if (payee->uid == payer->uid)
                return;

            std::scoped_lock lock(payee->mutex, payer->mutex);

            if (payer->balance < amount_transferred)
                return;

            payer->balance -= amount_transferred;
            payer->transfers_logged.push_back(uid);

            payee->balance += amount_transferred;
            payee->transfers_logged.push_back(uid);
        }

    private:
        const std::shared_ptr<Account> payee;
        const std::shared_ptr<Account> payer;

};

class TransferWorker {
    private: 
        std::vector<Transfer> transfers;

    public:
        TransferWorker(std::vector<Transfer>& transfers) 
            : transfers(transfers) {}

        void add_transfer(Transfer& transfer) {
            transfers.push_back(transfer);
        }

        void run() {
            for (auto &transfer : transfers)
                transfer.execute();
        }
};

class ConsistencyChecker {
    private:
        const std::unordered_map<std::uint32_t, std::shared_ptr<Account>>& accounts;
        const std::unordered_map<std::uint32_t, Transfer>& transfers;
        std::atomic<bool>& running;

    public:
        ConsistencyChecker(const std::unordered_map<std::uint32_t, std::shared_ptr<Account>>& accounts,
                           const std::unordered_map<std::uint32_t, Transfer>& transfers,
                           std::atomic<bool>& running)
            : accounts(accounts), transfers(transfers), running(running) {}

        unsigned int check_account(const Account& account) const {
            auto& transfers_logged = account.transfers_logged;
            unsigned int account_balance = account.balance;

            unsigned int expected_balance = account.initial_balance;
            for (const std::uint32_t transfer_uid : transfers_logged) {
                const Transfer& transfer = transfers.at(transfer_uid);

                if (transfer.payee_uid == account.uid)
                    expected_balance += transfers.at(transfer_uid).amount_transferred;
                else if (transfer.payer_uid == account.uid)
                    expected_balance -= transfers.at(transfer_uid).amount_transferred;
                else
                    throw std::runtime_error("Logged transfer is not related to account!");
            }

            if (expected_balance != account_balance)
                throw std::runtime_error("Account balance is different from expected balance!");

            return expected_balance;
        }

        void run() const {
            const auto wait_duration = std::chrono::nanoseconds(CHECKER_SLEEP_DURATION_NS);
            unsigned int bank_total_balance = 0;
            unsigned int expected_total_balance = 0;

            while (running) {
                for (const auto& [uid, account] : accounts)
                    account->mutex.lock();

                for (const auto& [uid, account] : accounts) {
                    unsigned int expected_balance = check_account(*account);

                    bank_total_balance += account->balance;
                    expected_total_balance += expected_balance;
                }

                for (const auto& [uid, account] : accounts)
                    account->mutex.unlock();

                if (bank_total_balance != expected_total_balance)
                    throw std::runtime_error("Bank total balance is differented form expected balance!");

                std::cout << "Consistency check succeeded!" << std::endl;

                std::this_thread::sleep_for(wait_duration);
            }
        }
};

class Bank {
    std::unordered_map<std::uint32_t, std::shared_ptr<Account>> accounts;
    std::unordered_map<std::uint32_t, Transfer> transfers;
    std::atomic<bool> run_checker;

    std::vector<TransferWorker> workers;
    ConsistencyChecker consistency_checker;

    void _create_accounts(unsigned int account_count,
                          unsigned int max_amount) {
        for (std::uint32_t uid = 0; uid < account_count; uid++)
            accounts[uid] = std::make_shared<Account>(uid, random_int(0, max_amount));
    }

    void _create_transfers(unsigned int transfer_count,
                           unsigned int max_transferred_amount) {
        for (std::uint32_t uid = 0; uid < transfer_count; uid++) {
            std::uint32_t payee_uid = random_int(0, accounts.size());
            std::uint32_t payer_uid = random_int(0, accounts.size());

            int amount = random_int(0, max_transferred_amount);

            transfers.insert(std::make_pair(uid, 
                        Transfer(uid, amount, accounts[payee_uid], accounts[payer_uid])));
        }
    }

    void _assign_transfers_to_workers() {
        workers.reserve(WORKER_COUNT);
        unsigned int transfers_per_worker = transfers.size() / WORKER_COUNT;

        for (unsigned int transfer_batch_index = 0; 
                transfer_batch_index < WORKER_COUNT; transfer_batch_index++) {
            std::uint32_t start_batch_uid = transfer_batch_index * transfers_per_worker;

            std::vector<Transfer> transfer_batch;
            transfer_batch.reserve(transfers_per_worker);
            for (std::uint32_t transfer_uid = start_batch_uid; 
                    transfer_uid < transfers.size(); transfer_uid++)
                transfer_batch.push_back(transfers.at(transfer_uid));

            workers.emplace_back(transfer_batch);
        }
    }

    public:
        Bank() : consistency_checker(accounts, transfers, run_checker) {}

        void prepare_bank() {
            _create_accounts(ACCOUNT_COUNT, MAX_ACCOUNT_AMOUNT);
            _create_transfers(TRANSFER_COUNT, MAX_TRANSFERRED_AMOUNT);
            _assign_transfers_to_workers();
        }

        void open_bank() {
            auto t_start = std::chrono::high_resolution_clock::now();

            std::deque<std::thread> worker_threads;
            for (auto &worker : workers)
                worker_threads.emplace_back(&TransferWorker::run, worker);

            run_checker = true;
            std::thread checker_thread(&ConsistencyChecker::run, consistency_checker);

            for (auto &thread : worker_threads)
                thread.join();

            const auto wait_duration = std::chrono::nanoseconds(CHECKER_SLEEP_DURATION_NS);
            std::this_thread::sleep_for(wait_duration);

            run_checker = false;
            checker_thread.join();

            auto t_end = std::chrono::high_resolution_clock::now();
            double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();

            std::cout << "Bank closed after " << elapsed_time_ms << "ms" << std::endl;
        }
};

int main() {
    Bank bank;

    bank.prepare_bank();
    bank.open_bank();
}
