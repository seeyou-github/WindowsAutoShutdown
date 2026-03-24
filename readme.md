# WindowsAutoShutdown

一个基于 Win32 API + C++17 的 Windows 10 暗色风格关机工具。

## 功能

- 主界面显示“关键时间”大字号（`HH:MM:SS`，居中）。
- 固定 8 个累加按钮，2 行 x 4 列布局，按钮值可在 `ini` 配置。
- 开始关机、取消关机（同时清空关机时间）。
- 配置中心按钮位于主界面左上角。
- 关闭窗口行为可配置：
  - 勾选“关闭窗口时最小化到托盘”则隐藏到托盘。
  - 不勾选则关闭窗口直接退出软件。
- 配置中心包含：防休眠、文件夹监控参数。
- 软件禁止多开（单实例）。

## 目录结构

- `build/` 编译输出目录
- `src/` 源码
- `res/` 资源文件
- `build.bat` 编译脚本
- `readme.md` 项目说明

## 编译

在项目根目录执行：

```bat
build.bat
```

成功后输出：

`build\WindowsAutoShutdown.exe`

## 说明

- 本项目采用模块化设计，便于扩展：
  - `MainWindow.*`：主界面、托盘、配置中心弹窗、主题绘制
  - `ShutdownScheduler.*`：关机流程状态机
  - `FolderMonitor.*`：文件夹监控策略
  - `ReminderWindow.*`：提醒弹窗
  - `Config.*`：INI 配置读写
- 若需要更强监控能力，后续可扩展为：
  - 文件数量阈值触发
  - 指定文件后缀过滤
  - 监控时间窗口（例如仅夜间有效）

## INI 按钮配置

`WindowsAutoShutdown.ini` 的 `Timer` 段支持以下键（单位：分钟）：

- `AddBtn1Min`
- `AddBtn2Min`
- `AddBtn3Min`
- `AddBtn4Min`
- `AddBtn5Min`
- `AddBtn6Min`
- `AddBtn7Min`
- `AddBtn8Min`

默认值：`1, 5, 10, 30, 60, 120, 300, 600`。
