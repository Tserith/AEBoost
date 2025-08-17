cmake -P cmkr.cmake
cmake -A Win32 -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --config Release