set(ENGINE_SOURCES
    main.cpp
    engine.cpp
    renderer/renderer.cpp
    renderer/pipelinemanager.cpp
    window.cpp
    shader.cpp
    model_loader.cpp
    commandpool.cpp
    buffer.cpp
    texture.cpp
    queue.cpp
    3rdparty/imgui/imgui.cpp
    3rdparty/imgui/imgui_draw.cpp
    3rdparty/imgui/imgui_tables.cpp
    3rdparty/imgui/imgui_widgets.cpp
    3rdparty/imgui/imgui_impl_glfw.cpp
    3rdparty/imgui/imgui_impl_vulkan.cpp
)
set(ENGINE_LIBS
    glfw3dll
    $<IF:$<CONFIG:Release>, fmt, fmtd>
    $<IF:$<CONFIG:Release>, VulkanMemoryAllocator, VulkanMemoryAllocatord>
    $<IF:$<CONFIG:Release>, spirv-cross-core, spirv-cross-cored>
    vulkan-1
    spdlog
    assimp
)
set(ENGINE_SHARED_LIBS
    glfw3.dll
    spdlog.dll
    assimp.dll
)
set(ENGINE_ASSETS
    src/assets
)

add_executable(engine ${ENGINE_SOURCES})

target_include_directories(engine PRIVATE 
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty"
)
target_link_directories(engine PRIVATE "${CMAKE_SOURCE_DIR}/lib")
target_link_libraries(engine PRIVATE "${ENGINE_LIBS}")
target_compile_features(engine PRIVATE cxx_std_20)
target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic -Werror $<IF:$<CONFIG:Relase>, -O3, -O0>)
target_compile_definitions(engine PRIVATE GLFW_INCLUDE_VULKAN VK_VERSION_1_3)
target_precompile_headers(engine 
    PRIVATE 
        <GLFW/glfw3.h>
        <vulkan/vulkan.hpp>
        <spirv_cross/spirv_cross.hpp>
        # <GLFW/glfw3native.h>         # ERROR WHEN ENABLED WITH IMGUI
        # <imgui/imgui.h>              # ERROR WHEN ENABLED 
        # <imgui/imgui_impl_glfw.h>    # ERROR WHEN ENABLED         
        # <imgui/imgui_impl_vulkan.h>  # ERROR WHEN ENABLED             
)

foreach(lib IN LISTS ENGINE_SHARED_LIBS)
    add_custom_command(TARGET engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/shared/${lib}"
            "$<TARGET_FILE_DIR:engine>") 
endforeach()

file(GLOB_RECURSE ENGINE_ASSETS_SHADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${ENGINE_ASSETS}/shaders/*.*")
set(ENGINE_ASSETS_COMPILED_SHADERS "")
foreach(sh IN LISTS ENGINE_ASSETS_SHADERS)
    get_filename_component(sh_filename ${sh} NAME)
    list(APPEND ENGINE_ASSETS_COMPILED_SHADERS "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/${sh_filename}.spv")
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/${sh_filename}.spv"
        DEPENDS "${sh}"
        COMMAND glslc -c "${sh}" -o "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/${sh_filename}.spv"
        VERBATIM
    )
endforeach()

add_custom_target(COMPILE_SHADERS
    SOURCES ${ENGINE_ASSETS_SHADERS}
    DEPENDS ${ENGINE_ASSETS_COMPILED_SHADERS}
)