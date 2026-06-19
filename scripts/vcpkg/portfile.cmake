vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ktprime/emhash
    REF "v${VERSION}"
    # SHA512 must be updated after each release:
    #   vcpkg install emhash && vcpkg hash --algorithm SHA512 <tarball>
    SHA512 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_BENCHMARKS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME emhash CONFIG_PATH lib/cmake/emhash)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")
if(EXISTS "${CURRENT_PACKAGES_DIR}/lib64")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib64")
endif()

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)

file(READ "${SOURCE_PATH}/VERSION" EMHASH_VERSION)
configure_file("${CMAKE_CURRENT_LIST_DIR}/vcpkg.json" "${CURRENT_PACKAGES_DIR}/share/${PORT}/vcpkg.json" @ONLY)
