if(NOT DEFINED BUNDLE_PATH)
    message(FATAL_ERROR "BUNDLE_PATH is required")
endif()

file(GLOB BUNDLED_LIBRARIES LIST_DIRECTORIES false "${BUNDLE_PATH}/Contents/libs/*.dylib")
foreach(BUNDLED_LIBRARY IN LISTS BUNDLED_LIBRARIES)
    execute_process(
        COMMAND /usr/bin/codesign --force --sign - "${BUNDLED_LIBRARY}"
        COMMAND_ERROR_IS_FATAL ANY
    )
endforeach()

execute_process(
    COMMAND /usr/bin/codesign --force --sign - "${BUNDLE_PATH}/Contents/MacOS/magicseteditor"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND /usr/bin/codesign --force --sign - "${BUNDLE_PATH}"
    COMMAND_ERROR_IS_FATAL ANY
)