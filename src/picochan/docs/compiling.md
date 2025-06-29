## Compiling using Picochan {#compiling_page}

Picochan is written in C using the Pico SDK. It uses CMake in the
way recommended by that SDK. Similarly to how the Pico SDK divides
into multiple modules with names of the form `pico_foo` and
`hardware_foo`, Picochan is divided into three CMake modules:
* `picochan_base`
* `picochan_css`
* `picochan_cu`

Application code need only compile with the `picochan_base` and
`picochan_css` modules.

Device driver code need only compile with the `picochan_base` and
`picochan_cu` modules.

This Doxygen-format Picochan documentation is in its early stages
and does not separate API information clearly enough from
internal implementation details. In an attempt to make some sort
of separation, much of the Doxygen documentation (generated from
code comments) has been marked as belonging to a "Doxygen topic"
with a name of either `picochan_base`, `picochan_css`, `picochan_cu`
or `internal_foo` (for various values of `foo`). The intent is that
any Doxygen topic with prefix `internal_` is not intended for API
use but there may be mis-classifications.

More documentation is needed on how to compile against Picochan
but in brief:
* Prepare your CMakeLists.txt in the usual way for Pico SDK
* Set environment variable `PICO_SDK_PATH` to the path of the
  Picochan library source - the `src/picochan` subtree of the
  Picochan repository. This path is the one which contains the
  top-level `CMakeLists.txt` file starting:

  ```
  if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/base/include/picochan/ccw.h)
  ```
  It is _not_ the root directory of the repository (which contains
  subdirectories "src" and "tools") nor is it the "base" subdirectory
  of the library source subtree which contains a `CMakeLists.txt`
  file starting:

  ```
  add_library(picochan_base INTERFACE)
  ```
* In your own `CMakeLists.txt` file for your software, add the
  following in appropriate places
  - Before any `target_compile_definitions` or similar, add
    ```
    include($ENV{PICOCHAN_PATH}/CMakeLists.txt)
    ```
  - In the `target_link_libraries` section for your target
    * add `picochan_css` if using application API (to CSS)
    * add `picochan_cu` if using device driver API (on CU)
    * add `hardware_dma` and `hardware_pio` if using any pio channels
    * add `hardware_uart` if using any uart channels
  - In the `target_compile_definitions` for your target, add any
    desired settings to any extra runtime sanity and argument checks
    for Debug builds or to enable tracing, such as
```
    PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
    PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
    PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
    PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
    PARAM_ASSERTIONS_ENABLED_PCH_CSS=1
```
  - Add definitions to choose sizes for various global tables if the
    defaults are not suitable (and they may well not be).
    * Examples for using CSS:
```
    PCH_NUM_CSS_CUS=4
    PCH_NUM_SCHIBS=40
```
    * Examples for using CU:
```
    PCH_NUM_CUS=2
```
