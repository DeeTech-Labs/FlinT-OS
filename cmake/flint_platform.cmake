set(FLINT_PLATFORM_DIR "${CMAKE_CURRENT_LIST_DIR}/../platform" CACHE PATH "IgniteSDK checkout (defaults to the pinned submodule)")
if(NOT EXISTS "${FLINT_PLATFORM_DIR}/include/flint/contracts.h")
    message(FATAL_ERROR "Initialize IgniteSDK: git submodule update --init --recursive")
endif()

# The sensor broker/runtime and on-device packages share one versioned contract.
file(READ "${FLINT_PLATFORM_DIR}/include/flint/contracts.h" _flint_contract_line)
if(NOT _flint_contract_line MATCHES "ContractRevision[ \t]*=[ \t]*0x20261114[ \t]*;")
    message(FATAL_ERROR "FlinT OS requires IgniteSDK ContractRevision 0x20261114. The pinned SDK is older; configure -DFLINT_PLATFORM_DIR=../Flint-IgniteSDK with the matching SDK checkout until its reviewed revision is committed and pinned.")
endif()
