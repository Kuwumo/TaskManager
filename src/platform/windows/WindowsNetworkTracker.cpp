#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>

#include "platform/windows/WindowsNetworkTracker.h"

#include <cstring>
#include <vector>

void WINAPI networkEventCallback(PEVENT_RECORD record);

namespace {

// Microsoft-Windows-Kernel-Network
const GUID kNetworkProvider = {
    0x7DD42A49, 0x5329, 0x4832, {0x8D, 0xFD, 0x43, 0xD9, 0x79, 0x15, 0x3A, 0x88}};

std::wstring nameAt(const std::vector<BYTE>& buffer, ULONG offset)
{
    if (offset == 0 || offset >= buffer.size()) {
        return {};
    }
    return reinterpret_cast<const wchar_t*>(buffer.data() + offset);
}

bool containsToken(const std::wstring& text, const wchar_t* token)
{
    return text.find(token) != std::wstring::npos;
}

std::wstring lowerCopy(std::wstring text)
{
    for (wchar_t& ch : text) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return text;
}

bool readU64Property(PEVENT_RECORD record, const wchar_t* name, std::uint64_t* value)
{
    PROPERTY_DATA_DESCRIPTOR descriptor{};
    descriptor.PropertyName = reinterpret_cast<ULONGLONG>(name);
    descriptor.ArrayIndex = ULONG_MAX;

    ULONG size = 0;
    if (TdhGetPropertySize(record, 0, nullptr, 1, &descriptor, &size) != ERROR_SUCCESS) {
        return false;
    }
    if (size == 0 || size > sizeof(std::uint64_t)) {
        return false;
    }

    BYTE raw[sizeof(std::uint64_t)] = {};
    if (TdhGetProperty(record, 0, nullptr, 1, &descriptor, size, raw) != ERROR_SUCCESS) {
        return false;
    }
    std::uint64_t decoded = 0;
    memcpy(&decoded, raw, size);
    *value = decoded;
    return true;
}

} // namespace

WindowsNetworkTracker::WindowsNetworkTracker()
    : lastTake_(std::chrono::steady_clock::now())
    , sessionName_(L"TaskManagerPerProcessNetwork")
{
    auto properties = std::vector<BYTE>(sizeof(EVENT_TRACE_PROPERTIES) + (sessionName_.size() + 1) * sizeof(wchar_t));
    auto* session = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(properties.data());
    session->Wnode.BufferSize = static_cast<ULONG>(properties.size());
    session->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    session->Wnode.ClientContext = 1;
    session->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    session->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    session->BufferSize = 64;
    session->MinimumBuffers = 8;
    session->MaximumBuffers = 32;
    wcscpy_s(reinterpret_cast<wchar_t*>(properties.data() + session->LoggerNameOffset),
             sessionName_.size() + 1,
             sessionName_.c_str());

    ControlTraceW(0, sessionName_.c_str(), session, EVENT_TRACE_CONTROL_STOP);

    properties.assign(sizeof(EVENT_TRACE_PROPERTIES) + (sessionName_.size() + 1) * sizeof(wchar_t), 0);
    session = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(properties.data());
    session->Wnode.BufferSize = static_cast<ULONG>(properties.size());
    session->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    session->Wnode.ClientContext = 1;
    session->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    session->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    session->BufferSize = 64;
    session->MinimumBuffers = 8;
    session->MaximumBuffers = 32;
    wcscpy_s(reinterpret_cast<wchar_t*>(properties.data() + session->LoggerNameOffset),
             sessionName_.size() + 1,
             sessionName_.c_str());

    TRACEHANDLE sessionHandle = 0;
    const ULONG started = StartTraceW(&sessionHandle, sessionName_.c_str(), session);
    session_ = sessionHandle;
    if (started != ERROR_SUCCESS) {
        startError_.store(started);
        return;
    }

    ENABLE_TRACE_PARAMETERS enable{};
    enable.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    const ULONG enabled = EnableTraceEx2(
        session_,
        &kNetworkProvider,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0xFFFFFFFFFFFFFFFFULL,
        0,
        0,
        &enable);
    if (enabled != ERROR_SUCCESS) {
        startError_.store(enabled);
        stop();
        return;
    }

    EVENT_TRACE_LOGFILEW logfile{};
    logfile.LoggerName = sessionName_.data();
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = networkEventCallback;
    logfile.Context = this;
    const TRACEHANDLE traceHandle = OpenTraceW(&logfile);
    trace_ = traceHandle;
    if (trace_ == INVALID_PROCESSTRACE_HANDLE) {
        startError_.store(GetLastError());
        stop();
        return;
    }

    available_.store(true);
    thread_ = std::thread([this] { traceLoop(); });
}

WindowsNetworkTracker::~WindowsNetworkTracker()
{
    stop();
}

bool WindowsNetworkTracker::available() const
{
    return available_.load();
}

unsigned long WindowsNetworkTracker::startError() const
{
    return startError_.load();
}

std::unordered_map<std::uint32_t, std::uint64_t> WindowsNetworkTracker::takeRates()
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - lastTake_).count();
    lastTake_ = now;

    std::unordered_map<std::uint32_t, std::uint64_t> rates;
    if (seconds >= 0.05) {
        for (const auto& entry : bytes_) {
            const std::uint64_t total = entry.second.send + entry.second.recv;
            rates.emplace(entry.first, static_cast<std::uint64_t>(total / seconds));
        }
    }
    bytes_.clear();
    return rates;
}

void WindowsNetworkTracker::stop()
{
    if (trace_ != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(static_cast<TRACEHANDLE>(trace_));
        trace_ = INVALID_PROCESSTRACE_HANDLE;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    if (session_ != 0) {
        std::vector<BYTE> properties(sizeof(EVENT_TRACE_PROPERTIES) + (sessionName_.size() + 1) * sizeof(wchar_t));
        auto* session = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(properties.data());
        session->Wnode.BufferSize = static_cast<ULONG>(properties.size());
        session->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        wcscpy_s(reinterpret_cast<wchar_t*>(properties.data() + session->LoggerNameOffset),
                 sessionName_.size() + 1,
                 sessionName_.c_str());
        ControlTraceW(static_cast<TRACEHANDLE>(session_), nullptr, session, EVENT_TRACE_CONTROL_STOP);
        session_ = 0;
    }
    available_.store(false);
}

void WindowsNetworkTracker::traceLoop()
{
    if (trace_ == INVALID_PROCESSTRACE_HANDLE) {
        return;
    }
    TRACEHANDLE traceHandle = static_cast<TRACEHANDLE>(trace_);
    const ULONG status = ProcessTrace(&traceHandle, 1, nullptr, nullptr);
    if (status != ERROR_SUCCESS && status != ERROR_CANCELLED) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_.store(false);
        if (startError_.load() == 0) {
            startError_.store(status);
        }
    }
}

void WINAPI networkEventCallback(PEVENT_RECORD record)
{
    if (record == nullptr || record->UserContext == nullptr) {
        return;
    }
    static_cast<WindowsNetworkTracker*>(record->UserContext)->onEvent(record);
}

void WindowsNetworkTracker::onEvent(void* recordPtr)
{
    auto* record = static_cast<PEVENT_RECORD>(recordPtr);
    ULONG size = 0;
    ULONG status = TdhGetEventInformation(record, 0, nullptr, nullptr, &size);
    if (status != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return;
    }

    std::vector<BYTE> buffer(size);
    auto* info = reinterpret_cast<PTRACE_EVENT_INFO>(buffer.data());
    status = TdhGetEventInformation(record, 0, nullptr, info, &size);
    if (status != ERROR_SUCCESS) {
        return;
    }

    const std::wstring label = lowerCopy(nameAt(buffer, info->TaskNameOffset) + L" "
                                          + nameAt(buffer, info->OpcodeNameOffset));
    const bool send = containsToken(label, L"send");
    const bool recv = containsToken(label, L"recv") || containsToken(label, L"receive");
    if (!send && !recv) {
        return;
    }

    std::uint32_t pid = record->EventHeader.ProcessId;
    bool haveSize = false;
    std::uint64_t byteCount = 0;

    const ULONG propertyCount = info->TopLevelPropertyCount;
    for (ULONG index = 0; index < propertyCount; ++index) {
        const auto& property = info->EventPropertyInfoArray[index];
        if ((property.Flags & PropertyStruct) != 0) {
            continue;
        }
        const wchar_t* propertyName = reinterpret_cast<const wchar_t*>(buffer.data() + property.NameOffset);
        std::uint64_t value = 0;
        if (!readU64Property(record, propertyName, &value)) {
            continue;
        }

        const std::wstring lowered = lowerCopy(propertyName);
        if (lowered == L"pid" || lowered == L"processid") {
            if (value != 0 && value <= 0xFFFFFFFFULL) {
                pid = static_cast<std::uint32_t>(value);
            }
        } else if (lowered == L"size" || lowered == L"packetsize" || lowered == L"datasize" || lowered == L"bytes") {
            byteCount = value;
            haveSize = true;
        }
    }

    if (!haveSize || byteCount == 0 || pid == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto& slot = bytes_[pid];
    if (send) {
        slot.send += byteCount;
    }
    if (recv) {
        slot.recv += byteCount;
    }
}
