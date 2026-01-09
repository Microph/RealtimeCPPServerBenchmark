cmake_minimum_required(VERSION 3.19)

include_guard(GLOBAL)

# Call this function before first project() call to aquire vcpkg
function(bootstrap_vcpkg)
    # Check required modules
    if(UNIX)
        find_package(PkgConfig REQUIRED)
        find_program(CURL_EXECUTABLE curl REQUIRED)
        find_program(ZIP_EXECUTABLE zip REQUIRED)
        find_program(UNZIP_EXECUTABLE unzip REQUIRED)
        find_program(TAR_EXECUTABLE tar REQUIRED)
    endif()
    find_package(Git REQUIRED)

    set(X_VCPKG_GIT_REPOSITORY_URL "https://github.com/Microsoft/vcpkg.git")
    set(X_VCPKG_CLONE_DIR "${CMAKE_SOURCE_DIR}/vcpkg")
    set(X_VCPKG_PATCHES "")

    if(NOT EXISTS "${X_VCPKG_CLONE_DIR}/.git")
        message(STATUS "Cloning vcpkg into ${X_VCPKG_CLONE_DIR}")
        
        # Remove incomplete directory if it exists
        if(EXISTS "${X_VCPKG_CLONE_DIR}")
            message(STATUS "Removing incomplete vcpkg directory")
            file(REMOVE_RECURSE "${X_VCPKG_CLONE_DIR}")
        endif()
        
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" clone "${X_VCPKG_GIT_REPOSITORY_URL}" "${X_VCPKG_CLONE_DIR}"
            RESULT_VARIABLE _vcpkg_git_clone_result
            ERROR_VARIABLE _vcpkg_git_clone_error
        )
        if(NOT _vcpkg_git_clone_result EQUAL 0)
            message(FATAL_ERROR "Could not clone vcpkg repository from ${X_VCPKG_GIT_REPOSITORY_URL}\nError: ${_vcpkg_git_clone_error}\nMake sure you have access rights and the URL is valid.")
        endif()
        
        if(NOT EXISTS "${X_VCPKG_CLONE_DIR}/.git")
            message(FATAL_ERROR "vcpkg clone directory does not contain .git folder after cloning. Clone may have failed.")
        endif()
        
        message(STATUS "Cloning vcpkg into ${X_VCPKG_CLONE_DIR} - done")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${X_VCPKG_CLONE_DIR}"
        OUTPUT_VARIABLE _vcpkg_current_head_sha
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rev_parse_result
    )
    
    if(NOT _rev_parse_result EQUAL 0)
        message(FATAL_ERROR "Failed to get vcpkg HEAD revision from ${X_VCPKG_CLONE_DIR}. Make sure vcpkg was cloned successfully.")
    endif()
    set(_baseline_sha "${_vcpkg_current_head_sha}")

    file(READ "${CMAKE_SOURCE_DIR}/vcpkg-configuration.json" _vcpkg_configuration_json_contents)
    string(JSON _default_registry_kind ERROR_VARIABLE _get_baseline_err GET "${_vcpkg_configuration_json_contents}" "default-registry" "kind")
    if(_default_registry_kind STREQUAL "builtin")
        string(JSON _maybe_baseline_sha ERROR_VARIABLE _get_baseline_err GET "${_vcpkg_configuration_json_contents}" "default-registry" "baseline")
    elseif(_default_registry_kind STREQUAL "git")
        string(JSON _git_registry_repo_url ERROR_VARIABLE _get_baseline_err GET "${_vcpkg_configuration_json_contents}" "default-registry" "repository")
        if(_git_registry_repo_url STREQUAL "${X_VCPKG_GIT_REPOSITORY_URL}")
            string(JSON _maybe_baseline_sha ERROR_VARIABLE _get_baseline_err GET "${_vcpkg_configuration_json_contents}" "default-registry" "baseline")
        endif()
    endif()
    if(NOT _maybe_baseline_sha)
        file(READ "${CMAKE_SOURCE_DIR}/vcpkg.json" _vcpkg_json_contents)
        string(JSON _maybe_baseline_sha ERROR_VARIABLE _get_baseline_err GET "${_vcpkg_json_contents}" "builtin-baseline")
    endif()

    if(_maybe_baseline_sha)
        set(_baseline_sha "${_maybe_baseline_sha}")
    endif()

    set(_should_update_vcpkg FALSE)
    if(NOT "${_vcpkg_current_head_sha}" STREQUAL "${_baseline_sha}")
        set(_should_update_vcpkg TRUE)
    endif()
    foreach(_patch IN LISTS X_VCPKG_PATCHES)
      get_filename_component(_patch_name "${_patch}" NAME)
      if(NOT EXISTS "${X_VCPKG_CLONE_DIR}/${_patch_name}.applied")
        set(_should_update_vcpkg TRUE)
        break()
      endif()
    endforeach()

    if(_should_update_vcpkg)
        message(STATUS "Fetching changes from vcpkg upstream")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" fetch --quiet --prune
            WORKING_DIRECTORY "${X_VCPKG_CLONE_DIR}"
            # No error checking here to allow offline usage
        )
        message(STATUS "Fetching changes from vcpkg upstream - done")

        message(STATUS "Switching vcpkg HEAD to ${_baseline_sha}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" reset --quiet --hard "${_baseline_sha}"
            COMMAND "${GIT_EXECUTABLE}" clean -qfd
            WORKING_DIRECTORY "${X_VCPKG_CLONE_DIR}"
            COMMAND_ERROR_IS_FATAL ANY
        )
        message(STATUS "Switching vcpkg HEAD to ${_baseline_sha} - done")

        # Apply patches
        foreach(_patch IN LISTS X_VCPKG_PATCHES)
            message(STATUS "Applying patch ${_patch}")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" apply --whitespace nowarn "${_patch}"
                COMMAND "${CMAKE_COMMAND}" -E touch "${X_VCPKG_CLONE_DIR}/${_patch_name}.applied"
                WORKING_DIRECTORY "${X_VCPKG_CLONE_DIR}"
                COMMAND_ERROR_IS_FATAL ANY
            )
        endforeach()

        # Remove vcpkg executable to trigger bootstrap
        if(WIN32)
            file(REMOVE "${X_VCPKG_CLONE_DIR}/vcpkg.exe")
        else()
            file(REMOVE "${X_VCPKG_CLONE_DIR}/vcpkg")
        endif()
    endif()
endfunction()
