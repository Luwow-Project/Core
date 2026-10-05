#include "ModuleLayers.h"

#include <exception>
#include <iostream>

namespace Luwow::Engine {

void SerialLayer::add(ILuauModule* module) {
    modules.push_back(module);
}

void SerialLayer::run() {
    for (ILuauModule* module : modules) {
        module->run();
    }
}

ParallelLayer::~ParallelLayer() {
    requestStop();
    join();
}

void ParallelLayer::add(ILuauModule* module) {
    std::lock_guard<std::mutex> lock(mutex);
    workers.push_back(Worker { module, std::thread() });
    if (started) startWorker(workers.back());
}

void ParallelLayer::start() {
    std::lock_guard<std::mutex> lock(mutex);
    if (started) return;
    started = true;
    for (Worker& worker : workers) startWorker(worker);
}

void ParallelLayer::startWorker(Worker& worker) {
    ILuauModule* module = worker.module;
    worker.thread = std::thread([module] {
        try { // An exception escaping a thread would terminate the process, so it's reported instead
            module->run();
        } catch (const std::exception& e) {
            std::cerr << "Parallel module " << module->getModuleName() << " failed: " << e.what() << "\n";
        }
    });
}

void ParallelLayer::requestStop() {
    std::lock_guard<std::mutex> lock(mutex);
    for (Worker& worker : workers) {
        if (worker.thread.joinable()) worker.module->stop();
    }
}

void ParallelLayer::join() {
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (Worker& worker : workers) {
            if (worker.thread.joinable()) threads.push_back(std::move(worker.thread));
        }
    }
    for (std::thread& thread : threads) thread.join();
}

} // namespace Luwow::Engine
