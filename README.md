# UDS over LIN — ECU 诊断协议栈 (ISO 14229-1)

C11 实现的 ISO 14229-1 统一诊断服务 (UDS)，运行在 LIN 总线上的 ECU。

## 架构

```
  LIN 总线
    │
  HAL UART (uart_read / uart_write)
    │
  LIN 传输层 (N_PCI: SF / FF / CF / FC 帧拆分与重组)
    │
  UDS 解析器 (SID, 子功能, 参数提取)
    │
  会话管理 (Default / Programming / Extended 会话状态机)
    │
  服务分发 (SID 路由表)
    │
  服务处理器 (26 个标准 UDS 服务)
    │
  UDS 序列化器 (正响应 / 负响应 格式化)
    │
  LIN 传输层 (帧拆分)
    │
  HAL UART (uart_write)
    │
  LIN 总线
```

## 支持的服务

| 服务 | SID | 模块 |
|------|-----|------|
| DiagnosticSessionControl | 0x10 | uds_svc_diagcomm |
| ECUReset | 0x11 | uds_svc_diagcomm |
| SecurityAccess | 0x27 | uds_security |
| CommunicationControl | 0x28 | uds_svc_diagcomm |
| TesterPresent | 0x3E | uds_svc_diagcomm |
| ControlDTCSetting | 0x85 | uds_svc_diagcomm |
| ResponseOnEvent | 0x86 | uds_svc_diagcomm |
| LinkControl | 0x87 | uds_svc_diagcomm |
| ReadDataByIdentifier | 0x22 | uds_svc_data |
| WriteDataByIdentifier | 0x2E | uds_data |
| ReadMemoryByAddress | 0x23 | uds_svc_upload |
| ReadScalingDataByIdentifier | 0x24 | uds_svc_data |
| ReadDataByPeriodicIdentifier | 0x2A | uds_svc_stored |
| DynamicallyDefineDataIdentifier | 0x2C | uds_svc_stored |
| WriteMemoryByAddress | 0x3D | uds_svc_data |
| ReadDTCInformation | 0x19 | uds_dtc |
| ClearDiagnosticInformation | 0x14 | uds_dtc |
| InputOutputControlByIdentifier | 0x2F | uds_svc_io |
| RoutineControl | 0x31 | uds_svc_routine |
| RequestDownload | 0x34 | uds_svc_upload |
| RequestUpload | 0x35 | uds_svc_upload |
| TransferData | 0x36 | uds_svc_upload |
| RequestTransferExit | 0x37 | uds_svc_upload |
| RequestFileTransfer | 0x38 | uds_svc_upload |
| Authentication | 0x29 | uds_svc_auth |
| SecuredDataTransmission | 0x84 | (stub) |

## 学习工具

`tools/uds_learning_suite.html` — 双模式 UDS 学习套件，浏览器直接打开：

- **📖 学习模式** — 10 个标签页：服务浏览器、NRC 参考、SID 地图、会话管理、消息构造器、知识测验、LIN 传输协议、DTC 状态字节、寻址与响应、LIN-UART 物理层
- **🔌 模拟模式** — 完整的 ECU 模拟器：发送诊断请求、实时消息日志、13 个预设场景、暗色/亮色主题

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

产物：`build/libuds-core.a` + `build/uds-sim`

## 测试

```bash
ctest --test-dir build --output-on-failure
```

## 文档

`doc/` — ISO 14229 系列标准文档（1-8 部分，PDF + TXT）

## 目录结构

```
.
├── inc/uds/         公共 API 头文件
├── src/uds/         协议栈源码
├── src/hal/         硬件抽象层 (UART, Timer, NVM)
├── sim/             PC 模拟器入口
├── test/            单元测试 / 集成测试
├── tools/           学习套件 + 工具脚本
└── doc/             ISO 14229 标准文档
```

## 协议标准

| 标准 | 内容 |
|------|------|
| ISO 14229-1:2020 | UDS 应用层 |
| ISO 14229-7:2015 | UDS on LIN |
| ISO 17987-2 | LIN 传输层 & 网络层 (N_PCI) |
| ISO 17987-3 | LIN 数据链路层 |
