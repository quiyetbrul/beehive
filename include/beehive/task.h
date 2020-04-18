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

#pragma once

#include <future>

namespace beehive {
class Task {
    public:
        using Priority = uint8_t;
        static constexpr Priority MinPriority = 0;
        static constexpr Priority DefaultPriority = 127;
        static constexpr Priority MaxPriority = 255;

        using Callable = std::function<void()>;

        Task(Callable);

        std::shared_future<void>& future();

        void run();

    private:
        Callable mCallable;
        std::promise<void> mPromise;
        std::shared_future<void> mFuture;
};
}
