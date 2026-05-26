# UDS over LIN — ECU 诊断协议栈 | Diagnostic Stack

[![ISO 14229-1](https://img.shields.io/badge/ISO--14229--1:2020-blue)](doc/)
[![Language](https://img.shields.io/badge/C-C11-green)]()

> ⚠️ **AI Coding 声明 | AI-Generated Notice**：本项目由 AI 辅助生成，仅供参考学习 UDS 诊断协议 (ISO 14229-1) 和 LIN 总线通信。不适用于生产环境。This project is AI-assisted and intended for educational reference only — for learning UDS diagnostic protocol (ISO 14229-1) and LIN bus communication. Not for production use.

C11 实现的 ISO 14229-1 统一诊断服务，运行在 LIN 总线上的 ECU。
A C11 implementation of ISO 14229-1 Unified Diagnostic Services (UDS) for an ECU communicating over LIN.

专为学习而生的完整诊断协议栈：从物理层 UART 帧到应用层 26 个标准 UDS 服务，每一层的源码都在这里。
A complete diagnostic stack built for learning: from physical UART frames up through 26 standard UDS services, source for every layer lives here.

## 目录 | TOC

- [架构 | Architecture](#架构--architecture)
- [支持的服务 | Supported Services](#支持的服务--supported-services)
- [学习工具 | Learning Tools](#学习工具--learning-tools)
- [构建 | Build](#构建--build)
- [测试 | Test](#测试--test)
- [模糊测试 | Fuzz Testing](#模糊测试--fuzz-testing)
- [目录结构 | Directory Structure](#目录结构--directory-structure)
- [协议标准 | Protocol Documents](#协议标准--protocol-documents)
- [许可证 | License](#许可证--license)

## 架构 | Architecture

诊断请求与响应的完整数据流路径。左侧为请求（下行），右侧为响应（上行）。

The complete data flow for diagnostic requests and responses. Left side is request (downstream), right side is response (upstream).

```
  LIN 总线 | LIN Bus
    │
  HAL UART (uart_read / uart_write)
    │        实现：src/hal/hal_stubs.c（嵌入式占位）或 hal_uart_posix.c（桌面模拟）
    │        Implementation: src/hal/hal_stubs.c (embedded stubs) or hal_uart_posix.c (desktop sim)
    │
  LIN 传输层 | LIN Transport Layer (N_PCI)
    │        ISO 17987-2: SF / FF / CF / FC 帧拆分与重组
    │        ISO 17987-2: single-frame / first-frame / consecutive-frame / flow-control segmentation
    │        src/uds/uds_lin_transport.c
    │
  UDS 解析器 | UDS Parser
    │        提取 SID、子功能、参数；验证最小长度；检测抑制正响应位
    │        Extracts SID, sub-function, parameters; validates minimum length; detects suppress positive response bit
    │        src/uds/uds_core.c — uds_parse_request()
    │
  会话管理 | Session Management
    │        Default / Programming / Extended 会话状态机，P2 timing
    │        Default / Programming / Extended session state machine, P2 timing
    │        src/uds/uds_session.c
    │
  安全访问 | Security Access
    │        种子-密钥挑战与验证状态机（Level 1 / Level 2）
    │        Seed-key challenge/response state machine (Level 1 / Level 2)
    │        src/uds/uds_security.c
    │
  服务分发 | Service Dispatch
    │        SID → 处理器路由表, 27-byte 服务列表
    │        SID → handler routing table, 27-byte service list
    │        src/uds/uds_service.c
    │
  服务处理器 | Service Handlers (26 个标准 UDS 服务)
    │        按功能模块分组（见下方表格）
    │        Grouped by functional module (see table below)
    │
  UDS 序列化器 | UDS Serializer
    │        正响应 (SID+0x40) 或负响应 (0x7F + NRC) 格式化
    │        Positive response (SID+0x40) or negative response (0x7F + NRC) formatting
    │        src/uds/uds_core.c — uds_serialize_response()
    │
  LIN 传输层 | LIN Transport Layer (出方向, transmit)
    │        帧拆分
    │        Frame segmentation
    │
  HAL UART (uart_write)
    │
  LIN 总线 | LIN Bus
```

### 关键设计决策 | Key Design Decisions

- **零动态分配** — 无 `malloc`。所有缓冲区在初始化时预分配，适配嵌入式场景。
  **Zero dynamic allocation** — no `malloc`. All buffers are pre-allocated at init time, suitable for embedded use.
- **严格 C11** — 无 GNU 扩展，`-Wall -Werror -Wextra -Wpedantic`。
  **Strict C11** — no GNU extensions, compiled with `-Wall -Werror -Wextra -Wpedantic`.
- **HAL 抽象** — 传输层通过 `hal_uart.h` 接口与物理层解耦，可替换为真实 UART 驱动或桌面模拟。
  **HAL abstraction** — transport layer decoupled from physical layer via `hal_uart.h` interface, swappable with real UART drivers or desktop simulation.
- **POSIX 模式** — 可选编译为带真实 UART (`/dev/ttyS0`), 定时器 (`clock_gettime`), NVM (文件 I/O) 的 Linux 可执行文件。
  **POSIX mode** — optionally compile into a Linux executable with real UART (`/dev/ttyS0`), timer (`clock_gettime`), and NVM (file I/O).

## 支持的服务 | Supported Services

基于 ISO 14229-1:2020。26 个标准 UDS 服务按功能分类列出。

Based on ISO 14229-1:2020. 26 standard UDS services listed by functional group.

| 服务 | Service | SID | 模块 | Module | 状态 | Status |
|------|---------|-----|------|--------|-------|-------|
| **诊断与通信管理** | **Diagnostic & Communication Management** |
| DiagnosticSessionControl | 诊断会话控制 | 0x10 | uds_svc_diagcomm | 完整 | Complete |
| ECUReset | ECU 复位 | 0x11 | uds_svc_diagcomm | 完整 | Complete |
| SecurityAccess | 安全访问 | 0x27 | uds_security | 完整 | Complete |
| CommunicationControl | 通信控制 | 0x28 | uds_svc_diagcomm | 完整 | Complete |
| TesterPresent | 测试器在线 | 0x3E | uds_svc_diagcomm | 完整 | Complete |
| ControlDTCSetting | DTC 设置控制 | 0x85 | uds_svc_diagcomm | 完整 | Complete |
| ResponseOnEvent | 事件触发响应 | 0x86 | uds_svc_diagcomm | 完整 | Complete |
| LinkControl | 链路控制 | 0x87 | uds_svc_diagcomm | 完整 | Complete |
| **数据传输** | **Data Transmission** |
| ReadDataByIdentifier | 按标识符读数据 | 0x22 | uds_svc_data | 完整 | Complete |
| WriteDataByIdentifier | 按标识符写数据 | 0x2E | uds_data | 完整 | Complete |
| ReadMemoryByAddress | 按地址读内存 | 0x23 | uds_svc_upload | 完整 | Complete |
| ReadScalingDataByIdentifier | 按标识符读标定数据 | 0x24 | uds_svc_data | 完整 | Complete |
| WriteMemoryByAddress | 按地址写内存 | 0x3D | uds_svc_data | 完整 | Complete |
| **已存储数据传输** | **Stored Data Transmission** |
| ReadDataByPeriodicIdentifier | 周期性读数据 | 0x2A | uds_svc_stored | 完整 | Complete |
| DynamicallyDefineDataIdentifier | 动态定义数据标识符 | 0x2C | uds_svc_stored | 完整 | Complete |
| ClearDiagnosticInformation | 清除诊断信息 | 0x14 | uds_dtc | 完整 | Complete |
| ReadDTCInformation | 读 DTC 信息 | 0x19 | uds_dtc | 完整 | Complete |
| **输入输出控制** | **Input/Output Control** |
| InputOutputControlByIdentifier | 按标识符输入输出控制 | 0x2F | uds_svc_io | 完整 | Complete |
| **远程例程激活** | **Remote Routine Activation** |
| RoutineControl | 例程控制 | 0x31 | uds_svc_routine | 完整 | Complete |
| **上传 / 下载** | **Upload / Download** |
| RequestDownload | 请求下载 | 0x34 | uds_svc_upload | 完整 | Complete |
| RequestUpload | 请求上传 | 0x35 | uds_svc_upload | 完整 | Complete |
| TransferData | 数据传输 | 0x36 | uds_svc_upload | 完整 | Complete |
| RequestTransferExit | 请求传输退出 | 0x37 | uds_svc_upload | 完整 | Complete |
| RequestFileTransfer | 请求文件传输 | 0x38 | uds_svc_upload | 完整 | Complete |
| **认证与安全** | **Authentication & Security** |
| Authentication | 认证服务 | 0x29 | uds_svc_auth | 完整 | Complete |
| SecuredDataTransmission | 安全数据传输 | 0x84 | (stub) | 桩实现 | Stub |

## 学习工具 | Learning Tools

`tools/uds_learning_suite.html` — 双模式 UDS 学习套件，浏览器直接打开。零依赖，只需一个现代浏览器。

`tools/uds_learning_suite.html` — dual-mode UDS learning suite, open directly in your browser. Zero dependencies, just a modern browser.

| 模式 | Mode | 功能 | Features |
|------|------|------|----------|
| 📖 学习 | Learn | 10 个标签页：服务浏览器、NRC 参考、SID 地图、会话管理、消息构造器、知识测验、LIN 传输协议、DTC 状态字节、寻址与响应、LIN-UART 物理层 | 10 tabs: Service Browser, NRC Reference, SID Map, Session Management, Message Builder, Knowledge Quiz, LIN Transport Protocol, DTC Status Bytes, Addressing & Response, LIN-UART Physical Layer |
| 🔌 模拟 | Simulate | 完整的 ECU 模拟器：发送诊断请求、实时消息日志、13 个预设场景、暗色/亮色主题 | Full ECU simulator: send diagnostic requests, real-time message log, 13 preset scenarios, dark/light theme |

还有两个附加工具：

Two additional tools:

- `tools/uds_learning_tool.html` — 精简版 UDS 概念学习工具。Simplified UDS concept learning tool.
- `tools/uds_simulator.html` — 独立的 UDS 消息模拟器。Standalone UDS message simulator.

## 构建 | Build

### 桌面模拟 | Desktop Simulation （默认 | default）

```bash
# 配置与构建（Debug 模式，带 AddressSanitizer）
# Configure & build (Debug mode with AddressSanitizer)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUDS_ASAN=ON
cmake --build build
```

产物 | Artifacts：
- `build/libuds-core.a` — 可链接的诊断协议栈静态库 | linkable diagnostic stack static library
- `build/uds-sim` — PC 模拟器可执行文件（接收 LIN 帧、运行完整协议栈）| PC simulator executable (receives LIN frames, runs the full stack)

### POSIX 真实硬件模式 | POSIX Real Hardware Mode

```bash
# 构建带真实 POSIX HAL 的版本（UART: /dev/ttyS0, Timer: clock_gettime, NVM: file I/O）
# Build with real POSIX HAL (UART: /dev/ttyS0, Timer: clock_gettime, NVM: file I/O)
cmake -B build -DHAL_PLATFORM=posix
cmake --build build
```

### 编译器要求 | Compiler Requirements

- C11 编译器（GCC ≥ 5, Clang ≥ 3.8, 或 MSVC ≥ 2015）
- C11 compiler (GCC ≥ 5, Clang ≥ 3.8, or MSVC ≥ 2015)
- CMake ≥ 3.14

### 编译选项 | Build Options

| 选项 | Option | 默认值 | Default | 说明 | Description |
|------|--------|--------|---------|------|------|
| `UDS_ASAN` | | OFF | 启用 AddressSanitizer | Enable AddressSanitizer |
| `UDS_UBSAN` | | OFF | 启用 UndefinedBehaviorSanitizer | Enable UndefinedBehaviorSanitizer |
| `HAL_PLATFORM=posix` | | — | 使用真实 POSIX HAL 替代 stubs | Use real POSIX HAL instead of stubs |
| `ENABLE_FUZZ` | | OFF | 编译模糊测试目标 | Build fuzz test targets |

## 测试 | Test

```bash
# 运行全部测试
# Run all tests
ctest --test-dir build --output-on-failure
```

## 模糊测试 | Fuzz Testing

可选模糊测试框架，用于发现解析器和传输层的边界缺陷。

Optional fuzz harnesses for discovering edge-case defects in the parser and transport layer.

```bash
# 构建模糊测试目标（需要 clang/libFuzzer）
# Build fuzz targets (requires clang/libFuzzer)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_FUZZ=ON \
  -DCMAKE_C_COMPILER=clang
cmake --build build

# 运行 UDS 解析器模糊测试
# Run UDS parser fuzz test
./build/fuzz-uds-parser -max_len=4096 -runs=1000000

# 运行 LIN 传输层模糊测试
# Run LIN transport layer fuzz test
./build/fuzz-lin-transport -max_len=4096 -runs=1000000
```

### 测试套件一览 | Test Suite Overview

| 测试可执行文件 | Test Executable | 覆盖范围 | Coverage |
|------|------|------|------|
| `uds-core-test` | 解析器/序列化器 | Parser / Serializer |
| `uds-session-test` | 会话状态机 | Session State Machine |
| `uds-lin-transport-test` | LIN 传输层 (N_PCI) | LIN Transport Layer (N_PCI) |
| `uds-dtc-test` | DTC 状态机 | DTC State Machine |
| `uds-security-test` | 安全访问种子-密钥 | Security Access Seed-Key |
| `uds-did-test` | DID 注册与访问控制 | DID Registry & Access Control |
| `uds-diagcomm-test` | 诊断与通信管理服务 | Diagnostic & Communication Services |
| `uds-data-service-test` | 数据传输服务 | Data Transmission Services |
| `uds-io-test` | I/O 控制服务 | I/O Control Services |
| `uds-upload-test` | 上传/下载服务 | Upload / Download Services |
| `uds-stored-test` | 已存储数据服务 | Stored Data Services |
| `uds-routine-test` | 例程控制服务 | Routine Control Services |
| `uds-auth-test` | 认证服务 | Authentication Services |
| `uds-lin-sim-test` | LIN 主站模拟器 | LIN Master Simulator |
| `uds-runner-test` | 运行器基础设施 | Runner Infrastructure |
| `uds-integration-test` | 端到端集成测试 | End-to-End Integration |
| `uds-service-test` | 服务分发路由 | Service Dispatch Routing |

## 目录结构 | Directory Structure

```
.
├── CMakeLists.txt                构建系统 | Build system (CMake 3.14+)
├── inc/
│   ├── hal/                      硬件抽象层公开接口 | HAL public interface
│   │   ├── hal_uart.h            UART 收发 | UART receive/transmit
│   │   ├── hal_timer.h           定时器 | Timer
│   │   ├── hal_nvm.h             非易失性存储 | Non-volatile memory
│   │   ├── hal_cfg.h             HAL 配置常量 | HAL config constants
│   │   ├── hal_common.h          通用 HAL 类型与状态码 | Common HAL types & status codes
│   │   └── hal_stubs.h           桩函数声明 | Stub function declarations
│   └── uds/                      UDS 协议栈公开接口 | UDS stack public interface
│       ├── uds_core.h            核心类型 / 解析 / 序列化 / SID 宏 | Core types / parse / serialize / SID macros
│       ├── uds_session.h         会话管理 | Session management
│       ├── uds_security.h        安全访问 | Security access
│       ├── uds_lin_transport.h   LIN 传输层 | LIN transport layer
│       ├── uds_data.h            DID 注册表与数据存储 | DID registry & data storage
│       ├── uds_dtc.h             DTC 状态机与状态字节掩码 | DTC state machine & status byte masks
│       ├── uds_service.h         27-byte 服务支持列表 | 27-byte service support list
│       ├── uds_svc_diagcomm.h    诊断与通信管理服务 | Diagnostic & comm management services
│       ├── uds_svc_data.h        数据传输服务 | Data transmission services
│       ├── uds_svc_stored.h      已存储数据传输服务 | Stored data transmission services
│       ├── uds_svc_routine.h     例程控制服务 | Routine control services
│       ├── uds_svc_io.h          I/O 控制服务 | I/O control services
│       ├── uds_svc_upload.h      上传/下载服务 | Upload/download services
│       └── uds_svc_auth.h        认证服务 | Authentication services
├── src/
│   ├── hal/
│   │   └── hal_stubs.c           嵌入式桩函数（空实现）| Embedded stubs (empty implementation)
│   └── uds/                      协议栈全量源码 | Complete stack source
│       ├── uds_core.c            解析器 / 序列化器 / 工具函数
│       ├── uds_session.c         会话状态机 + P2 timing
│       ├── uds_security.c        种子-密钥挑战/响应状态机
│       ├── uds_lin_transport.c   N_PCI 帧编码/解码
│       ├── uds_data.c            DID 注册表 / 读 / 写
│       ├── uds_dtc.c             DTC 设置 / 清除 / 快照
│       ├── uds_service.c         SID 路由表 + 服务支持列表
│       ├── uds_svc_diagcomm.c    8 个诊断与通信管理服务
│       ├── uds_svc_data.c        5 个数据传输服务
│       ├── uds_svc_stored.c      2 个 + DTC 服务
│       ├── uds_svc_routine.c     例程控制（启动/停止/查询结果）
│       ├── uds_svc_io.c          I/O 控制（短/长参数返回）
│       ├── uds_svc_upload.c      5 个上传/下载服务
│       └── uds_svc_auth.c        认证 + PKI 校验
├── sim/
│   ├── main_sim.c                PC 模拟器主循环 | PC simulator main loop
│   └── sim_cfg.h                 模拟器调优常量 | Simulator tuning constants
├── test/
│   ├── lin_sim/                  LIN 主站模拟工具 | LIN master simulation helpers
│   ├── mock/                     模拟 HAL 实现（桌面测试用）| Mock HAL (for desktop testing)
│   ├── test_core.c               核心解析/序列化测试
│   ├── test_session.c            会话状态机测试
│   ├── test_lin_transport.c      LIN 传输层测试
│   ├── test_dtc.c                DTC 状态机测试
│   ├── test_security.c           安全访问测试
│   ├── test_did.c                DID 注册表测试
│   ├── test_svc_diagcomm.c       诊断通信服务测试
│   ├── test_svc_data.c           数据传输服务测试
│   ├── test_svc_io.c             I/O 控制服务测试
│   ├── test_svc_upload.c         上传/下载服务测试
│   ├── test_svc_stored.c         已存储数据服务测试
│   ├── test_svc_routine.c        例程控制服务测试
│   ├── test_svc_auth.c           认证服务测试
│   ├── test_svc_service.c        服务分发路由测试
│   ├── test_lin_sim.c            LIN 主站模拟器测试
│   ├── test_uds_runner.c         测试运行器基础设施
│   ├── test_uds_integration.c    端到端集成测试
│   └── fuzz/                     模糊测试目标
├── tools/
│   ├── uds_learning_suite.html   主学习套件（学习 + 模拟双模式）
│   ├── uds_learning_tool.html    精简版学习工具
│   └── uds_simulator.html        独立消息模拟器
├── doc/
│   └── ISO 14229-*               完整协议标准（PDF + TXT 文本提取）
│                                  Complete protocol standards (PDF + TXT extraction)
├── local_unity/                  Unity 测试框架 (v2.6.1, vendored)
└── README.md                     本文件 | This file
```

## 协议标准 | Protocol Documents

实现所依据的权威标准文档。

Authoritative standards the implementation is based on.

| 标准 | 年份 | 内容 (中文) | Content (English) |
|------|------|------------|-------------------|
| ISO 14229-1:2020 | 2020 | UDS 应用层 — 所有 26 个服务定义 | UDS Application Layer — all 26 services |
| ISO 14229-2:2013 | 2013 | UDS 会话层 — P2/P2\*/S3 时序 | UDS Session Layer — P2/P2\*/S3 timing |
| ISO 14229-3:2012 | 2012 | UDS on CAN | UDS on CAN |
| ISO 14229-4:2012 | 2012 | UDS on FlexRay | UDS on FlexRay |
| ISO 14229-5:2013 | 2013 | UDS on IP (DoIP) | UDS on IP (DoIP) |
| ISO 14229-6:2013 | 2013 | UDS on K-Line | UDS on K-Line |
| **ISO 14229-7:2015** | **2015** | **UDS on LIN** | **UDS on LIN** |
| ISO 14229-8:2020 | 2020 | UDS on Clock Extension | UDS on Clock Extension |
| ISO 17987-2 | — | LIN 传输层 (N_PCI: SF/FF/CF/FC) | LIN Transport Layer (N_PCI) |
| ISO 17987-3 | — | LIN 数据链路层 | LIN Data Link Layer |

> 注意：TXT 文件为 PDF 文本提取产物，表格、图表和部分数值可能不完整。精确协议验证请查阅原始 PDF。
> Note: TXT files are PDF text extractions; tables, figures, and some numeric values may be incomplete. For precise protocol verification, consult original PDF documents.

## 许可证 | License

MIT License

Copyright (c) 2025
