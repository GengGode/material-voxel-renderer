add_executable(material-voxel-renderer.app)

if (MSVC)
    target_compile_options(material-voxel-renderer.app
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++23preview>
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(material-voxel-renderer.app
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Wall>
            $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
            $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++2b>
            $<$<COMPILE_LANGUAGE:CXX>:-finput-charset=UTF-8>
            $<$<COMPILE_LANGUAGE:CXX>:-fexec-charset=UTF-8>
    )
endif()


target_include_directories(material-voxel-renderer.app
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(material-voxel-renderer.app
    PRIVATE
        app.cpp
)




target_link_libraries(material-voxel-renderer.app
    PRIVATE
        material-voxel-renderer.static
)

find_package(fmt REQUIRED)
target_link_libraries(material-voxel-renderer.app
    PRIVATE
        fmt::fmt
)

find_package(global_utils CONFIG REQUIRED)
target_link_libraries(material-voxel-renderer.app
    PRIVATE
        global_utils::global_utils
)