#pragma once

//includes
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <cwctype>

#include <windows.h>

#include <d3d9.h>
#include <d3dx9.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "injector/injector.hpp"
#include "memory/memory.hpp"
#include "vars/vars.hpp"
#include "utils/utils.hpp"
#include "gui/Menu.hpp"

using namespace std::chrono_literals;