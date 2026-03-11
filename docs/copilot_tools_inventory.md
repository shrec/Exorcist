# GitHub Copilot (VS Code) — სრული ინსტრუმენტების ინვენტარი

> გენერირების თარიღი: 2026-03-10
> მოდელი: Claude Opus 4.6
> გარემო: VS Code + GitHub Copilot Chat Extension
> სულ: **113 ინსტრუმენტი + 5 სუბაგენტი**

---

## 1. ფაილური სისტემა (8)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 1 | `create_file` | ახალი ფაილის შექმნა მითითებული შიგთავსით. ავტომატურად ქმნის საჭირო დირექტორიებს. |
| 2 | `create_directory` | დირექტორიის შექმნა (mkdir -p ანალოგი, რეკურსიული). |
| 3 | `read_file` | ფაილის წაკითხვა ხაზების დიაპაზონით (1-indexed). |
| 4 | `replace_string_in_file` | ტექსტის ჩანაცვლება არსებულ ფაილში (ერთი ჩანაცვლება). |
| 5 | `multi_replace_string_in_file` | მრავალი ჩანაცვლება ერთ ზარში (ერთ ან რამდენიმე ფაილზე). |
| 6 | `file_search` | ფაილების ძიება glob პატერნით (მაგ: `**/*.cpp`). |
| 7 | `list_dir` | დირექტორიის შიგთავსის ჩამოთვლა (ფოლდერები `/`-ით მთავრდება). |
| 8 | `get_changed_files` | Git-ის შეცვლილი ფაილების diffs (staged, unstaged, merge-conflicts). |

---

## 2. ძიება (4)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 9 | `grep_search` | სწრაფი ტექსტური/regex ძიება workspace-ში. includePattern, includeIgnoredFiles. |
| 10 | `semantic_search` | სემანტიკური ძიება (natural language) კოდბეისში. |
| 11 | `search_subagent` | ძიების სუბაგენტი — ორკესტრირებს semantic_search, grep_search, file_search, read_file. |
| 12 | `get_search_view_results` | VS Code-ის Search View-ის შედეგების მიღება. |

---

## 3. ტერმინალი (5)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 13 | `run_in_terminal` | PowerShell ბრძანების გაშვება ტერმინალში (persistent session). isBackground ბექგრაუნდისთვის. |
| 14 | `get_terminal_output` | ბექგრაუნდ ტერმინალის output-ის მიღება (ID-ით). |
| 15 | `await_terminal` | ბექგრაუნდ ბრძანების დასრულების მოლოდინი (timeout-ით). |
| 16 | `kill_terminal` | ტერმინალის გაჩერება ID-ით. |
| 17 | `terminal_last_command` | აქტიური ტერმინალის ბოლო ბრძანების მიღება. |

---

## 4. კოდის ანალიზი და რეფაქტორინგი (5)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 18 | `get_errors` | კომპილაციის/lint შეცდომები (ფაილზე ან მთლიან workspace-ზე). |
| 19 | `vscode_listCodeUsages` | სიმბოლოს ყველა გამოყენების პოვნა (references, definitions, implementations). |
| 20 | `vscode_renameSymbol` | სიმბოლოს სემანტიკური გადარქმევა (Language Server-ით, ყველა reference-ს ცვლის). |
| 21 | `terminal_selection` | აქტიური ტერმინალის მონიშნული ტექსტი. |
| 22 | `test_failure` | ტესტის წარუმატებლობის ინფორმაცია. |

---

## 5. სუბაგენტები და ორკესტრაცია (3)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 23 | `runSubagent` | სუბაგენტის გაშვება — complex, multi-step ამოცანების ავტონომიური შესრულება. |
| 24 | `manage_todo_list` | TODO სიის მართვა (not-started / in-progress / completed). |
| 25 | `tool_search_tool_regex` | დეფერული ინსტრუმენტების ძიება regex-ით (სახელი, აღწერა, პარამეტრები). |

---

## 6. მეხსიერება (1)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 26 | `memory` | მუდმივი მეხსიერების სისტემა — 3 scope: user (`/memories/`), session (`/memories/session/`), repo (`/memories/repo/`). ბრძანებები: view, create, str_replace, insert, delete, rename. |

---

## 7. VS Code ინტეგრაცია (6)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 27 | `run_vscode_command` | VS Code ბრძანების გაშვება (commandId + args). |
| 28 | `get_vscode_api` | VS Code API დოკუმენტაცია extension development-ისთვის. |
| 29 | `install_extension` | VS Code ექსტენშენის ინსტალაცია (publisher.extension ფორმატი). |
| 30 | `vscode_searchExtensions_internal` | Extensions Marketplace ძიება (კატეგორია, keywords, IDs). |
| 31 | `vscode_askQuestions` | მომხმარებლისთვის clarifying კითხვების დასმა (options, multiSelect, freeform). |
| 32 | `create_and_run_task` | tasks.json შექმნა/დამატება და task-ის გაშვება. |

---

## 8. Workspace და პროექტი (3)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 33 | `create_new_workspace` | ახალი პროექტის scaffolding (TypeScript, React, Node.js, Next.js, Vite...). |
| 34 | `get_project_setup_info` | პროექტის setup ინფორმაცია (python-script, mcp-server, vscode-extension...). |
| 35 | `github_repo` | GitHub რეპოზიტორიში კოდის ძიება (owner/repo ფორმატი). |

---

## 9. ვებ / ბრაუზერი (11)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 36 | `fetch_webpage` | ვებ-გვერდის შიგთავსის წამოღება და ანალიზი. |
| 37 | `open_browser_page` | ახალი ბრაუზერის გვერდის გახსნა URL-ით (pageId ბრუნდება). |
| 38 | `read_page` | ბრაუზერის გვერდის accessibility snapshot (ელემენტების ref-ები). |
| 39 | `screenshot_page` | ბრაუზერის გვერდის სკრინშოტი (fullPage, selector, ref). |
| 40 | `navigate_page` | ნავიგაცია: URL, back, forward, reload. |
| 41 | `click_element` | ელემენტზე კლიკი (left/right/middle, dblClick). |
| 42 | `hover_element` | ელემენტზე hover. |
| 43 | `drag_element` | ელემენტის drag & drop. |
| 44 | `type_in_page` | ტექსტის აკრეფა / კლავიშის დაჭერა ბრაუზერში. |
| 45 | `handle_dialog` | მოდალური დიალოგის (alert, confirm, prompt) მართვა. |
| 46 | `run_playwright_code` | Playwright კოდის გაშვება (page ობიექტით). |

---

## 10. Notebook / Jupyter (8)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 47 | `create_new_jupyter_notebook` | ახალი Jupyter Notebook-ის შექმნა. |
| 48 | `copilot_getNotebookSummary` | Notebook-ის შეჯამება (cells, types, outputs, execution info). |
| 49 | `run_notebook_cell` | Notebook cell-ის გაშვება (cellId-ით). |
| 50 | `read_notebook_cell_output` | Cell-ის output-ის წაკითხვა (ბოლო execution ან disk-იდან). |
| 51 | `edit_notebook_file` | Notebook ფაილის რედაქტირება (insert, edit, delete cell). |
| 52 | `restart_notebook_kernel` | Notebook kernel-ის რესტარტი. |
| 53 | `configure_python_notebook` | Python Notebook kernel-ის კონფიგურაცია და გაშვება. |
| 54 | `configure_non_python_notebook` | არა-Python Notebook kernel-ის კონფიგურაცია. |

---

## 11. Python გარემო (5)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 55 | `configure_python_environment` | Python გარემოს კონფიგურაცია (ყველა Python ოპერაციამდე საჭიროა). |
| 56 | `get_python_environment_details` | Python გარემოს დეტალები (ტიპი, ვერსია, პაკეტები). |
| 57 | `get_python_executable_details` | Python executable-ის path და გაშვების ბრძანება. |
| 58 | `install_python_packages` | Python პაკეტების ინსტალაცია. |
| 59 | `renderMermaidDiagram` | Mermaid.js დიაგრამის რენდერი. |

---

## 12. SonarQube (4)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 60 | `sonarqube_analyze_file` | ფაილის SonarQube ანალიზი (code quality + security). |
| 61 | `sonarqube_list_potential_security_issues` | პოტენციური უსაფრთხოების პრობლემების ჩამოთვლა (Security Hotspots, Taint Vulnerabilities). |
| 62 | `sonarqube_exclude_from_analysis` | ფაილების/ფოლდერების გამორიცხვა SonarQube ანალიზიდან (glob pattern). |
| 63 | `sonarqube_setup_connected_mode` | SonarQube Connected Mode-ის დაყენება (Server/Cloud). |

---

## 13. Docker Container Management — MCP (12)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 64 | `mcp_copilot_conta_list_containers` | კონტეინერების ჩამოთვლა (გაჩერებულების ჩათვლით). |
| 65 | `mcp_copilot_conta_list_images` | კონტეინერ იმიჯების ჩამოთვლა. |
| 66 | `mcp_copilot_conta_list_networks` | Docker ქსელების ჩამოთვლა. |
| 67 | `mcp_copilot_conta_list_volumes` | Docker volume-ების ჩამოთვლა. |
| 68 | `mcp_copilot_conta_act_container` | კონტეინერის start/stop/restart/remove. |
| 69 | `mcp_copilot_conta_act_image` | იმიჯის pull/remove. |
| 70 | `mcp_copilot_conta_inspect_container` | კონტეინერის ინსპექტირება. |
| 71 | `mcp_copilot_conta_inspect_image` | იმიჯის ინსპექტირება. |
| 72 | `mcp_copilot_conta_logs_for_container` | კონტეინერის ლოგების ნახვა. |
| 73 | `mcp_copilot_conta_run_container` | ახალი კონტეინერის გაშვება (image, ports, mounts, env). |
| 74 | `mcp_copilot_conta_tag_image` | იმიჯის tag-ირება. |
| 75 | `mcp_copilot_conta_prune` | გამოუყენებელი რესურსების გასუფთავება (containers/images/volumes/networks/all). |

---

## 14. Docker Browser — MCP (20)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 76 | `mcp_mcp_docker_browser_navigate` | URL-ზე ნავიგაცია. |
| 77 | `mcp_mcp_docker_browser_navigate_back` | უკან დაბრუნება. |
| 78 | `mcp_mcp_docker_browser_snapshot` | Accessibility snapshot (უკეთესია screenshot-ზე). |
| 79 | `mcp_mcp_docker_browser_take_screenshot` | სკრინშოტი (png/jpeg, fullPage, element). |
| 80 | `mcp_mcp_docker_browser_click` | ელემენტზე კლიკი (ref-ით). |
| 81 | `mcp_mcp_docker_browser_hover` | Hover. |
| 82 | `mcp_mcp_docker_browser_type` | ტექსტის აკრეფა. |
| 83 | `mcp_mcp_docker_browser_fill_form` | ფორმის შევსება (textbox, checkbox, radio, combobox, slider). |
| 84 | `mcp_mcp_docker_browser_select_option` | Dropdown-ში ოფციის არჩევა. |
| 85 | `mcp_mcp_docker_browser_press_key` | კლავიშის დაჭერა. |
| 86 | `mcp_mcp_docker_browser_drag` | Drag & drop. |
| 87 | `mcp_mcp_docker_browser_file_upload` | ფაილების ატვირთვა. |
| 88 | `mcp_mcp_docker_browser_handle_dialog` | დიალოგის მართვა (accept/dismiss). |
| 89 | `mcp_mcp_docker_browser_evaluate` | JavaScript-ის შესრულება გვერდზე. |
| 90 | `mcp_mcp_docker_browser_run_code` | Playwright კოდის გაშვება. |
| 91 | `mcp_mcp_docker_browser_console_messages` | Console მესიჯების მიღება (error/warning/info/debug). |
| 92 | `mcp_mcp_docker_browser_network_requests` | ქსელის მოთხოვნების ჩამოთვლა. |
| 93 | `mcp_mcp_docker_browser_tabs` | ტაბების მართვა (list, new, close, select). |
| 94 | `mcp_mcp_docker_browser_resize` | ფანჯრის ზომის ცვლილება. |
| 95 | `mcp_mcp_docker_browser_wait_for` | ტექსტის/დროის მოლოდინი. |
| 96 | `mcp_mcp_docker_browser_close` | გვერდის დახურვა. |
| 97 | `mcp_mcp_docker_browser_install` | ბრაუზერის ინსტალაცია. |

---

## 15. Docker MCP Server Management (5)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 98 | `mcp_mcp_docker_mcp-find` | MCP სერვერის ძიება კატალოგში (სახელით, აღწერით). |
| 99 | `mcp_mcp_docker_mcp-add` | MCP სერვერის დამატება სესიაში. |
| 100 | `mcp_mcp_docker_mcp-remove` | MCP სერვერის წაშლა/გამორთვა. |
| 101 | `mcp_mcp_docker_mcp-config-set` | MCP სერვერის კონფიგურაცია (schema validation-ით). |
| 102 | `mcp_mcp_docker_mcp-exec` | MCP tool-ის პირდაპირი შესრულება. |
| 103 | `mcp_mcp_docker_code-mode` | JavaScript code-mode tool-ის შექმნა (მრავალი MCP სერვერის კომბინაცია). |

---

## 16. Pylance MCP (12)

| # | ინსტრუმენტი | აღწერა |
|---|------------|--------|
| 104 | `mcp_pylance_mcp_s_pylanceDocuments` | Pylance დოკუმენტაციის ძიება (კონფიგურაცია, ფიჩერები, troubleshooting). |
| 105 | `mcp_pylance_mcp_s_pylanceSettings` | Python analysis-ის მიმდინარე settings. |
| 106 | `mcp_pylance_mcp_s_pylancePythonEnvironments` | Python გარემოების ჩამოთვლა (აქტიური + ყველა ხელმისაწვდომი). |
| 107 | `mcp_pylance_mcp_s_pylanceUpdatePythonEnvironment` | Python გარემოს გადართვა. |
| 108 | `mcp_pylance_mcp_s_pylanceWorkspaceRoots` | Workspace root-ების მიღება. |
| 109 | `mcp_pylance_mcp_s_pylanceWorkspaceUserFiles` | User Python ფაილების სია (library ფაილების გარეშე). |
| 110 | `mcp_pylance_mcp_s_pylanceImports` | Workspace-ის ყველა import-ის ანალიზი (resolved/unresolved). |
| 111 | `mcp_pylance_mcp_s_pylanceInstalledTopLevelModules` | დაინსტალირებული top-level მოდულები. |
| 112 | `mcp_pylance_mcp_s_pylanceSyntaxErrors` | Python კოდის syntax error-ების შემოწმება (ფაილად შენახვის გარეშე). |
| 113 | `mcp_pylance_mcp_s_pylanceFileSyntaxErrors` | Python ფაილის syntax error-ების შემოწმება. |
| 114 | `mcp_pylance_mcp_s_pylanceInvokeRefactoring` | ავტომატური რეფაქტორინგი (unusedImports, convertImportFormat, addTypeAnnotation...). |
| 115 | `mcp_pylance_mcp_s_pylanceRunCodeSnippet` | Python კოდის snippet-ის გაშვება (სწორი interpreter-ით, shell escaping-ის გარეშე). |

---

## 17. სუბაგენტები (5)

| # | სახელი | აღწერა |
|---|--------|--------|
| A | `architect` | სისტემის არქიტექტურის დიზაინი, კომპონენტების სტრუქტურის შეფასება, ინტერფეისების საზღვრების რევიუ. Qt 6, plugin არქიტექტურა, service registry, layered design. |
| B | `developer` | ფიჩერების იმპლემენტაცია, ბაგ ფიქსი, რეფაქტორინგი. C++17, Qt 6, CMake, plugin system კოდი. |
| C | `plugin-dev` | პლაგინების შექმნა, plugin API-ს გაფართოება, plugin loading-ის დებაგი, QPluginLoader, ServiceRegistry. |
| D | `reviewer` | კოდის რევიუ — ხარისხი, უსაფრთხოება, პერფორმანსი, კონვენციების დაცვა. |
| E | `Explore` | სწრაფი read-only კოდბეისის გამოკვლევა და Q&A. ძიება + ფაილების წაკითხვა. |

---

## შეჯამება კატეგორიებად

| კატეგორია | რაოდენობა |
|-----------|-----------|
| ფაილური სისტემა | 8 |
| ძიება | 4 |
| ტერმინალი | 5 |
| კოდის ანალიზი | 5 |
| სუბაგენტები / ორკესტრაცია | 3 |
| მეხსიერება | 1 |
| VS Code ინტეგრაცია | 6 |
| Workspace / პროექტი | 3 |
| ვებ / ბრაუზერი | 11 |
| Notebook / Jupyter | 8 |
| Python გარემო | 5 |
| SonarQube | 4 |
| Docker Container MCP | 12 |
| Docker Browser MCP | 22 |
| Docker MCP Server Mgmt | 6 |
| Pylance MCP | 12 |
| **სულ ინსტრუმენტები** | **115** |
| სუბაგენტები | 5 |
| **სულ** | **120** |
