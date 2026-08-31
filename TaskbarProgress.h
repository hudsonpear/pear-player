#pragma once

class QWidget;
struct ITaskbarList3;

/// Wraps the Windows taskbar icon's progress overlay (ITaskbarList3).
/// Playback position drives the bar fill; play/pause state drives its
/// color, since Windows renders TBPF_NORMAL green and TBPF_PAUSED amber.
class TaskbarProgress
{
public:
    explicit TaskbarProgress(QWidget *window);
    ~TaskbarProgress();

    TaskbarProgress(const TaskbarProgress &) = delete;
    TaskbarProgress &operator=(const TaskbarProgress &) = delete;

    /// fraction is clamped to 0.0-1.0.
    void setProgress(double fraction);

    /// true = green/advancing, false = amber/frozen.
    void setPlaying(bool playing);

    /// Removes the overlay entirely (no file loaded).
    void clear();

private:
    QWidget *window_;
    ITaskbarList3 *taskbarList_ = nullptr;
};
