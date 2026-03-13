find_package(Qt6 REQUIRED COMPONENTS Network)

add_subdirectory(common)
add_subdirectory(copilot)
add_subdirectory(github)
add_subdirectory(remote)
add_subdirectory(build)
# add_subdirectory(claude)   # disabled — Copilot-first
# add_subdirectory(codex)    # disabled — Copilot-first
# add_subdirectory(ollama)   # disabled — Copilot-first
# add_subdirectory(byok)     # disabled — Copilot-first
