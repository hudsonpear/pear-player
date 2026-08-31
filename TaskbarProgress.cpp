#include "TaskbarProgress.h"

#include <QWidget>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>

#include <algorithm>

TaskbarProgress::TaskbarProgress(QWidget *window)
    : window_(window)
{
    // Harmless if Qt already initialized COM on this thread (S_FALSE) or
    // with a different concurrency model (RPC_E_CHANGED_MODE) -- either
    // way COM is up and CoCreateInstance below can proceed. Deliberately
    // never paired with CoUninitialize(): we didn't necessarily own the
    // initialization, and the process is tearing down anyway by the time
    // this object dies.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbarList_));
    if (taskbarList_) {
        taskbarList_->HrInit();
    }
}

TaskbarProgress::~TaskbarProgress()
{
    if (taskbarList_) {
        taskbarList_->Release();
    }
}

void TaskbarProgress::setProgress(double fraction)
{
    if (!taskbarList_) {
        return;
    }
    constexpr ULONGLONG kTotal = 1000;
    const auto completed = static_cast<ULONGLONG>(std::clamp(fraction, 0.0, 1.0) * kTotal);
    const auto hwnd = reinterpret_cast<HWND>(window_->winId());
    taskbarList_->SetProgressValue(hwnd, completed, kTotal);
}

void TaskbarProgress::setPlaying(bool playing)
{
    if (!taskbarList_) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(window_->winId());
    taskbarList_->SetProgressState(hwnd, playing ? TBPF_NORMAL : TBPF_PAUSED);
}

void TaskbarProgress::clear()
{
    if (!taskbarList_) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(window_->winId());
    taskbarList_->SetProgressState(hwnd, TBPF_NOPROGRESS);
}
