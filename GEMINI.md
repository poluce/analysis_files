# GEMINI Project Context

## Directory Overview

This directory contains the architectural and design documentation for a **DSC/TGA Thermal Analysis Software**. The project is a C++ application built with the Qt framework. The documents provide a comprehensive overview of the software's design, from a high-level 4-layer architecture to detailed class designs and future improvement plans.

The core design is a 4-layer architecture:
1.  **Presentation Layer:** The user interface, built with Qt Widgets.
2.  **Application Layer:** Coordinates business logic, services, and controllers.
3.  **Domain Layer:** Defines the core business models and interfaces.
4.  **Infrastructure Layer:** Handles technical implementations like file I/O and specific algorithms.

## Key Files

*   `🏗️ DSCTGA热分析软件 - 模块化架构设计.md`: The main architecture document. It outlines the 4-layer architecture, module divisions, core module designs, and design principles.
*   `1023四层架构详细解析.md`: A deep dive into the 4-layer architecture, explaining the responsibilities of each layer and module in detail.
*   `数据架构分析案例.md`: A simplified, real-world analogy to explain the data flow and responsibilities within the architecture.
*   `展示/`: This directory contains PlantUML diagrams and other visual documentation.
    *   `架构层级图.puml` & `架构图.puml`: PlantUML source for the detailed and simplified architecture diagrams.
    *   `信号流向设计说明.md`: Explains the signal/slot communication patterns between different layers of the application.
    *   `MDI多文档架构改进方案.md`, `MDI架构对比图.puml`, `MDI架构详细设计.puml`: A set of documents proposing and detailing a significant architectural improvement to support a Multi-Document Interface (MDI), allowing users to work with multiple datasets simultaneously.

## Usage

This directory serves as the primary technical reference for the "ThermalAnalysis" software project. It should be used to:

*   Understand the overall software architecture and design patterns.
*   Guide new feature development and ensure it aligns with the existing structure.
*   Provide context for onboarding new developers.
*   Review and discuss future architectural improvements, such as the proposed MDI architecture.
