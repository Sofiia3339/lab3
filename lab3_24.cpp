#include <iostream>
#include <vector>
#include <thread>
#include <semaphore>
#include <latch>
#include <syncstream>
#include <atomic>
#include <deque>
#include <chrono>

const int NT = 7;

const int cnt_a = 6;
const int cnt_b = 9;
const int cnt_c = 7;
const int cnt_d = 7;
const int cnt_e = 8;
const int cnt_f = 6;
const int cnt_g = 5;
const int cnt_h = 7;
const int cnt_i = 9;
const int cnt_j = 8;

const int TOTAL_TASKS = cnt_a + cnt_b + cnt_c + cnt_d + cnt_e +
cnt_f + cnt_g + cnt_h + cnt_i + cnt_j;

struct Task {
    char group;
    int id;
};

std::deque<Task> task_queue;
std::binary_semaphore queue_lock{ 1 };
std::counting_semaphore<1000> task_signal{ 0 };
std::latch completion_latch{ TOTAL_TASKS };

std::atomic<int> rem_a{ cnt_a }, rem_b{ cnt_b }, rem_c{ cnt_c },
rem_d{ cnt_d }, rem_e{ cnt_e }, rem_f{ cnt_f },
rem_g{ cnt_g }, rem_h{ cnt_h }, rem_i{ cnt_i }, rem_j{ cnt_j };

std::atomic<int> deps_for_i{ 2 };
std::atomic<int> deps_for_j{ 2 };

std::atomic<bool> stop_workers{ false };

void push_tasks(char group, int count) {
    queue_lock.acquire();
    for (int i = 1; i <= count; ++i) {
        task_queue.push_back({ group, i });
    }
    queue_lock.release();
    task_signal.release(count);
}

void f(char group, int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::osyncstream(std::cout) << "Action " << id << " from set " << group << " completed.\n";
}

void check_dependencies(char completed_group) {
    if (completed_group == 'a') {
        if (rem_a.fetch_sub(1) == 1) {
            push_tasks('c', cnt_c);
            push_tasks('d', cnt_d);
            push_tasks('e', cnt_e);
        }
    }
    else if (completed_group == 'b') {
        if (rem_b.fetch_sub(1) == 1) {
            push_tasks('f', cnt_f);
            push_tasks('g', cnt_g);
            push_tasks('h', cnt_h);
        }
    }
    else if (completed_group == 'd') {
        if (rem_d.fetch_sub(1) == 1) {
            if (deps_for_i.fetch_sub(1) == 1) {
                push_tasks('i', cnt_i);
            }
        }
    }
    else if (completed_group == 'f') {
        if (rem_f.fetch_sub(1) == 1) {
            if (deps_for_i.fetch_sub(1) == 1) {
                push_tasks('i', cnt_i);
            }
        }
    }
    else if (completed_group == 'e') {
        if (rem_e.fetch_sub(1) == 1) {
            if (deps_for_j.fetch_sub(1) == 1) {
                push_tasks('j', cnt_j);
            }
        }
    }
    else if (completed_group == 'g') {
        if (rem_g.fetch_sub(1) == 1) {
            if (deps_for_j.fetch_sub(1) == 1) {
                push_tasks('j', cnt_j);
            }
        }
    }
    else if (completed_group == 'c') rem_c.fetch_sub(1);
    else if (completed_group == 'h') rem_h.fetch_sub(1);
    else if (completed_group == 'i') rem_i.fetch_sub(1);
    else if (completed_group == 'j') rem_j.fetch_sub(1);
}

void worker_thread() {
    while (true) {
        task_signal.acquire();

        if (stop_workers) return;

        Task t;
        bool have_task = false;

        queue_lock.acquire();
        if (!task_queue.empty()) {
            t = task_queue.front();
            task_queue.pop_front();
            have_task = true;
        }
        queue_lock.release();

        if (have_task) {
            f(t.group, t.id);
            check_dependencies(t.group);
            completion_latch.count_down();
        }
    }
}

int main() {
    std::osyncstream(std::cout) << "Computation started.\n";

    std::vector<std::thread> threads;
    for (int i = 0; i < NT; ++i) {
        threads.emplace_back(worker_thread);
    }

    push_tasks('a', cnt_a);
    push_tasks('b', cnt_b);

    completion_latch.wait();

    std::osyncstream(std::cout) << "Computation finished.\n";

    stop_workers = true;
    task_signal.release(NT);
    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    return 0;
}

