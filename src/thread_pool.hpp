/**
 * The ThreadPool class.
 * Keeps a set of threads constantly waiting to execute incoming jobs.
 */
#pragma once

#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include "thread_queue.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace dispatch
{

}

#endif