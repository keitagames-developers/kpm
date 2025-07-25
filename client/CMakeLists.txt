cmake_minimum_required(VERSION 3.10)
project(kpm)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
# CMakeLists.txt の末尾に追記


include_directories(${LIBARCHIVE_INCLUDE_DIRS})
link_directories(${LIBARCHIVE_LIBRARY_DIRS})

add_executable(kpm src/main.cpp)
target_link_libraries(kpm
  PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    ${LIBARCHIVE_LIBRARIES}
)
install(TARGETS kpm
        RUNTIME DESTINATION bin)    # デフォルト /usr/local/bin へ配置
