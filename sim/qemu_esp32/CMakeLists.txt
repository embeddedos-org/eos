# EoS ESP32 image through CMake. Flags, sources, linker script and image
# parameters are read from sim.yaml, so this and build.sh cannot drift apart.
#   cmake -S sim/qemu_esp32 -B build/esp32 -G Ninja \
#       -DCMAKE_TOOLCHAIN_FILE=$PWD/toolchains/xtensa-esp32.cmake \
#       -DEOS_ESP32_APP_SOURCES="sim/qemu_esp32/scheduler_test.c"
#   cmake --build build/esp32      # esp32_image.elf, .bin, .flash.bin
cmake_minimum_required(VERSION 3.16)
project(EoS_ESP32 C ASM)

set(EOS_ESP32_APP_SOURCES "" CACHE STRING "Application sources (; separated)")
set(EOS_ESP32_EXTRA_CFLAGS "" CACHE STRING "Extra compiler flags (; separated)")
set(EOS_ESP32_ESPTOOL esptool CACHE STRING "esptool command")
if(NOT EOS_ESP32_APP_SOURCES)
    message(FATAL_ERROR "set -DEOS_ESP32_APP_SOURCES=<app.c;...>")
endif()

get_filename_component(root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(yaml "${CMAKE_CURRENT_SOURCE_DIR}/sim.yaml")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${yaml}")

# sim.yaml value at KEY (list: ';'-joined, scalar: as is), or its image.KEY.
function(yaml_get out key)
    execute_process(COMMAND python3 -c
        "import sys,yaml;v=yaml.safe_load(open(sys.argv[1]))
for k in sys.argv[2].split('.'): v=v[k]
print(';'.join(map(str,v)) if isinstance(v,list) else v)"
        "${yaml}" "${key}" OUTPUT_VARIABLE v RESULT_VARIABLE rc OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "cannot read '${key}' from ${yaml} (python3 + PyYAML needed)")
    endif()
    set(${out} "${v}" PARENT_SCOPE)
endfunction()

yaml_get(cflags cflags)
yaml_get(incs includes)
yaml_get(ldflags ldflags)
yaml_get(libs libs)
yaml_get(ld linker_script)
yaml_get(srcs sources)
foreach(k chip flash_mode flash_freq flash_size offset)
    yaml_get(img_${k} image.${k})
endforeach()

set(files "")
foreach(s ${srcs})
    list(APPEND files "${root}/${s}")
endforeach()
foreach(a ${EOS_ESP32_APP_SOURCES})
    get_filename_component(a "${a}" ABSOLUTE BASE_DIR "${root}")
    list(APPEND files "${a}")
endforeach()

add_executable(esp32_image ${files})
set_target_properties(esp32_image PROPERTIES SUFFIX .elf)
foreach(d ${incs})
    target_include_directories(esp32_image PRIVATE "${root}/${d}")
endforeach()
target_compile_options(esp32_image PRIVATE ${cflags} ${EOS_ESP32_EXTRA_CFLAGS})
target_link_options(esp32_image PRIVATE ${ldflags} -T "${root}/${ld}" -Wl,-Map,esp32_image.map)
target_link_libraries(esp32_image PRIVATE ${libs})
set_target_properties(esp32_image PROPERTIES LINK_DEPENDS "${root}/${ld}")

separate_arguments(esptool UNIX_COMMAND "${EOS_ESP32_ESPTOOL}")
set(bin "${CMAKE_BINARY_DIR}/esp32_image.bin")
set(flash "${CMAKE_BINARY_DIR}/esp32_image.flash.bin")
# OUTPUT rather than POST_BUILD so the images are dependency-tracked. A
# POST_BUILD hook runs only when the target itself relinks, so deleting an
# image, or changing an image setting or pad_flash.py while the ELF stayed
# up to date, left the .bin and .flash.bin missing or stale while the build
# reported success. Declaring them as outputs makes CMake rebuild them
# whenever the ELF, this file or the padding script changes.
add_custom_command(
    OUTPUT "${bin}" "${flash}"
    COMMAND ${esptool} --chip ${img_chip} elf2image --flash_mode ${img_flash_mode}
            --flash_freq ${img_flash_freq} --flash_size ${img_flash_size}
            -o "${bin}" "$<TARGET_FILE:esp32_image>"
    # Whole-flash image for QEMU: erased flash with the app at its offset,
    # the same bytes build.sh writes.
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/pad_flash.py"
        "${bin}" "${flash}" "${img_offset}" "${img_flash_size}"
    DEPENDS esp32_image
            "$<TARGET_FILE:esp32_image>"
            "${CMAKE_CURRENT_SOURCE_DIR}/pad_flash.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/sim.yaml"
            "${CMAKE_CURRENT_LIST_FILE}"
    COMMENT "Creating ESP32 flash images"
    VERBATIM)

# In ALL so `cmake --build` still produces the images without naming a target.
add_custom_target(esp32_images ALL DEPENDS "${bin}" "${flash}")
