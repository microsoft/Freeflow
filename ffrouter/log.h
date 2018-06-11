// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef LOG_H
#define LOG_H

#include <iostream>

#define LOG_LEVEL 10

#define LOG_INFO(x) (((LOG_LEVEL) <= 3) ? std::cout << "[INFO] "  << x << std::endl : std::cout << "")
#define LOG_DEBUG(x) (((LOG_LEVEL) <= 2) ? std::cout << "[DEBUG] " << x << std::endl : std::cout << "")
#define LOG_TRACE(x) (((LOG_LEVEL) <= 1) ? std::cout << "[TRACE] " << x << std::endl : std::cout << "")
#define LOG_ERROR(x) (std::cerr << "[ERROR] " << x << std::endl);

#endif /* LOG_H */
