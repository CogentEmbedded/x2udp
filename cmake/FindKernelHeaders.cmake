# Find the kernel release
execute_process(
  COMMAND uname -r
  OUTPUT_VARIABLE KERNEL_RELEASE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Find the headers
find_path(KERNELHEADERS_DIR
  include/linux/user.h
  HINTS
    /usr/src/kernels/${KERNEL_RELEASE}
    /usr/src/kernel
    /usr
)

message(STATUS "Kernel release: ${KERNEL_RELEASE}")
message(STATUS "Kernel headers: ${KERNELHEADERS_DIR}")

if (KERNELHEADERS_DIR)
  set(KERNELHEADERS_INCLUDE_DIRS
    ${KERNELHEADERS_DIR}/include
    ${KERNELHEADERS_DIR}/arch/x86/include
    CACHE PATH "Kernel headers include dirs"
  )
endif (KERNELHEADERS_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KernelHeaders DEFAULT_MSG
                                  KERNELHEADERS_INCLUDE_DIRS)

mark_as_advanced(KERNELHEADERS_INCLUDE_DIRS)
