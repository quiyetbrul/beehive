/*
Copyright 2020 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <beehive/mq.h>
#include <beehive/worker.h>
#include <beehive/pool.h>
#include <beehive/platform.h>
#include <iostream>
#include <sstream>

using namespace beehive;

Worker::Worker(Pool* parent, int id) : mParent(parent), mId(id) {
    name(nullptr);
    mWorkThread = std::thread([this] {
        this->WorkLoop();
    });
}

static std::mutex gDumpMutex;

void Worker::onBeforeMessage() {
    mStats.idle().stop();
    mStats.active().start();
    mStats.message();
}
void Worker::onAfterMessage() {
    mStats.active().stop();
    mStats.idle().start();
}

SignalingQueue::Handler::Result Worker::onNop() {
    return SignalingQueue::Handler::Result::CONTINUE;
}
SignalingQueue::Handler::Result Worker::onExit() {
    return SignalingQueue::Handler::Result::FINISH;
}
SignalingQueue::Handler::Result Worker::onTask() {
    auto task = mParent->task();
    if (task) {
        mStats.run();
        task->run();
    }
    return SignalingQueue::Handler::Result::CONTINUE;
}
SignalingQueue::Handler::Result Worker::onDump() {
    auto s = stats();
    std::unique_lock<std::mutex> lk(gDumpMutex);
    std::cerr << "Thread: " << name() << std::endl;
    std::cerr << "Number of tasks ran: " << s.runs << std::endl;
    std::cerr << "Number of messages processed: " << s.messages << std::endl;
    std::cerr << "Time active: " << s.active.count() << " milliseconds" << std::endl;
    std::cerr << "Time idle: " << s.idle.count() << " milliseconds" << std::endl;
    return SignalingQueue::Handler::Result::CONTINUE;
}


void Worker::WorkLoop() {
    mStats.idle().start();
    mMsgQueue.loop(this);
}

Worker::Stats Worker::stats() {
    return mStats.load();
}

const char* Worker::name() const {
    return mName.c_str();
}

void Worker::name(const char* n) {
    if (n && n[0]) {
        mName.assign(n);
    } else {
        std::stringstream ss;
        ss << "worker[" << mId << "]";
        mName = ss.str();
    }
}

int Worker::id() const {
    return mId;
}

std::thread::id Worker::tid() {
    return mWorkThread.get_id();
}

std::thread::native_handle_type Worker::nativeid() {
    return mWorkThread.native_handle();
}

void Worker::send(Message m) {
    mMsgQueue.send(m);
}

Worker::~Worker() {
    exit();
    mWorkThread.join();
}

Worker::View Worker::view() {
    return View(this);
}

void Worker::exit() {
    send(Message::Kind::EXIT);
}

void Worker::task() {
    send(Message::Kind::TASK);
}

void Worker::dump() {
    send(Message::Kind::DUMP);
}

std::vector<bool> Worker::affinity() {
    return Platform::affinity(nativeid());
}

void Worker::affinity(const std::vector<bool>& a) {
    Platform::affinity(nativeid(), a);
}

Worker::AtomicStats::AtomicStats() = default;

Worker::Stats Worker::AtomicStats::load() {
    Stats s;
    s.messages = mMessages.load();
    s.runs = mRuns.load();
    s.idle = mIdle.value();
    s.active = mActive.value();
    return s;
}

void Worker::AtomicStats::message() {
    mMessages.fetch_add(1);
}

void Worker::AtomicStats::run() {
    mRuns.fetch_add(1);
}

TimeCounter& Worker::AtomicStats::active() {
    return mActive;
}
TimeCounter& Worker::AtomicStats::idle() {
    return mIdle;
}

bool Worker::Stats::operator==(const Stats& rhs) const {
    return (messages == rhs.messages) &&
           (runs == rhs.runs) &&
           (idle == rhs.idle) &&
           (active == rhs.active);
}

bool Worker::Stats::operator!=(const Stats& rhs) const {
    return !(*this == rhs);
}
