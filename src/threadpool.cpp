/**
 * threadpool.cpp
 * Thread pool implementation for parallel processing.
 * Provides a global singleton pool used by all scanners.
 */

#include "../include/toolkit.h"
#include <iostream>

// ─────────────────────────────────────────────
//  ThreadPool implementation
// ─────────────────────────────────────────────
ThreadPool::ThreadPool(size_t threadCount) {
    if (threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 4;
    }
    workers_.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stopped_ = true;
    }
    condition_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this] {
                return stopped_ || !tasks_.empty();
            });
            if (stopped_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
            // Increment while still holding the lock so waitAll() never
            // observes an empty queue with the popped task not yet counted.
            activeTasks_++;
        }
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[ThreadPool] Task exception: " << e.what() << "\n";
        }
        activeTasks_--;
    }
}

void ThreadPool::waitAll() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (tasks_.empty() && activeTasks_ == 0) return;
        }
        std::this_thread::yield();
    }
}

size_t ThreadPool::pending() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

bool ThreadPool::busy() const {
    return activeTasks_ > 0 || pending() > 0;
}

// ─────────────────────────────────────────────
//  Global thread pool singleton
// ─────────────────────────────────────────────
ThreadPool& globalThreadPool() {
    static ThreadPool pool;
    return pool;
}