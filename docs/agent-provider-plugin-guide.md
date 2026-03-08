# Agent Provider Plugin Guide

ეს გიდი აღწერს როგორ უნდა დაიწეროს ახალი AI provider plugin Exorcist-ისთვის ისე, რომ:

- ჩაჯდეს არსებულ agent platform-ში
- გამოიყენოს shared tools
- არ დაადუბლიროს chat runtime
- ადვილად შენარჩუნებადი იყოს

ეს არის რეკომენდებული გზა ისეთი plugin-ებისთვის, როგორიცაა:

- Copilot
- Claude
- Codex
- OpenAI-compatible custom endpoints
- Ollama
- Gemini
- xAI

## 1. მთავარი წესი

Provider plugin არის backend adapter, არა ცალკე chat სისტემა.

plugin-ის პასუხისმგებლობა:

- auth/config
- model discovery
- request transport
- streaming parse
- provider-specific response normalization

plugin-ის პასუხისმგებლობა არ არის:

- tool implementations
- session store
- chat transcript UI
- inline chat UI
- diff/apply/undo core logic

ეს ყველაფერი უნდა დარჩეს shared platform-ში.

## 2. მინიმალური contract

ახალი provider plugin ჩვეულებრივ implements-ს აკეთებს ორ interface-ზე:

- `IPlugin`
- `IAgentPlugin`

საფუძველი:

- `src/plugininterface.h`
- `src/agent/iagentplugin.h`
- `src/aiinterface.h`

მინიმალური skeleton:

```cpp
class MyProviderPlugin : public QObject, public IPlugin, public IAgentPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IAgentPlugin)

public:
    PluginInfo info() const override;
    void initialize(QObject *services) override;
    void shutdown() override;
    QList<IAgentProvider *> createProviders(QObject *parent) override;
};
```

Provider class უნდა implement-ებდეს `IAgentProvider`-ს.

## 3. Provider class checklist

`IAgentProvider` implementation-ში აუცილებლად უნდა არსებობდეს:

- `id()`
- `displayName()`
- `capabilities()`
- `isAvailable()`
- `availableModels()`
- `currentModel()`
- `setModel()`
- `initialize()`
- `shutdown()`
- `sendRequest()`
- `cancelRequest()`

თუ provider-ს richer model metadata აქვს, უნდა დააბრუნოს:

- `modelInfoList()`
- `currentModelInfo()`

## 4. Recommended file layout

ახალი plugin-ისთვის რეკომენდებული structure:

```text
plugins/myprovider/
  CMakeLists.txt
  myproviderplugin.h
  myproviderplugin.cpp
  myproviderprovider.h
  myproviderprovider.cpp
```

თუ provider-ს auth/settings flow რთული აქვს, დაამატე:

```text
  myproviderauthmanager.h
  myproviderauthmanager.cpp
  myprovidersettingswidget.h
  myprovidersettingswidget.cpp
```

მაგრამ chat UI widget-ები აქ არ უნდა გაჩნდეს.

## 5. Request handling rules

`sendRequest(const AgentRequest &request)` უნდა იქცეოდეს ასე:

1. გადაამოწმე auth/config availability.
2. გადააქციე `AgentRequest` provider-specific payload-ად.
3. გაითვალისწინე:
   - conversation history
   - selected model
   - reasoning effort
   - tools list
   - active file/context
4. თუ provider supports streaming-ს:
   - emit `responseDelta()`
   - emit `thinkingDelta()` საჭიროების შემთხვევაში
5. დასრულებისას emit `responseFinished()`.
6. შეცდომისას emit `responseError()`.

## 6. Tool handling rules

ყველაზე მნიშვნელოვანი compatibility rule:

- provider plugin არ ასრულებს IDE tools-ს.
- provider plugin მხოლოდ გადასცემს tool schema-ს მოდელს და აბრუნებს model-emitted tool calls-ს.

shared tool runtime არის core-ში:

- `ToolRegistry`
- `ITool`
- `ToolSpec`
- `ToolExecResult`

შედეგად, ნებისმიერი ახალი provider ავტომატურად იღებს იგივე tools-ს, რასაც Copilot/Claude/Codex იღებენ.

## 7. Capabilities declaration

`capabilities()` უნდა იყოს ზუსტი და არა optimistic.

მაგალითად:

```cpp
AgentCapabilities MyProvider::capabilities() const
{
    return AgentCapability::Chat
        | AgentCapability::Streaming
        | AgentCapability::ToolCalling
        | AgentCapability::CodeEdit;
}
```

არ მონიშნო capability, თუ რეალურად სტაბილურად არ მუშაობს:

- `InlineCompletion`
- `ToolCalling`
- `Vision`
- `Thinking`

UI და runtime სწორედ ამ flags-ზე დაყრდნობით უნდა იქცეოდეს.

## 8. Model discovery rules

თუ provider-ს model discovery endpoint აქვს, plugin-მა უნდა:

- გამოიძახოს initialization-ისას ან lazily
- შეინახოს normalized `ModelInfo`
- emit `modelsChanged()` როდესაც list იცვლება

რეკომენდებულია `ModelInfo`-ში შეივსოს:

- `id`
- `name`
- `vendor`
- `version`
- `capabilities`
- `billing`
- `supportedEndpoints`

## 9. Auth/config rules

თუ provider-ს სჭირდება API key/OAuth/token refresh:

- auth logic გამოეყოს provider transport class-იდან როცა ზომა იზრდება
- sensitive values არ შეინახო plain text-ში
- `isAvailable()` ასახავდეს რეალურ usable state-ს
- availability change-ზე emit `availabilityChanged()`

## 10. Error normalization

ყველა provider უნდა map-ავდეს თავის შეცდომებს `AgentError`-ზე:

- `NetworkError`
- `AuthError`
- `RateLimited`
- `Cancelled`
- `Unknown`

ეს აუცილებელია unified UI behavior-ისთვის.

## 11. Streaming normalization

provider-specific streaming ფორმატები შეიძლება განსხვავდებოდეს:

- SSE
- chunked JSON
- websocket
- messages API

მაგრამ UI-სთვის შედეგი ერთნაირი უნდა იყოს:

- `responseDelta(requestId, textChunk)`
- `thinkingDelta(requestId, textChunk)`
- `responseFinished(requestId, response)`

თუ provider tool calls-ს incremental-ად აგზავნის, რეკომენდებულია plugin-მა დააგროვოს intermediate state და `AgentResponse.toolCalls`-ში normalized სახით დააბრუნოს.

## 12. Session import support

თუ provider-ს აქვს remote/cloud/CLI session log format, კარგი plugin-ის დამატებითი შესაძლებლობაა:

- remote transcript import
- session reconstruction
- provider-specific history parser

მაგრამ ესეც უნდა დასრულდეს shared chat part model-ზე და არა provider-specific widget-ებზე.

## 13. Minimal implementation recipe

ახალი provider plugin-ის დასამატებლად პრაქტიკული ნაბიჯები:

1. დააკოპირე არსებული ერთ-ერთი plugin directory scaffold-ად.
2. შეცვალე `PluginInfo` metadata.
3. შექმენი `IAgentProvider` implementation.
4. დაამატე auth/config loading.
5. დაამატე model discovery.
6. დაამატე request building from `AgentRequest`.
7. დაამატე streaming/response parse.
8. დარწმუნდი, რომ tool definitions passed through correctly.
9. შეამოწმე `isAvailable()` და `availabilityChanged()` behavior.
10. დაამატე `CMakeLists.txt` entry და ჩატვირთვის ტესტი.

## 14. Author checklist

- Plugin implements `IPlugin` and `IAgentPlugin`
- Provider implements `IAgentProvider`
- No shared tools duplicated in plugin
- No chat UI classes added in plugin
- Capabilities are accurate
- Errors are normalized to `AgentError`
- Models emit `modelsChanged()` properly
- Auth state emits `availabilityChanged()` properly
- `sendRequest()` respects `AgentRequest.conversationHistory`
- `sendRequest()` respects provided tool definitions

## 15. Future extension points

როგორც platform იზრდება, ახალი plugin ავტორებისთვის სასურველია დაემატოს:

- `IAgentSettingsPageProvider`
- `IProviderAuthIntegration`
- `IChatSessionImporterPlugin`
- `IProviderTelemetryContributor`

მაგრამ საბაზისო provider integration ამ გიდის contract-ით უკვე შესაძლებელი უნდა იყოს.

## 16. Bottom line

თუ third-party developer შეძლებს მხოლოდ `IAgentProvider`-ის დაწერით Exorcist-ში ახალი მოდელის პლაგინის დამატებას, tool/runtime/UI duplication-ის გარეშე, მაშინ architecture სწორად არის აწყობილი.