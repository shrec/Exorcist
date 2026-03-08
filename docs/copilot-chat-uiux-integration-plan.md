# Copilot Chat UI/UX სრული პორტირების სამუშაო დოკუმენტი

ეს დოკუმენტი არის სამუშაო blueprint, რომლის მიზანია `ReserchRepos/copilot`-ის საფუძველზე GitHub Copilot Chat-ის სრული ვიზუალური და ინტერაქციული გამოცდილების გადატანა Exorcist-ში Qt Widgets-ზე.

## 1. მთავარი დასკვნა

ყველაზე მნიშვნელოვანი ფაქტი, რომელიც ამ ანალიზიდან დადასტურდა:

- `ReserchRepos/copilot` მარტო არ შეიცავს Copilot Chat-ის მთელ UI-ს.
- Copilot extension side ამ repo-ში მართავს participant-ებს, session content provider-ებს, tool invocation semantics-ს, model selection-ს, prompt shaping-ს, follow-up generation-ს და session reconstruction-ს.
- რეალური chat panel/workbench rendering-ის დიდი ნაწილი VS Code core-შია, ძირითადად `microsoft/vscode`-ს `src/vs/workbench/contrib/chat/` ქვეშ.

აქედან გამომდინარეობს სწორი სტრატეგია:

- Exorcist-ში უნდა დაიპორტოს არა მხოლოდ Copilot plugin logic.
- უნდა აშენდეს VS Code workbench chat widget-ის Qt equivalent.
- Copilot plugin უნდა დარჩეს provider/integration layer-ად, ხოლო UI უნდა გაიყოს reusable chat framework-ად.

## 2. რა იპოვა ანალიზმა

### 2.1 Copilot repo-ში რა ეკუთვნის extension-side semantics-ს

ეს ფაილები განსაზღვრავს რა უნდა იცოდეს Exorcist-ის chat layer-მა:

- `ReserchRepos/copilot/package.json`
  Copilot-ის `contributes.languageModelTools`, tool contracts, activation surface, proposed API usage.
- `ReserchRepos/copilot/src/extension/conversation/vscode-node/chatParticipants.ts`
  Ask/Edit/Agent/Terminal/Notebook participants, მათი icon/title/welcome behavior და model switching quota behavior.
- `ReserchRepos/copilot/src/extension/conversation/vscode-node/welcomeMessageProvider.ts`
  additional welcome messaging.
- `ReserchRepos/copilot/src/extension/chatSessions/vscode-node/chatSessions.ts`
  contributed chat session providers, Claude/Copilot CLI/Copilot Cloud session registration, session item/content provider wiring.
- `ReserchRepos/copilot/src/extension/chatSessions/vscode-node/chatHistoryBuilder.ts`
  Claude-style JSONL history reconstruction: markdown parts, thinking parts, tool invocations, slash-command turns, subagent tool injection.
- `ReserchRepos/copilot/src/extension/chatSessions/vscode-node/copilotCloudSessionContentBuilder.ts`
  cloud session log parsing, PR card insertion, tool invocation part creation, diff/file-aware tool labels.
- `ReserchRepos/copilot/src/extension/chatSessions/copilotcli/common/copilotCLITools.ts`
  tool invocation friendly naming, pre/post formatting, Bash/edit/search/todo/task/subagent presentation semantics.
- `ReserchRepos/copilot/src/extension/agentDebug/vscode-node/toolResultContentRenderer.ts`
  tool result content normalization.
- `ReserchRepos/copilot/src/extension/completions-core/vscode-node/extension/src/panelShared/themes/dark-modern.ts`
  color tokens used as visual baseline.

### 2.2 VS Code core-ში რა ეკუთვნის actual chat UI-ს

ეს ნაწილი ამ workspace-ში არ დევს, მაგრამ სრული ვიზუალური parity-ისთვის აუცილებელი reference-ია:

- `microsoft/vscode: src/vs/workbench/contrib/chat/browser/widgetHosts/viewPane/chatViewPane.ts`
  chat panel host, mode-dependent view options, followups/rendering behavior, working set toggles.
- `microsoft/vscode: src/vs/workbench/contrib/chat/browser/widget/chatWidget.ts`
  overall widget composition: list, input, followups, welcome, checkpoints.
- `microsoft/vscode: src/vs/workbench/contrib/chat/browser/widget/input/chatInputPart.ts`
  attachments, implicit context, model/mode controls, input toolbars, followups, working-set UI, history navigation.
- `microsoft/vscode: src/vs/workbench/contrib/chat/browser/widget/chatListRenderer.ts`
  central renderer for markdown, thinking, tool invocations, subagents, attachments, workspace edits, followups, PR cards, extensions, errors.
- `microsoft/vscode: src/vs/workbench/contrib/chat/browser/widget/chatContentParts/*`
  concrete rendering subcomponents.
- `microsoft/vscode: src/vs/workbench/contrib/chat/common/model/chatModel.ts`
  session/request/response model.
- `microsoft/vscode: src/vs/workbench/contrib/chat/common/chatService/chatServiceImpl.ts`
  request lifecycle, queuing, session restore, followups, request adoption/resend/cancel.

## 3. Exorcist-ის ამჟამინდელი მდგომარეობა

ამჟამად Exorcist-ში chat UI-ის დიდი ნაწილი კონცენტრირებულია ერთ მონოლითურ widget-ში:

- `src/agent/agentchatpanel.h`
- `src/agent/agentchatpanel.cpp`

არსებული შესაძლებლობები უკვე ძლიერია:

- multiline input
- slash menu
- mention/file menu
- streaming response
- thinking stream
- working bar
- changes bar
- attachments
- model picker
- session history menu
- context strip

მაგრამ სტრუქტურული პრობლემა აშკარაა:

- `AgentChatPanel` ერთდროულად არის layout host, state manager, renderer, streaming controller, attachment manager, session UI, command UI და history store bridge.
- `QTextBrowser + HTML` approach საკმარისია baseline-ისთვის, მაგრამ VS Code parity-ისთვის აღარ არის სწორი abstraction.

სრული UI/UX parity-ისთვის საჭიროა rendering-ის გადასვლა component-based widget architecture-ზე.

## 4. სამიზნე არქიტექტურა Exorcist-ში

### 4.1 სწორი დაშლა

Copilot UI Exorcist-ში არ უნდა დარჩეს `AgentChatPanel`-ის ერთ ფაილად. სწორი სამიზნე არის ასეთი ფენა:

- `ChatPanelShell`
  ზედა host widget, layout orchestration, dock integration.
- `ChatSessionModel`
  ერთი conversation-ის request/response state, queued request, followups, title, timestamps, mode, selected model.
- `ChatTranscriptView`
  scrollable transcript container.
- `ChatTurnWidget`
  ერთი request/response pair container.
- `ChatContentPartWidget` hierarchy
  markdown, thinking, tool invocation, workspace edit, attachment list, followup row, PR/change summaries.
- `ChatInputWidget`
  editor, attachments strip, mode/model controls, side toolbar, send/cancel.
- `ChatSessionHistoryPopup`
  history/search/rename/delete UI.
- `ChatThemeTokens`
  VS Code-like token table mapped to Qt palette/styles.

### 4.2 რეკომენდებული ახალი Qt კომპონენტები

- `src/agent/chat/chatpanelwidget.h/.cpp`
- `src/agent/chat/chatsessionmodel.h/.cpp`
- `src/agent/chat/chatturnmodel.h/.cpp`
- `src/agent/chat/chatcontentpart.h`
- `src/agent/chat/chattranscriptview.h/.cpp`
- `src/agent/chat/chatinputwidget.h/.cpp`
- `src/agent/chat/chatattachmentstrip.h/.cpp`
- `src/agent/chat/chatfollowupswidget.h/.cpp`
- `src/agent/chat/chattoolinvocationwidget.h/.cpp`
- `src/agent/chat/chatthinkingwidget.h/.cpp`
- `src/agent/chat/chatworkspaceeditwidget.h/.cpp`
- `src/agent/chat/chatsessionhistorypopup.h/.cpp`
- `src/agent/chat/chatthemetokens.h/.cpp`

`AgentChatPanel` უნდა დარჩეს compatibility facade-ად და ეტაპობრივად გადაიქცეს მხოლოდ composition root-ად.

## 5. Component mapping: VS Code/Copilot -> Exorcist

| Source ownership | Source behavior | Exorcist target |
|---|---|---|
| VS Code `chatWidget.ts` | full chat composition | `ChatPanelShell` + `ChatTranscriptView` + `ChatInputWidget` |
| VS Code `chatInputPart.ts` | input editor, attachments, followups, toolbar, working set | `ChatInputWidget` + `ChatAttachmentStrip` + `ChatFollowupsWidget` |
| VS Code `chatListRenderer.ts` | render all content kinds | `ChatTurnWidget` + `ChatContentPartWidget` factory |
| VS Code `chatToolInvocationPart.ts` | tool streaming/completed/confirmation UI | `ChatToolInvocationWidget` |
| VS Code `chatProgressContentPart.ts` | progress and working stream | `ChatProgressWidget` / `ChatThinkingWidget` |
| VS Code `chatWorkspaceEditContentPart.ts` | edited file summaries | `ChatWorkspaceEditWidget` |
| VS Code `chatSubagentContentPart.ts` | grouped subagent block | `ChatSubagentWidget` |
| VS Code `chatFollowups.ts` | clickable followups | `ChatFollowupsWidget` |
| Copilot `chatParticipants.ts` | Ask/Edit/Agent participant semantics | `CopilotProvider` + `ChatMode` mapping |
| Copilot `chatHistoryBuilder.ts` | reconstructed turns from JSONL | `ChatHistoryImporter` |
| Copilot `copilotCloudSessionContentBuilder.ts` | cloud log -> parts | `CloudSessionImporter` |
| Copilot `copilotCLITools.ts` | tool labels and messages | `ToolPresentationFormatter` |

## 6. რა უნდა დაიპორტოს აუცილებლად

სრული parity-ისთვის ვიზუალური პორტი უნდა მოიცავდეს არა მხოლოდ message bubble-ებს, არამედ ქვემოთ მოცემულ ყველა UX ერთეულს:

### 6.1 Input UX

- inline mode selector input-ის ზონაში
- model picker input/footer chrome-ში და არა უბრალოდ ცალკე combo
- attachment strip იგივე hierarchy-ით: files, images, prompt files, implicit context
- slash commands
- `@`/`#`/tool references
- input history navigation
- send/cancel/request in progress state
- followup buttons input-სთან კავშირში
- context usage widget
- editing session working set summary

### 6.2 Transcript UX

- request/response turn pairing
- assistant name/icon and mode-aware identity
- timestamps
- markdown and code block parity
- inline references/file pills
- feedback row
- retry/edit/copy actions
- welcome screen
- quota/warning/error banners

### 6.3 Streaming UX

- token-by-token markdown streaming
- separate thinking stream
- tool invocation streaming state
- confirmation state
- completed state with past-tense message
- spinner/progress state when tool is active
- subagent grouping
- working stream pinned under thinking when appropriate

### 6.4 Change visualization UX

- changed-files summary inside transcript, არა მარტო გარე `changesBar`
- workspace edit groups
- per-file change summary
- optional expandable diff preview
- kept/undone/applied state indicators
- edit session working set collapse/expand

### 6.5 Session UX

- session title generation/update
- new chat
- history search
- rename
- delete
- restore previous session
- import reconstructed sessions from provider logs

## 7. ძირითადი ტექნიკური gap-ები Exorcist-ში

### 7.1 ყველაზე დიდი gap

`QTextBrowser`-ზე აგებული single HTML transcript ვერ მოგცემს იგივე ხარისხის behavior-ს შემდეგი შემთხვევებისთვის:

- streaming tool card rerender
- inline confirmation widgets
- collapsible IO sections
- diff/editor embedding
- subagent grouping
- pinned thinking/tool interleaving
- followups/stateful action rows
- todo list widgets

შედეგად:

- markdown rendering შეიძლება დარჩეს HTML-based,
- მაგრამ transcript უნდა გადავიდეს widget-per-part ან delegate-based rendering-ზე.

### 7.2 სწორი ალტერნატივა

ორი რეალისტური ვარიანტია:

1. `QScrollArea + QWidget tree`
2. `QListView/QAbstractItemView + custom delegates`

ამ პროექტის ამჟამინდელი მდგომარეობიდან უფრო პრაქტიკულია პირველი ვარიანტი:

- ნაკლები framework friction
- უფრო მარტივი collapsible blocks
- მარტივი embedded controls
- step-by-step migration შესაძლებელია `AgentChatPanel`-დან

## 8. პორტირების სწორი თანმიმდევრობა

## Phase 0 — Freeze semantics

ამ ფაზის მიზანია UI refactor-მდე state contracts-ის დაფიქსირება.

გასაკეთებელი:

- `AgentResponse`-ში მკაფიოდ გაიყოს response parts: markdown, thinking, toolInvocation, workspaceEdit, followups, warnings, errors.
- შეიქმნას `ChatContentPartModel` union ტიპი C++-ში.
- tool lifecycle დაფიქსირდეს: queued -> streaming -> confirmationNeeded -> completeSuccess -> completeError.
- session model-ში დაემატოს title, timestamps, mode, selectedModel, followups, pending queue.

სავარაუდო ფაილები:

- `src/agent/agentsession.h`
- `src/agent/agentcontroller.*`
- `src/agent/agentchatpanel.*`
- ახალი `src/agent/chat/*model*`

## Phase 1 — Extract input from `AgentChatPanel`

მიზანია input UX გახდეს დამოუკიდებელი და testable.

გასაკეთებელი:

- `ChatInputWidget` გამოყავი `AgentChatPanel`-დან.
- გადაიტანე აქ: input editor, send/cancel, attachments strip, slash menu, mention menu, mode picker, model picker, context usage widget.
- modes ქვედა bar-იდან გადაიტანე input chrome-ში.
- დაამატე followups container input-ს ქვემოთ/ზემოთ mode-aware layout-ით.

acceptance:

- `AgentChatPanel` აღარ მართავს editor internals-ს.
- input-specific state აღარ ინახება HTML transcript code-ში.

## Phase 2 — Replace monolithic transcript renderer

მიზანია `QTextBrowser`-ის ფუნქციის დაშლა.

გასაკეთებელი:

- დაამატე `ChatTranscriptView`.
- თითო turn გახდეს `ChatTurnWidget`.
- თითო response part გახდეს ცალკე widget:
  - markdown
  - thinking
  - tool invocation
  - workspace edit
  - followups
  - warning/error
- code block action row ამოვიდეს HTML link scheme-ებიდან widget actions-ზე.

acceptance:

- tool cards დამოუკიდებლად rerender-დება მთლიან transcript-ის ხელახლა აგების გარეშე.
- streaming update აღარ საჭიროებს სრულ HTML rewrite-ს.

## Phase 3 — Streaming parity

მიზანია Copilot/VS Code-ის მსგავსი incremental response UX.

გასაკეთებელი:

- markdown stream buffering per active response part
- thinking stream widget
- tool invocation stream widget
- spinner/progress widget
- state transitions confirmation/completion-ზე
- pinned working stream behavior agent mode-ში

აუცილებელი behavior:

- ერთი response-ში parts interleave უნდა იყოს შესაძლებელი
- thinking დასრულების შემდეგ უნდა დაიხუროს ან collapse-დებოდეს
- tool invocation completion უნდა ცვლიდეს presentation-ს streaming view-დან completed view-ზე

## Phase 4 — Tool invocation system visual parity

ეს არის ყველაზე მნიშვნელოვანი UX ბლოკი Ask/Edit/Agent parity-ისთვის.

გასაკეთებელი:

- `ToolPresentationFormatter` შექმენი `copilotCLITools.ts`-ის logic-ის საფუძველზე.
- თითო tool-ს ჰქონდეს:
  - friendly title
  - invocation message
  - past tense message
  - origin message
  - tool-specific payload
  - confirmation requirement
  - result details
- widget-ს მხარდაჭერა ჰქონდეს:
  - simple progress row
  - terminal command preview
  - read/edit/create file message
  - collapsible input/output panel
  - file/resource list
  - todo list state sync
  - MCP/tool output rendering

სავარაუდო ფაილები:

- ახალი `src/agent/chat/toolpresentationformatter.*`
- ახალი `src/agent/chat/chattoolinvocationwidget.*`

## Phase 5 — Workspace edit and change history UX

დღევანდელი `changesBar` საკმარისი არაა.

გასაკეთებელი:

- transcript-ში ჩაჯდეს edited-files summary block
- თითო edited file clickable გახდეს
- expandable diff summary დაემატოს
- Keep/Undo გადაიყვანე global bar-იდან response-bound action area-ში
- edit session working set sidebar/inline section დაამატე

VS Code mapping:

- `ChatWorkspaceEditContentPart`
- editing session working set inside `chatInputPart.ts`

## Phase 6 — Session history parity

გასაკეთებელი:

- history popup widget with search field
- local sessions + provider-backed sessions ერთიან სიაში
- rename/delete/new session commands
- session status badges
- restored session rendering from imported parts

საჭირო back-end:

- `SessionStore` metadata enrichment
- mode/model/title persistence
- imported session source type

## Phase 7 — Welcome, followups, error and state surfaces

გასაკეთებელი:

- empty state variants
- auth/quota/network/tool error states
- followup chips/buttons
- warning banners
- model switched / premium quota exhausted informational row

Reference ownership:

- Copilot `welcomeMessageProvider.ts`
- VS Code `ChatFollowups`
- VS Code chat error/quota content parts

## Phase 8 — Visual polish and theme parity

გასაკეთებელი:

- `ChatThemeTokens` ააწყე `dark-modern.ts`-ის ბაზაზე
- fonts: `'Segoe WPC', 'Segoe UI'`
- spacing system და border radii გააერთიანე
- link blue, button blue, input bg, panel bg სრულად გაასწორე
- hover, focus, pressed, disabled states
- scrollbars
- accessible names/tooltips/tab order

## 9. კონკრეტული integration plan Copilot plugin-თან

რადგან Copilot plugin უკვე მეტწილად დასრულებულია, სწორი ინტეგრაცია ასეთი უნდა იყოს:

### 9.1 UI framework უნდა იყოს provider-agnostic

UI არ უნდა იყოს მიბმული კონკრეტულად Copilot provider-ზე.

ამის ნაცვლად:

- core chat UI უნდა იკვებებოდეს generic `ChatSessionModel`-ით
- Copilot plugin უნდა აწვდიდეს normalized parts-ს
- Claude/Codex სხვა provider-ებიც შეძლებენ იგივე UI contract-ის გამოყენებას

### 9.2 Copilot plugin-ის პასუხისმგებლობები

- participant/mode metadata
- model list and capabilities
- raw streaming events
- tool invocation lifecycle events
- session import/reconstruction
- followups
- workspace edits metadata

### 9.3 Chat UI layer-ის პასუხისმგებლობები

- render all parts consistently
- local interaction state
- collapse/expand
- selection/copy/open file/apply undo actions
- per-turn visual transitions

## 10. პირველი რეალური refactor backlog

ეს არის რეკომენდებული პირველი პრაქტიკული სერია, რომლითაც პორტი სწრაფად წავა წინ რისკის გარეშე:

1. `AgentChatPanel`-იდან გამოყავი `ChatInputWidget`.
2. შემოიტანე `ChatSessionModel` და turn/part model structs.
3. `QTextBrowser`-ის პარალელურად ააწყე ახალი `ChatTranscriptView` feature flag-ით.
4. პირველად გადაიტანე მხოლოდ markdown + thinking + tool invocation parts.
5. შემდეგ გადაიტანე workspace edit/change summary.
6. ბოლოს გადაიტანე session history popup/followups/welcome variants.

## 11. Done definition

პორტი ჩაითვლება დასრულებულად მხოლოდ მაშინ, როცა შესრულდება ეს კრიტერიუმები:

- Ask/Edit/Agent mode-ები ვიზუალურად განსხვავდება და mode-aware chrome აქვთ.
- streaming response აღარ არის plain HTML rewrite loop.
- thinking/tool/subagent blocks აქვთ ცალკე widget presentation.
- changed files/history/followups/session restore მუშაობს transcript-native სახით.
- model picker/mode picker/input attachments/followups განლაგებით ახლოსაა VS Code chat widget-თან.
- `AgentChatPanel` აღარ შეიცავს transcript HTML/CSS მონოლითს.
- Copilot provider-ს შეუძლია reconstructed remote sessions-ის იგივე UI-ში ჩასმა.

## 12. რისი გაკეთება არ უნდა ვცადოთ პირველივე ეტაპზე

- სრულ DOM/CSS parity-ს ზუსტად 1:1 კოპირება
- ყველა content part-ის ერთდროულად გადატანა
- Copilot-specific hacks UI layer-ში ჩადება
- `QTextBrowser`-ის ზედმეტად გაზრდა იმის ნაცვლად, რომ widget architecture-ზე გადავიდეთ

სწორი გზა არის:

- ჯერ part model
- მერე component rendering
- მერე progressive replacement

## 13. მოკლე რეკომენდაცია

თუ მიზანი არის არა უბრალოდ "კარგი chat UI", არამედ რეალურად Copilot Chat-ის UI/UX parity, მაშინ შემდეგი არქიტექტურული ნაბიჯი აუცილებელია:

- `AgentChatPanel` უნდა დაიშალოს reusable chat framework-ად
- Copilot plugin უნდა დაჯდეს ამ framework-ზე
- VS Code workbench chat rendering უნდა აღვიქვათ როგორც design reference, ხოლო `ReserchRepos/copilot` როგორც behavior/semantics reference

სხვა სიტყვებით:

- UI პორტი არ არის მხოლოდ CSS tuning
- ეს არის transcript engine + input system + part renderer + session UX-ის ხელახლა აწყობა Qt Widgets-ზე

## 14. Platform dependency

ეს UI/UX პორტირება სწორად შესრულდება მხოლოდ მაშინ, თუ parallel-ად გავასწორებთ agent platform-ის არქიტექტურას:

- Copilot plugin უნდა იყოს provider plugin.
- Claude/Codex უნდა დაჯდეს იმავე runtime-ზე.
- shared tools უნდა დარჩეს core-ში.
- chat UI უნდა ეყრდნობოდეს shared session/runtime model-ს.

ამ არქიტექტურული refactor-ის ცალკე გეგმა აღწერილია `docs/agent-platform-refactor.md`-ში.