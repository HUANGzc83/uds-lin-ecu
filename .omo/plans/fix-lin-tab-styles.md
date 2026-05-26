# 修复 LIN 传输协议标签页样式

## TL;DR

> **Quick Summary**: 为 LIN 传输协议标签页添加缺失的 CSS 样式定义，修复排版混乱和暗色模式下文字看不清的问题。
>
> **Deliverables**:
> - `tools/uds_learning_suite.html` — 新增 ~50 行 CSS（`.lin-table`, `.lin-frame-table`, `.lin-info-box` 等）
>
> **Estimated Effort**: Quick
> **Parallel Execution**: NO — single CSS edit

---

## Work Objectives

### Core Objective
在 `uds_learning_suite.html` 的 `<style>` 区域中（已有 `/* ===== LIN Frame Annotations ===== */` 块之后）追加 LIN 标签页专用的 CSS 样式定义。

### 问题诊断
LIN 标签页 HTML 使用了以下 CSS 类，但对应的样式规则完全不存在：
- `.lin-section` — 段落容器间距
- `.lin-table` — 通用数据表格（边框、内边距、斑马纹、暗色适配）
- `.lin-frame-table` — 帧布局表格（字节颜色标记：NAD 蓝、PCI 黄、数据绿、FC 粉、未使用灰）
- `.lin-info-box` — 提示信息框（背景、左侧色条、暗色适配）
- `.lin-legend` / `.lin-legend-item` / `.lin-legend-dot` — 图例

### 修复内容

在 `/* ===== LIN Frame Annotations ===== */` 块末尾（现有 `.dark .lin-frame .lin-tag.uds` 之后）插入以下 CSS：

```css
/* ===== LIN Transport Protocol Tab ===== */
.lin-section { margin-bottom: 2rem; }
.lin-table { width: 100%; border-collapse: collapse; font-size: .82rem; margin: .5rem 0 1rem; }
.lin-table th, .lin-table td { padding: .5rem .7rem; text-align: left; border: 1px solid var(--border); }
.lin-table th { background: var(--card2); font-weight: 600; font-size: .78rem; color: var(--text); }
.lin-table td { color: var(--text); }
.lin-table tr:nth-child(even) td { background: rgba(128,128,128,.04); }
.lin-table code { background: var(--code-bg); padding: 1px 6px; border-radius: 3px; font-size: .82rem; color: var(--text); }

.lin-frame-table { width: 100%; border-collapse: collapse; font-size: .78rem; margin: .5rem 0 1rem; }
.lin-frame-table th, .lin-frame-table td { padding: .4rem .5rem; text-align: center; border: 1px solid var(--border); }
.lin-frame-table th { background: var(--card2); font-weight: 600; color: var(--text); }
.lin-frame-table .byte-nad { background: #dbeafe; color: #1e40af; font-weight: 600; }
.lin-frame-table .byte-pci { background: #fef3c7; color: #92400e; font-weight: 600; }
.lin-frame-table .byte-data { background: #d1fae5; color: #065f46; font-weight: 600; }
.lin-frame-table .byte-fc { background: #fce7f3; color: #9d174d; font-weight: 600; }
.lin-frame-table .byte-unused { background: var(--code-bg); color: var(--text3); }
.dark .lin-frame-table .byte-nad { background: #1e3a5f; color: #93c5fd; }
.dark .lin-frame-table .byte-pci { background: #422006; color: #fbbf24; }
.dark .lin-frame-table .byte-data { background: #064e3b; color: #6ee7b7; }
.dark .lin-frame-table .byte-fc { background: #4c0519; color: #f9a8d4; }

.lin-info-box { margin: .8rem 0; padding: .8rem 1rem; background: var(--code-bg); border-radius: 8px; border-left: 3px solid var(--primary); font-size: .82rem; line-height: 1.7; color: var(--text); }
.lin-info-box code { background: var(--card2); padding: 1px 5px; border-radius: 3px; font-size: .8rem; }
.lin-info-box strong { color: var(--primary); }

.lin-legend { display: flex; gap: 1.2rem; margin-bottom: 1rem; flex-wrap: wrap; }
.lin-legend-item { display: flex; align-items: center; gap: 6px; font-size: .78rem; color: var(--text2); }
.lin-legend-dot { width: 12px; height: 12px; border-radius: 3px; flex-shrink: 0; }
```

### 关键设计
- 所有颜色使用 `var(--xxx)` 系统，暗色模式下自动切换
- 帧布局表的字节颜色同时定义 `.dark` 版本（`.dark .lin-frame-table .byte-xxx`）
- `.lin-info-box` 使用 `var(--code-bg)` 背景 + `var(--primary)` 左边框色条
- `.lin-table` 斑马纹使用半透明 `rgba(128,128,128,.04)` 适配双主题

### 验证
```bash
grep -c '.lin-table\|.lin-frame-table\|.lin-info-box\|.lin-legend\|.byte-nad' tools/uds_learning_suite.html
# 预期输出：≥10 个匹配（每个类至少一个定义 + 使用）
```
