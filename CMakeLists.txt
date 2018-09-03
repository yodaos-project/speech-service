CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(speech-service)
set(VERSION 0.1)

option(BUILD_DEBUG "debug or release" OFF)
option(BUILD_DEMO "build demo or not" OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  set(COMPILER_IS_CLANG true)
else()
	set(COMPILER_IS_CLANG false)
endif()

if (BUILD_DEBUG)
	set (common_cxxflags "-g -O0")
else()
	set (common_cxxflags -O3)
	set (common_ldflags -s)
endif()
if (NOT ${COMPILER_IS_CLANG})
	set (common_cxxflags "${common_cxxflags} -pthread")
endif()
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${common_cxxflags}")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${common_ldflags}")

include(${CUSTOM_CMAKE_MODULES}/common.mk)

git_commit_id(commit_id)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGIT_COMMIT_ID=${commit_id}")

findPackage(mutils REQUIRED
	HINTS ${mutilsPrefix}
	HEADERS rlog.h caps.h clargs.h
	INC_PATH_SUFFIX include/log include/caps include/misc
	SHARED_LIBS rlog caps
	STATIC_LIBS misc
)

findPackage(flora REQUIRED
	HINTS ${floraPrefix}
	HEADERS flora-cli.h
	SHARED_LIBS flora-cli
)

findPackage(speech REQUIRED
	HINTS ${speechPrefix}
	HEADERS speech.h
	INC_PATH_SUFFIX include/speech
	SHARED_LIBS speech
	RPATH_LINK
)

file(GLOB ss_src_files
	src/*.cpp
	src/*.h
)

add_executable(speech-service ${ss_src_files})
target_include_directories(speech-service PRIVATE
	src
	${mutils_INCLUDE_DIRS}
	${flora_INCLUDE_DIRS}
	${speech_INCLUDE_DIRS}
)
target_link_libraries(speech-service
	${mutils_LIBRARIES}
	${flora_LIBRARIES}
	${speech_LIBRARIES}
)

install(TARGETS speech-service
	RUNTIME DESTINATION bin
)

if(BUILD_DEMO)
file(GLOB demo_src_files
	demo/*.cpp
	demo/*.h
)
add_executable(demo ${demo_src_files})
target_include_directories(demo PRIVATE
	${mutils_INCLUDE_DIRS}
	${flora_INCLUDE_DIRS}
	demo
)
target_link_libraries(demo
	${mutils_LIBRARIES}
	${flora_LIBRARIES}
)
endif(BUILD_DEMO)
