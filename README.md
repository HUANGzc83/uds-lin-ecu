# UDS over LIN — ECU 诊断协议栈

[🌐 English](README_EN.md)

[![ISO 14229-1](https://img.shields.io/badge/ISO--14229--1:2020-blue)](doc/)
[![Language](https://img.shields.io/badge/C-C11-green)]()

> ⚠️ **AI Coding 声明**：本项目由 AI 辅助生成，仅供参考学习 UDS 诊断协议 (ISO 14229-1) 和 LIN 总线通信。不适用于生产环境。

C11 实现的 ISO 14229-1 统一诊断服务，运行在 LIN 总线上的 ECU。

专为学习而生的完整诊断协议栈：从物理层 UART 帧到应用层 26 个标准 UDS 服务，每一层的源码都在这里。

## 目录

- [架构](#架构)
- [支持的服务](#支持的服务)
- [学习工具](#学习工具)
- [构建](#构建)
- [测试](#测试)
- [模糊测试](#模糊测试)
- [目录结构](#目录结构)
- [协议标准](#协议标准)
- [许可证](#许可证)

## 架构

诊断请求与响应的完整数据流路径。左侧为请求（下行），右侧为响应（上行）。

```
  LIN 总线
    │
  HAL UART (uart_read / uart_write)
    │        实现：src/hal/hal_stubs.c（嵌入式占位）或 hal_uart_posix.c（桌面模拟）
    │
  LIN 传输层 (N_PCI)
    │        ISO 17987-2: SF / FF / CF / FC 帧拆分与重组
    │        src/uds/uds_lin_transport.c
    │
  UDS 解析器
    │        提取 SID、子功能、参数；验证最小长度；检测抑制正响应位
    │        src/uds/uds_core.c — uds_parse_request()
    │
  会话管理
    │        Default / Programming / Extended 会话状态机，P2 timing
    │        src/uds/uds_session.c
    │
  安全访问
    │        种子-密钥挑战与验证状态机（Level 1 / Level 2）
    │        src/uds/uds_security.c
    │
  服务分发
    │        SID → 处理器路由表, 27-byte 服务列表
    │        src/uds/uds_service.c
    │
  服务处理器（26 个标准 UDS 服务）
    │        按功能模块分组（见下方表格）
    │
  UDS 序列化器
    │        正响应 (SID+0x40) 或负响应 (0x7F + NRC) 格式化
    │        src/uds/uds_core.c — uds_serialize_response()
    │
  LIN 传输层（出方向）
    │        帧拆分
    │
  HAL UART (uart_write)
    │
  LIN 总线
```

### 关键设计决策

- **零动态分配** — 无 `malloc`。所有缓冲区在初始化时预分配，适配嵌入式场景。
- **严格 C11** — 无 GNU 扩展，`-Wall -Werror -Wextra -Wpedantic`。
- **HAL 抽象** — 传输层通过 `hal_uart.h` 接口与物理层解耦，可替换为真实 UART 驱动或桌面模拟。
- **POSIX 模式** — 可选编译为带真实 UART (`/dev/ttyS0`), 定时器 (`clock_gettime`), NVM (文件 I/O) 的 Linux 可执行文件。

## 支持的服务

基于 ISO 14229-1:2020。26 个标准 UDS 服务按功能分类列出。

| 服务 | SID | 模块 | 状态 |
|------|-----|------|------|
| **诊断与通信管理** |
| DiagnosticSessionControl | 0x10 | uds_svc_diagcomm | 完整 |
| ECUReset | 0x11 | uds_svc_diagcomm | 完整 |
| SecurityAccess | 0x27 | uds_security | 完整 |
| CommunicationControl | 0x28 | uds_svc_diagcomm | 完整 |
| TesterPresent | 0x3E | uds_svc_diagcomm | 完整 |
| ControlDTCSetting | 0x85 | uds_svc_diagcomm | 完整 |
| ResponseOnEvent | 0x86 | uds_svc_diagcomm | 完整 |
| LinkControl | 0x87 | uds_svc_diagcomm | 完整 |
| **数据传输** |
| ReadDataByIdentifier | 0x22 | uds_svc_data | 完整 |
| WriteDataByIdentifier | 0x2E | uds_data | 完整 |
| ReadMemoryByAddress | 0x23 | uds_svc_upload | 完整 |
| ReadScalingDataByIdentifier | 0x24 | uds_svc_data | 完整 |
| WriteMemoryByAddress | 0x3D | uds_svc_data | 完整 |
| **已存储数据传输** |
| ReadDataByPeriodicIdentifier | 0x2A | uds_svc_stored | 完整 |
| DynamicallyDefineDataIdentifier | 0x2C | uds_svc_stored | 完整 |
| ClearDiagnosticInformation | 0x14 | uds_dtc | 完整 |
| ReadDTCInformation | 0x19 | uds_dtc | 完整 |
| **输入输出控制** |
| InputOutputControlByIdentifier | 0x2F | uds_svc_io | 完整 |
| **远程例程激活** |
| RoutineControl | 0x31 | uds_svc_routine | 完整 |
| **上传 / 下载** |
| RequestDownload | 0x34 | uds_svc_upload | 完整 |
| RequestUpload | 0x35 | uds_svc_upload | 完整 |
| TransferData | 0x36 | uds_svc_upload | 完整 |
| RequestTransferExit | 0x37 | uds_svc_upload | 完整 |
| RequestFileTransfer | 0x38 | uds_svc_upload | 完整 |
| **认证与安全** |
| Authentication | 0x29 | uds_svc_auth | 完整 |
| SecuredDataTransmission | 0x84 | (stub) | 桩实现 |

## 学习工具

`tools/uds_learning_suite.html` — 双模式 UDS 学习套件，浏览器直接打开。零依赖，只需一个现代浏览器。

| 模式 | 功能 |
|------|------|
| 📖 学习 | 10 个标签页：服务浏览器、NRC 参考、SID 地图、会话管理、消息构造器、知识测验、LIN 传输协议、DTC 状态字节、寻址与响应、LIN-UART 物理层 |
| 🔌 模拟 | 完整的 ECU 模拟器：发送诊断请求、实时消息日志、13 个预设场景、暗色/亮色主题 |

还有两个附加工具：

- `tools/uds_learning_tool.html` — 精简版 UDS 概念学习工具。
- `tools/uds_simulator.html` — 独立的 UDS 消息模拟器。

## 构建

### 桌面模拟（默认）

```bash
# 配置与构建（Debug 模式，带 AddressSanitizer）
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUDS_ASAN=ON
cmake --build build
```

产物：
- `build/libuds-core.a` — 可链接的诊断协议栈静态库
- `build/uds-sim` — PC 模拟器可执行文件（接收 LIN 帧、运行完整协议栈）

### POSIX 真实硬件模式

```bash
# 构建带真实 POSIX HAL 的版本（UART: /dev/ttyS0, Timer: clock_gettime, NVM: file I/O）
cmake -B build -DHAL_PLATFORM=posix
cmake --build build
```

### 编译器要求

- C11 编译器（GCC ≥ 5, Clang ≥ 3.8, 或 MSVC ≥ 2015）
- CMake ≥ 3.14

### 编译选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `UDS_ASAN` | OFF | 启用 AddressSanitizer |
| `UDS_UBSAN` | OFF | 启用 UndefinedBehaviorSanitizer |
| `HAL_PLATFORM=posix` | — | 使用真实 POSIX HAL 替代 stubs |
| `ENABLE_FUZZ` | OFF | 编译模糊测试目标 |

## 测试

```bash
# 运行全部测试
ctest --test-dir build --output-on-failure
```

## 模糊测试

可选模糊测试框架，用于发现解析器和传输层的边界缺陷。

```bash
# 构建模糊测试目标（需要 clang/libFuzzer）
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_FUZZ=ON \
  -DCMAKE_C_COMPILER=clang
cmake --build build

# 运行 UDS 解析器模糊测试
./build/fuzz-uds-parser -max_len=4096 -runs=1000000

# 运行 LIN 传输层模糊测试
./build/fuzz-lin-transport -max_len=4096 -runs=1000000
```

### 测试套件一览

| 测试可执行文件 | 覆盖范围 |
|------|------|
| `uds-core-test` | 解析器/序列化器 |
| `uds-session-test` | 会话状态机 |
| `uds-lin-transport-test` | LIN 传输层 (N_PCI) |
| `uds-dtc-test` | DTC 状态机 |
| `uds-security-test` | 安全访问种子-密钥 |
| `uds-did-test` | DID 注册与访问控制 |
| `uds-diagcomm-test` | 诊断与通信管理服务 |
| `uds-data-service-test` | 数据传输服务 |
| `uds-io-test` | I/O 控制服务 |
| `uds-upload-test` | 上传/下载服务 |
| `uds-stored-test` | 已存储数据服务 |
| `uds-routine-test` | 例程控制服务 |
| `uds-auth-test` | 认证服务 |
| `uds-lin-sim-test` | LIN 主站模拟器 |
| `uds-runner-test` | 运行器基础设施 |
| `uds-integration-test` | 端到端集成测试 |
| `uds-service-test` | 服务分发路由 |

## 目录结构

```
.
├── CMakeLists.txt                构建系统 (CMake 3.14+)
├── inc/
│   ├── hal/                      硬件抽象层公开接口
│   │   ├── hal_uart.h            UART 收发
│   │   ├── hal_timer.h           定时器
│   │   ├── hal_nvm.h             非易失性存储
│   │   ├── hal_cfg.h             HAL 配置常量
│   │   ├── hal_common.h          通用 HAL 类型与状态码
│   │   └── hal_stubs.h           桩函数声明
│   └── uds/                      UDS 协议栈公开接口
│       ├── uds_core.h            核心类型 / 解析 / 序列化 / SID 宏
│       ├── uds_session.h         会话管理
│       ├── uds_security.h        安全访问
│       ├── uds_lin_transport.h   LIN 传输层
│       ├── uds_data.h            DID 注册表与数据存储
│       ├── uds_dtc.h             DTC 状态机与状态字节掩码
│       ├── uds_service.h         27-byte 服务支持列表
│       ├── uds_svc_diagcomm.h    诊断与通信管理服务
│       ├── uds_svc_data.h        数据传输服务
│       ├── uds_svc_stored.h      已存储数据传输服务
│       ├── uds_svc_routine.h     例程控制服务
│       ├── uds_svc_io.h          I/O 控制服务
│       ├── uds_svc_upload.h      上传/下载服务
│       └── uds_svc_auth.h        认证服务
├── src/
│   ├── hal/
│   │   └── hal_stubs.c           嵌入式桩函数（空实现）
│   └── uds/                      协议栈全量源码
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
│   ├── main_sim.c                PC 模拟器主循环
│   └── sim_cfg.h                 模拟器调优常量
├── test/
│   ├── lin_sim/                  LIN 主站模拟工具
│   ├── mock/                     模拟 HAL 实现（桌面测试用）
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
├── local_unity/                  Unity 测试框架 (v2.6.1, vendored)
└── README.md                     本文件
```

## 协议标准

实现所依据的权威标准文档。

| 标准 | 年份 | 内容 |
|------|------|------|
| ISO 14229-1:2020 | 2020 | UDS 应用层 — 所有 26 个服务定义 |
| ISO 14229-2:2013 | 2013 | UDS 会话层 — P2/P2\*/S3 时序 |
| ISO 14229-3:2012 | 2012 | UDS on CAN |
| ISO 14229-4:2012 | 2012 | UDS on FlexRay |
| ISO 14229-5:2013 | 2013 | UDS on IP (DoIP) |
| ISO 14229-6:2013 | 2013 | UDS on K-Line |
| **ISO 14229-7:2015** | **2015** | **UDS on LIN** |
| ISO 14229-8:2020 | 2020 | UDS on Clock Extension |
| ISO 17987-2 | — | LIN 传输层 (N_PCI: SF/FF/CF/FC) |
| ISO 17987-3 | — | LIN 数据链路层 |

> 注意：TXT 文件为 PDF 文本提取产物，表格、图表和部分数值可能不完整。精确协议验证请查阅原始 PDF。

## 许可证

MIT License

Copyright (c) 2025
