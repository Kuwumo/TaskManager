cmake_minimum_required(VERSION 3.21)

project(TaskManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(TaskManager
    WIN32
    src/main.cpp
    src/core/SystemMonitorFactory.cpp
    src/core/SamplerThread.h
    src/core/SamplerThread.cpp
    src/model/ProcessTableModel.h
    src/model/ProcessTableModel.cpp
    src/ui/HistoryChart.h
    src/ui/HistoryChart.cpp
    src/ui/ProcessPage.h
    src/ui/ProcessPage.cpp
    src/ui/PerformancePage.h
    src/ui/PerformancePage.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)

target_include_directories(TaskManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(TaskManager PRIVATE Qt6::Widgets)

target_compile_definitions(TaskManager PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(TaskManager PRIVATE /utf-8)
endif()

if(WIN32)
    target_sources(TaskManager PRIVATE
        src/platform/windows/WindowsNetworkTracker.h
        src/platform/windows/WindowsNetworkTracker.cpp
        src/platform/windows/WindowsSystemMonitor.h
        src/platform/windows/WindowsSystemMonitor.cpp
    )
    target_link_libraries(TaskManager PRIVATE Psapi Pdh Tdh Advapi32)
endif()
