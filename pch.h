#pragma once

//includes
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <sstream>
#include <thread>
#include <cwctype>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

#include "injector/injector.hpp"
#include "memory/memory.hpp"
#include "vars/vars.hpp"
#include "utils/utils.hpp"
#include "gui/Menu.hpp"

using namespace std::chrono_literals;
