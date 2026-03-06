add_executable(exorcist
    main.cpp
)

target_link_libraries(exorcist PRIVATE Qt6::Widgets)

if(EXORCIST_USE_LLVM)
    target_include_directories(exorcist PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(exorcist PRIVATE ${LLVM_DEFINITIONS})
endif()
