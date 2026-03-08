# Agent Platform Refactor Plan

ეს დოკუმენტი განსაზღვრავს Exorcist-ის AI/agent პლატფორმის სამიზნე არქიტექტურას, რათა:

- Copilot, Claude, Codex და მომავალი provider-ები იმუშაონ ერთი და იმავე chat/runtime framework-ზე.
- tools იყოს საერთო ფენა ყველა provider-ისთვის.
- core IDE არ იყოს მიბმული კონკრეტულ vendor-ზე.
- UI, session model, tool runtime და provider transport ერთმანეთისგან გაიყოს.

## 1. Design target

Exorcist-ის AI სისტემა უნდა დაიშალოს 4 ფენად:

1. `Agent Platform Core`
2. `Shared Tool Runtime`
3. `Provider Plugins`
4. `Chat/UI Surfaces`

ეს ნიშნავს:

- Copilot plugin არ უნდა ფლობდეს chat platform-ს.
- Claude/Codex plugins არ უნდა აკეთებდნენ საკუთარ tool runtime-ს.
- tool definitions, permission model, invocation tracing და approval flow უნდა იყოს core-ში.
- provider plugin-ები მხოლოდ model/backend integration-ზე უნდა იყვნენ პასუხისმგებელნი.

## 2. Current state

კოდში უკვე არსებობს კარგი საფუძველი:

- `src/aiinterface.h`
  provider-neutral request/response contract.
- `src/agent/iagentplugin.h`
  agent plugin extension point.
- `src/agent/agentorchestrator.h`
  active provider switching and request routing.
- `src/agent/itool.h`
  shared tool abstraction და `ToolRegistry`.
- `src/agent/modelregistry.h`
  aggregated model metadata registry.
- `src/mainwindow.cpp`
  core tools იქმნება centrally და providers იტვირთება plugins-იდან.

მაგრამ რამდენიმე მნიშვნელოვანი არქიტექტურული პრობლემა ჯერ კიდევ დარჩენილია.

## 3. Current gaps

### 3.1 `MainWindow` არის composition root-ზე ბევრად მეტი

დღეს `MainWindow` აკეთებს:

- provider registration
- tool registration
- tool dependency wiring
- chat controller creation
- chat panel creation
- status UI logic
- MCP tool bridging

ეს ძალიან ბევრი პასუხისმგებლობაა.

### 3.2 `ServiceRegistry` ზედმეტად სუსტია

დღევანდელი `ServiceRegistry`:

- string -> `QObject*`
- no typed access
- no service interfaces
- no lifecycle hooks
- no capability discovery

ეს საკმარისია MVP-სთვის, მაგრამ არა დიდი plugin platform-ისთვის.

### 3.3 Provider plugins ძალიან thin არიან

დღევანდელი plugin model რეალურად მხოლოდ provider creation-ს აკეთებს.

აკლია:

- provider metadata/capability declaration
- auth/settings surfaces
- provider diagnostics
- session import/export hooks
- provider-specific welcome/help integration

### 3.4 Chat runtime და chat UI ზედმეტად coupled არიან

`AgentChatPanel`, `AgentController`, `SessionStore` და orchestrator-ს შორის coupling ჯერ კიდევ მაღალია.

### 3.5 Tool layer shared არის, მაგრამ formal platform contract აკლია

დღეს tools shared-ია ფაქტობრივად, მაგრამ ოფიციალურად არაა გაწერილი:

- ვინ ქმნის tool list-ს request-ისთვის
- provider შეიძლება თუ არა tools filter-დეს capability-ით
- tool result types როგორ იშლება UI ნაწილებად
- approval policy სად ცხოვრობს

## 4. Target architecture

## 4.1 Agent Platform Core

Core layer უნდა შეიცავდეს:

- `AgentProviderRegistry`
- `ModelRegistry`
- `ToolRegistry`
- `ToolApprovalService`
- `ChatSessionService`
- `AgentRequestPipeline`
- `AgentEventBus`
- `SessionImportService`

პასუხისმგებლობა:

- request preparation
- context assembly
- model selection
- tool selection by permission/capability
- request execution lifecycle
- session persistence/import/export
- normalized event emission UI-სთვის

## 4.2 Shared Tool Runtime

tools უნდა იყოს საერთო infrastructure ყველა provider-ისთვის.

ეს ფენა უნდა შეიცავდეს:

- `ToolRegistry`
- `ToolSpec`
- `ToolDefinition`
- `ToolInvocationContext`
- `ToolExecResult`
- `ToolApprovalPolicy`
- `ToolPresentationFormatter`
- `ToolEventRecorder`

მნიშვნელოვანი წესი:

- providers tool logic-ს არ აძლევენ runtime-ში.
- providers მხოლოდ language-model tool-calls აგზავნიან/იღებენ.
- tool execution ყოველთვის core tool runtime-ში ხდება.

გამონაკლისი დასაშვებია მხოლოდ მაშინ, როცა provider-ს აქვს provider-specific remote session import parsing.

## 4.3 Provider Plugins

Provider plugin-ის სწორი პასუხისმგებლობა არის მხოლოდ:

- authentication
- endpoint discovery
- model discovery
- request/stream transport
- vendor-specific response parsing
- provider-specific session import/export adapters
- optional provider-specific settings widget

Provider plugin არ უნდა ფლობდეს:

- shared tools
- chat transcript UI
- session storage backend semantics
- diff/apply/undo logic

## 4.4 Chat/UI Surfaces

UI უნდა დაეყრდნოს normalized chat event/part model-ს:

- request turn
- markdown part
- thinking part
- tool invocation part
- workspace edit part
- followups part
- warning/error/system part

ეს UI შეიძლება გამოიყენოს:

- Copilot panel
- quick chat
- inline chat
- future terminal chat

## 5. Required new interfaces

## 5.1 Provider registry

დღევანდელი `AgentOrchestrator` უნდა დაიშალოს ორ ნაწილად:

- `AgentProviderRegistry`
  provider registration, active provider, provider capabilities.
- `AgentRequestRouter`
  active provider-ზე request dispatch.

`AgentOrchestrator` შეიძლება დროებით დარჩეს facade-ად compatibility-სთვის.

## 5.2 Session service

საჭიროა ახალი `ChatSessionService`:

- create session
- rename session
- delete session
- restore session
- import remote session
- list sessions by provider/type
- persist session metadata and turns

`SessionStore` უნდა გადაიქცეს persistence backend-ად, არა UI-facing service-ად.

## 5.3 Tool approval service

საჭიროა `ToolApprovalService`:

- allow once
- allow always for session
- deny
- capability-based defaults
- audit trail

ეს უნდა გამოეყოს `MainWindow`-ში მიმოფანტული confirmation logic-ებიდან.

## 5.4 Typed service access

`ServiceRegistry` უნდა განვითარდეს ასე:

- დარჩეს string-key lookup compatibility-სთვის
- დაემატოს typed wrapper/access helpers
- service interfaces უნდა გადავიდეს `src/core/` ან `src/agent/` interface headers-ში

მაგალითი მიზნობრივი სერვისებისა:

- `IAgentProviderRegistry`
- `IToolService`
- `IChatSessionService`
- `IAuthService`
- `IModelCatalogService`
- `IContextAssemblyService`

## 6. Plugin model target

დღევანდელი `IPlugin + IAgentPlugin` მოდელი მისაღებია, მაგრამ უნდა გაფართოვდეს შემდეგი მიმართულებით.

### 6.1 Provider plugin contract

ახალი optional interfaces:

- `IAgentProviderPlugin`
  ქმნის provider-ებს.
- `IAgentSettingsPageProvider`
  provider-specific config UI.
- `IChatSessionImporterPlugin`
  provider-specific remote/session transcript import.
- `IProviderAuthIntegration`
  auth status, sign-in/sign-out, token refresh hooks.

### 6.2 რაც არ უნდა გავაკეთოთ

- `ICopilotPlugin`, `IClaudePlugin`, `ICodexPlugin` ცალკე core contract-ებად
- provider-specific branching UI layer-ში
- tools-ის duplication per plugin

## 7. Model architecture target

`ModelRegistry` დღეს გლობალური registry-ა, მაგრამ სამომავლოდ მას უნდა შეეძლოს:

- source provider tracking
- duplicate model IDs-ის provider-qualified keys
- per-mode preferred model
- fallback chains
- capability filters
- provider availability awareness

რეკომენდებული ცვლილება:

- global unique key: `providerId:modelId`
- display surface-ში friendly name
- request contract-ში explicit `providerId` + `modelId`

## 8. Tool architecture target

Tools უნდა დარჩეს საერთო ფენად და დაიშალოს 3 კატეგორიად:

1. `Core IDE tools`
2. `Environment tools`
3. `Optional integration tools`

### 8.1 Core IDE tools

- read/write/apply_patch
- search/semantic search/file search
- diagnostics/problems
- changed files/diff
- symbol/navigation/refactor
- todo/memory/session tools

### 8.2 Environment tools

- run command
- terminal inspection
- notebook tools
- container tools
- web fetch/search

### 8.3 Optional integration tools

- MCP-discovered tools
- GitHub-specific helpers
- future extension integrations

ყველა მათგანი მაინც უნდა დაირეგისტრირდეს ერთიან `ToolRegistry`-ში.

## 9. Runtime pipeline target

რეკომენდებული execution pipeline:

1. UI აგზავნის `ChatSendRequest`-ს session service-ში
2. session service ქმნის request turn-ს
3. request pipeline აგებს `AgentRequest`
4. active provider აგზავნის request-ს
5. provider stream events გარდაიქმნება normalized `ChatEvent`-ებად
6. tool calls იგზავნება shared tool runtime-ში
7. tool results ბრუნდება request pipeline-ში
8. session model და UI ორივე იკვებებიან ერთი და იმავე event stream-ით

ამ pipeline-ით:

- inline chat
- full panel chat
- restored/imported remote sessions

ყველა ერთ contract-ზე დაჯდება.

## 10. Concrete refactor phases

## Phase 1 — Introduce service boundaries

გასაკეთებელი:

- `AgentOrchestrator` split plan document and compatibility facade
- `ChatSessionService` interface
- `ToolApprovalService` interface
- `ProviderRegistry` interface

კოდის მიზანი:

- `MainWindow` აღარ ქმნიდეს ქვედა ფენის policy objects-ს პირდაპირ.

## Phase 2 — Move bootstrap out of `MainWindow`

გასაკეთებელი:

- `AgentPlatformBootstrap` შექმენი
- provider loading/tool loading/context service wiring გადაიტანე იქ
- `MainWindow` მხოლოდ bootstrap-ს იღებდეს service registry-დან

## Phase 3 — Separate persistence from sessions

გასაკეთებელი:

- `SessionStore` დარჩეს storage backend
- ახალი `ChatSessionService` გახდეს API session operations-ისთვის

## Phase 4 — Formalize provider plugin extensions

გასაკეთებელი:

- `IAgentSettingsPageProvider`
- `IChatSessionImporterPlugin`
- `IProviderAuthIntegration`

## Phase 5 — Normalize tool presentation

გასაკეთებელი:

- `ToolPresentationFormatter` core-ში
- tool result -> transcript part transformation
- provider-independent tool visualization contract

## Phase 6 — UI migration

გასაკეთებელი:

- widget-based transcript renderer
- session service-backed history UI
- provider-neutral chat panel

## 11. Immediate implementation priorities

ყველაზე სწორი ახლო ნაბიჯებია:

1. `MainWindow`-დან agent bootstrap-ის ამოღება.
2. `SessionStore`-ის ზემოდან `ChatSessionService`-ის დადება.
3. `AgentOrchestrator`-ის პასუხისმგებლობის გაყოფა registry/router-ად.
4. `ToolApprovalService` და `ToolPresentationFormatter` introduction.
5. ახალი chat UI refactor-ის პარალელურად normalized event/part model-ის შემოტანა.

## 12. Rule of ownership

საბოლოო არქიტექტურული წესი უნდა იყოს:

- Core owns chat runtime.
- Core owns shared tools.
- Plugins own providers.
- UI owns rendering only.
- No provider owns the platform.

თუ ამ წესს დავიცავთ, Copilot იქნება უბრალოდ პირველი სრულფასოვანი provider plugin, ხოლო Claude/Codex იგივე პლატფორმაზე დაჯდება მინიმალური duplication-ით.