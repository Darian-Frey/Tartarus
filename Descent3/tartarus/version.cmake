# Script mode (-P): writes ${TARGET_DIR}/lib/tartarus_version.h for F-004.
# Inputs: SOURCE_DIR, TARTARUS_DIR, TARGET_DIR, TARTARUS_VERSION.

# Our own revision. Only Tartarus tags (tartarus-v*) are considered, so the
# upstream v1.x tags pulled in by `git fetch upstream` never label our build.
set(TARTARUS_GIT_HASH "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --dirty --abbrev=8 --match "tartarus-v*"
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE _describe
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _rc
  )
  if(_rc EQUAL 0 AND NOT _describe STREQUAL "")
    set(TARTARUS_GIT_HASH "${_describe}")
  endif()
endif()

# The upstream commit last merged in. Committed (and updated by
# tools/upstream-sync.sh --apply) because a fresh clone has no upstream remote.
set(TARTARUS_UPSTREAM_HASH "unknown")
if(EXISTS ${TARTARUS_DIR}/UPSTREAM_REF)
  file(STRINGS ${TARTARUS_DIR}/UPSTREAM_REF _ref LIMIT_COUNT 1 REGEX "^[0-9a-f]+$")
  if(_ref)
    string(SUBSTRING "${_ref}" 0 8 TARTARUS_UPSTREAM_HASH)
  endif()
endif()

configure_file(
  ${TARTARUS_DIR}/tartarus_version.h.in
  ${TARGET_DIR}/lib/tartarus_version.h
  @ONLY
)
