#  -*- mode: cmake -*-

#
# Build TPL: MSTK 
#    
# --- Define all the directories and common external project flags
if (NOT ENABLE_XSDK)
  define_external_project_args(MSTK
                               TARGET mstk
                               DEPENDS HDF5 NetCDF SEACAS METIS ParMetis Trilinos)
else()
  define_external_project_args(MSTK
                               TARGET mstk
                               DEPENDS XSDK SEACAS)
endif()

# add version version to the autogenerated tpl_versions.h file
amanzi_tpl_version_write(FILENAME ${TPL_VERSIONS_INCLUDE_FILE}
                         PREFIX MSTK
                         VERSION ${MSTK_VERSION_MAJOR} ${MSTK_VERSION_MINOR} ${MSTK_VERSION_PATCH})

# --- Patch the original code
set(MSTK_patch_file mstk-cmake.patch)
set(MSTK_sh_patch ${MSTK_prefix_dir}/mstk-patch-step.sh)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/mstk-patch-step.sh.in
               ${MSTK_sh_patch}
               @ONLY)

# configure the CMake patch step
set(MSTK_cmake_patch ${MSTK_prefix_dir}/mstk-patch-step.cmake)
configure_file(${SuperBuild_TEMPLATE_FILES_DIR}/mstk-patch-step.cmake.in
               ${MSTK_cmake_patch}
               @ONLY)

# set the patch command
set(MSTK_PATCH_COMMAND ${CMAKE_COMMAND} -P ${MSTK_cmake_patch})

# --- Define the configure parameters
# compile flags
set(mstk_cflags_list -I${TPL_INSTALL_PREFIX}/include ${Amanzi_COMMON_CFLAGS})
build_whitespace_string(mstk_cflags ${mstk_cflags_list})

set(mstk_ldflags_list -L${TPL_INSTALL_PREFIX}/lib ${MPI_C_LIBRARIES})
build_whitespace_string(mstk_ldflags ${mstk_ldflags_list})

set(MSTK_CMAKE_CACHE_ARGS
		    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
                    -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                    -DCMAKE_C_FLAGS:STRING=${mstk_cflags}
                    -DCMAKE_EXE_LINKER_FLAGS:STRING=${mstk_ldflags}
		    -DPREFER_STATIC_LIBRARIES:BOOL=${PREFER_STATIC_LIBRARIES}
                    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
                    -DMPI_CXX_COMPILER:FILEPATH=${MPI_CXX_COMPILER}
                    -DMPI_C_COMPILER:FILEPATH=${MPI_C_COMPILER}
                    -DMSTK_USE_MARKERS:BOOL=TRUE		    
                    -DENABLE_PARALLEL:BOOL=TRUE
                    -DENABLE_ExodusII:BOOL=TRUE
                    -DENABLE_EXODUSII:BOOL=TRUE
                    -DENABLE_ZOLTAN:BOOL=TRUE
                    -DENABLE_METIS:BOOL=TRUE
                    -DMETIS_MAJOR_VER:STRING=5
                    -DZOLTAN_NEEDS_ParMETIS:BOOL=TRUE
                    -DHDF5_DIR:PATH=${HDF5_DIR}
                    -DHDF5_INCLUDE_DIRS:PATH=${HDF5_INCLUDE_DIRS}
                    -DHDF5_LIBRARIES:LIST=${HDF5_LIBRARIES}
                    -DHDF5_NO_SYSTEM_PATHS:BOOL=TRUE
                    -DNetCDF_DIR:PATH=${NetCDF_DIR} 
                    -DExodusII_DIR:PATH=${TPL_INSTALL_PREFIX}/SEACAS
                    -DZOLTAN_DIR:PATH=${Zoltan_INSTALL_PREFIX}
                    -DMetis_DIR:PATH=${METIS_DIR} 
                    -DMETIS_DIR:PATH=${METIS_DIR} 
                    -DMetis_LIB_DIR:PATH=${METIS_DIR}/lib 
                    -DMETIS_LIB_DIR:PATH=${METIS_DIR}/lib 
                    -DMetis_LIBRARY:PATH=${METIS_LIBRARIES}
                    -DMETIS_LIBRARY:PATH=${METIS_LIBRARIES}
                    -DMetis_INCLUDE_DIR:PATH=${METIS_DIR}/include 
                    -DMETIS_INCLUDE_DIR:PATH=${METIS_DIR}/include 
                    -DMetis_INCLUDE_DIRS:PATH=${METIS_DIR}/include
                    -DMETIS_INCLUDE_DIRS:PATH=${METIS_DIR}/include
                    -DParMETIS_DIR:PATH=${METIS_DIR}
                    -DENABLE_Tests:BOOL=FALSE
                    -DINSTALL_DIR:PATH=${TPL_INSTALL_PREFIX}
                    -DINSTALL_ADD_VERSION:BOOL=FALSE)

# --- Add external project build and tie to the MSTK build target
ExternalProject_Add(${MSTK_BUILD_TARGET}
                    DEPENDS   ${MSTK_PACKAGE_DEPENDS}             # Package dependency target
                    TMP_DIR   ${MSTK_tmp_dir}                     # Temporary files directory
                    STAMP_DIR ${MSTK_stamp_dir}                   # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR  ${TPL_DOWNLOAD_DIR}             # Download directory
                    URL           ${MSTK_URL}                     # URL may be a web site OR a local file
                    URL_MD5       ${MSTK_MD5_SUM}                 # md5sum of the archive file
                    DOWNLOAD_NAME ${MSTK_SAVEAS_FILE}             # file name to store (if not end of URL)
                    # -- Patch 
                    PATCH_COMMAND ${MSTK_PATCH_COMMAND}
                    # -- Configure
                    SOURCE_DIR       ${MSTK_source_dir}           # Source directory
                    CMAKE_ARGS       -Wno-dev
                    CMAKE_CACHE_ARGS ${AMANZI_CMAKE_CACHE_ARGS}   # Global definitions from root CMakeList
                                     ${MSTK_CMAKE_CACHE_ARGS}
                    # -- Build
                    BINARY_DIR       ${MSTK_build_dir}            # Build directory 
                    BUILD_COMMAND    $(MAKE)                      # $(MAKE) enables parallel builds through make
                    BUILD_IN_SOURCE  ${MSTK_BUILD_IN_SOURCE}      # Flag for in source builds
                    # -- Install
                    INSTALL_DIR      ${TPL_INSTALL_PREFIX}        # Install directory
                    # -- Output control
                    ${MSTK_logging_args})

# --- set cache (global) variables
global_set(MSTK_INCLUDE_DIR "${TPL_INSTALL_PREFIX}/include")
global_set(MSTK_LIBRARY_DIR "${TPL_INSTALL_PREFIX}/lib")
