add_library(material-voxel-renderer.static STATIC)

if (MSVC)
    target_compile_options(material-voxel-renderer.static
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++23preview>
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(material-voxel-renderer.static
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Wall>
            $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
            $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++2b>
            $<$<COMPILE_LANGUAGE:CXX>:-finput-charset=UTF-8>
            $<$<COMPILE_LANGUAGE:CXX>:-fexec-charset=UTF-8>
    )
endif()

target_include_directories(material-voxel-renderer.static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(material-voxel-renderer.static
    PUBLIC
        Executor.cpp
        ImRenderer.cpp
        OpenglRenderer.cpp
        OpenglImRenderer.cpp
        OpenglRasterizationFramer.cpp
        OpenglComputeShaderFramer.cpp
)

target_link_libraries(material-voxel-renderer.static
    PUBLIC
        material-voxel-renderer.interface
)

find_package(global_utils CONFIG REQUIRED)
target_link_libraries(material-voxel-renderer.static
    PUBLIC
        global_utils::global_utils
)

find_package(cereal CONFIG REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(material-voxel-renderer.static
    PUBLIC
        cereal::cereal
        fmt::fmt
        spdlog::spdlog
)

find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
target_link_libraries(material-voxel-renderer.static
    PUBLIC
        glfw
        glm::glm
        glad::glad
        imgui::imgui
        implot::implot
)

if(WIN32)
    target_link_libraries(material-voxel-renderer.static
        PRIVATE
            opengl32    
    )
endif()
