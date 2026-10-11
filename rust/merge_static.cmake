# Merges the Rust static library into the C one, so that a static build is
# still linked as the single archive liboapv.a (or oapv.lib).
#   LIB      : the C archive, replaced by the merged one
#   RUST_LIB : the archive built by cargo
#   AR       : the archiver; MODE selects how it is driven (mri, libtool, lib)
set(TMP "${LIB}.merged")
if(MODE STREQUAL "libtool")
  execute_process(COMMAND libtool -static -o "${TMP}" "${LIB}" "${RUST_LIB}" RESULT_VARIABLE ret)
elseif(MODE STREQUAL "lib")
  execute_process(COMMAND "${AR}" /NOLOGO "/OUT:${TMP}" "${LIB}" "${RUST_LIB}" RESULT_VARIABLE ret)
else()
  file(WRITE "${LIB}.mri" "create ${TMP}\naddlib ${LIB}\naddlib ${RUST_LIB}\nsave\nend\n")
  execute_process(COMMAND "${AR}" -M INPUT_FILE "${LIB}.mri" RESULT_VARIABLE ret)
endif()
if(NOT ret EQUAL 0)
  message(FATAL_ERROR "failed to merge ${RUST_LIB} into ${LIB}")
endif()
file(RENAME "${TMP}" "${LIB}")
