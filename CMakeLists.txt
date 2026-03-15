cmake_minimum_required(VERSION 3.20)
project(AdvancedCGFirstProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# IMPORTANT:
# Don't glob src/*.cpp (avoids accidental multiple mains later).
set(LIB_FILES
  "${CMAKE_SOURCE_DIR}/src/Libs/Shader.cpp"
  "${CMAKE_SOURCE_DIR}/src/Libs/Window.cpp"
)

add_executable(AdvancedCGStarter
  ${LIB_FILES}
  "${CMAKE_SOURCE_DIR}/src/main.cpp"
)

target_include_directories(AdvancedCGStarter PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/Libs
)

target_link_libraries(AdvancedCGStarter PRIVATE
  OpenGL::GL
  glfw
  GLEW::GLEW
  glm::glm
)

if(APPLE)
  target_compile_definitions(AdvancedCGStarter PRIVATE GL_SILENCE_DEPRECATION)
  target_link_libraries(AdvancedCGStarter PRIVATE
    "-framework Cocoa" "-framework IOKit" "-framework CoreVideo"
  )
endif()

# Copy shaders next to the binary (Lab 1 uses Shaders/Lab1/*)
add_custom_command(TARGET AdvancedCGStarter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/Shaders
          $<TARGET_FILE_DIR:AdvancedCGStarter>/Shaders
)
