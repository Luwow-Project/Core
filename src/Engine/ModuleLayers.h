#pragma once

#include "ILuauModule.h"

#include <mutex>
#include <thread>
#include <vector>

namespace Luwow::Engine {

// Runs serial modules on the main thread, one after another
class SerialLayer {
public:
    void add(ILuauModule* module);

    // Runs each module's loop in the order they were added, returns once all have finished
    void run();

private:
    std::vector<ILuauModule*> modules;
};

// Runs each parallel module on its own thread
class ParallelLayer {
public:
    ~ParallelLayer();

    // Adds a module, its thread starts right away if the layer already started
    void add(ILuauModule* module);
    void start();

    // Asks every module to finish, then waits for their threads to end
    void requestStop();
    void join();

private:
    struct Worker {
        ILuauModule* module;
        std::thread thread;
    };

    void startWorker(Worker& worker);

    std::mutex mutex;
    std::vector<Worker> workers;
    bool started = false;
};

} // namespace Luwow::Engine
