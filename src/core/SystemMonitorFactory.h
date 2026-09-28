#pragma once

#include <memory>

class ISystemMonitor;

std::unique_ptr<ISystemMonitor> createSystemMonitor();
