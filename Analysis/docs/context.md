# Context for Refactor Handoff (One-shot Cutover on Top 3)

This document provides all context needed to execute a one-shot, thorough refactor focused on three classes. The goal is to fully switch the affected flows without keeping dual paths or temporary compatibility code.

## Deliverable
- One-shot cutover that removes old implementations and routes all relevant paths through three new classes:
  - AlgorithmExecutionController (algorithm execution: point selection, parameter dialog, progress/cancel, complete/fail, workflow callbacks)
  - MessagePresenter (centralized message presentation; concrete Qt-based implementation; no interface layer this round)
  - DeleteCurveUseCase (delete/cascade-delete business logic and command execution)

## Scope and Non‑Goals
- In scope:
  - Algorithm execution flow end-to-end: point selection → param dialog → progress/cancel → completion/failure → workflow notifications.
  - Curve delete/cascade-delete: validation, children aggregation, confirmation, command execution.
  - Controller-layer message presentation unification (replace direct QMessageBox calls) for the above flows.
- Not in scope (this round):
  - Import flow refactor, view tool controller split, parameter/dialog builders, or additional interfaces.
  - Any extra controllers or presenters beyond the three listed.
- Cutover policy: no dual implementations. Remove old code after migrating.

## Repository Snapshot
- Controllers
  - MainController: `src/ui/controller/main_controller.cpp:1`
  - CurveViewController: `src/ui/controller/curve_view_controller.cpp:1`
- App Composition
  - ApplicationContext: `src/application/application_context.cpp:1`
- Managers and Services
  - Algorithm: `src/application/algorithm/*`
  - History: `src/application/history/*`
  - Curves/Project Tree: `src/application/curve/*`, `src/application/project/*`
- Project file (qmake): `Analysis.pro:1`

## Problems to Fix (Summary)
- MainController is overloaded (assembly + UI + business).
- Business and UI are mixed (especially delete path), hard to reuse/test.
- Algorithm flow (point/params/progress/completion) is embedded in MainController.
- Message dialogs are called directly from controllers.

## Plan and Tasks
- Execution Plan (one-shot cutover): `docs/execution_plan_top3.md:1`
- Task List (one-shot cutover): `docs/task_list_top3.md:1`

## Code Hotspots to Migrate / Replace
- Algorithm flow (move to AlgorithmExecutionController):
  - Point selection start/hint: `src/ui/controller/main_controller.cpp:430`
  - Parameter dialog creation/extraction: `src/ui/controller/main_controller.cpp:706`, `src/ui/controller/main_controller.cpp:765`, `src/ui/controller/main_controller.cpp:844`
  - Progress/cancel: `src/ui/controller/main_controller.cpp:595`
  - Completion/failure/workflow: `src/ui/controller/main_controller.cpp:492`, `src/ui/controller/main_controller.cpp:516`, `src/ui/controller/main_controller.cpp:535`
- Delete use case (move to DeleteCurveUseCase):
  - Delete slot: `src/ui/controller/main_controller.cpp:344`
- Message dialogs (replace with MessagePresenter):
  - Examples: `src/ui/controller/main_controller.cpp:466`, `src/ui/controller/main_controller.cpp:551`
- Wiring / composition:
  - Instantiate and connect in `src/application/application_context.cpp:65`

## New Classes (files and responsibilities)
- AlgorithmExecutionController
  - Files: `src/ui/controller/algorithm_execution_controller.h`, `src/ui/controller/algorithm_execution_controller.cpp`
  - Responsibilities: own the full algorithm execution flow; connect to AlgorithmCoordinator and ChartView; show param dialog; show/update progress; handle cancellation; handle completion/failure/workflow.
- MessagePresenter
  - Files: `src/ui/presenter/message_presenter.h`, `src/ui/presenter/message_presenter.cpp`
  - Responsibilities: centralized confirm/info/warn; replace all controller-layer QMessageBox calls in the affected flows.
- DeleteCurveUseCase
  - Files: `src/application/usecase/delete_curve_use_case.h`, `src/application/usecase/delete_curve_use_case.cpp`
  - Responsibilities: validate, aggregate children for cascade delete, execute history commands; return status to the caller; MessagePresenter handles confirmations.

## Target State for MainController
- Keep only orchestrator responsibilities and `onAlgorithmRequested` entry/logging.
- Remove parameter dialog/progress/point selection/delete business implementations and related members.

## Acceptance Criteria
- No direct `QMessageBox::` calls remain in controller sources for the affected paths.
- MainController no longer contains parameter/progress/point selection/delete implementations (search by method names).
- Algorithm execution and delete flows behave as before (logs, UX) and build/run is green.

## Verification (suggested ripgrep checks)
- `rg -n "QMessageBox::" src/ui/controller`
- `rg -n "onRequestParameterDialog|createParameterWidget|extractParameters" src/ui/controller/main_controller.cpp`
- `rg -n "onAlgorithmStarted|onAlgorithmProgress" src/ui/controller/main_controller.cpp`
- `rg -n "onCurveDeleteRequested" src/ui/controller/main_controller.cpp`

## Build / Run Notes
- Qt project (qmake, C++17, Qt Widgets + Qt Charts).
  - Project: `Analysis.pro:1`
  - Add new files to SOURCES/HEADERS in `Analysis.pro`.
  - Windows UTF-8 flags already set; translations via `lrelease`.

## Implementation Notes
- Preserve existing Chinese log/prompt text for predictable regression checks.
- Centralize creation/injection in ApplicationContext; controllers should not `new` QWidget directly.
- Use `deleteLater` semantics for dialogs as in existing patterns.
- After adding files, update `Analysis.pro` with new paths:
  - SOURCES: add
    - `src/ui/controller/algorithm_execution_controller.cpp`
    - `src/ui/presenter/message_presenter.cpp`
    - `src/application/usecase/delete_curve_use_case.cpp`
  - HEADERS: add
    - `src/ui/controller/algorithm_execution_controller.h`
    - `src/ui/presenter/message_presenter.h`
    - `src/application/usecase/delete_curve_use_case.h`

## Start Here (T1)
- Instantiate the three classes in `src/application/application_context.cpp:65`, inject dependencies, and connect AlgorithmCoordinator/ChartView signals to AlgorithmExecutionController.
- Adjust MainController so `onAlgorithmRequested` only forwards to AlgorithmExecutionController and logs.
- Proceed with T2–T6 in `docs/task_list_top3.md:1`.

