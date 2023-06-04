cmake_minimum_required(VERSION 3.26)

project(dingoo-emu)

set(targetName dingoo-emu)

set(targetName_src
src/main.cpp
src/dingoo.cpp
src/app.cpp
src/bridge.cpp
src/utils.cpp
src/control.cpp
src/virtualmemory.cpp
src/virtualfilesys.cpp
src/framebuffer.cpp
src/virtualthread.cpp
src/sound.cpp
src/mixer.cpp
src/mysprintf.cpp
)

macro(source_group_by_dir source_files)
	set(sgbd_cur_dir ${CMAKE_CURRENT_SOURCE_DIR})
	foreach(sgbd_file ${${source_files}})
		string(REGEX REPLACE ${sgbd_cur_dir}/\(.*\) \\1 sgbd_fpath ${sgbd_file})
		string(REGEX REPLACE "\(.*\)/.*" \\1 sgbd_group_name ${sgbd_fpath})
		string(COMPARE EQUAL ${sgbd_fpath} ${sgbd_group_name} sgbd_nogroup)
		string(REPLACE "/" "\\" sgbd_group_name ${sgbd_group_name})
		if(sgbd_nogroup)
			set(sgbd_group_name "\\")
		endif(sgbd_nogroup)
		source_group(${sgbd_group_name} FILES ${sgbd_file})
	endforeach(sgbd_file)
endmacro(source_group_by_dir)

source_group_by_dir(targetName_src)

link_directories(
	../3rdparty/unicorn/lib
	../3rdparty/capstone/lib
	../3rdparty/SDL2/lib
	../3rdparty/pthreads-w32-2-9-1-release/Pre-built.2/lib/x64
	../3rdparty/SDL2_mixer-2.6.3/lib/x64/
)

include_directories(
	./include
	../3rdparty/unicorn/include
	../3rdparty/capstone/include
	../3rdparty/SDL2/include
	../3rdparty/SDL2/include/SDL2
	../3rdparty/SDL2_mixer-2.6.3/include
	../3rdparty/pthreads-w32-2-9-1-release/Pre-built.2/include
)

add_executable(${targetName} ${targetName_src})
target_link_libraries(${targetName}
unicorn-import.lib
capstone.lib
SDL2.lib
SDL2main.lib
pthreadVC2.lib
SDL2_mixer.lib
)
