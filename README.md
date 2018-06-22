# SIMD-Guid

SIMD implemented UUID parsing (stored with big endianness).

Requires SSE4.2

Some benchmark numbers:

Run on (8 X 3998 MHz CPU s)
CPU Caches:
  L1 Data 32K (x4)
  L1 Instruction 32K (x4)
  L2 Unified 262K (x4)
  L3 Unified 8388K (x1)
  
```
Benchmark                     Time           CPU Iterations
BM_MyUUID                     8 ns          8 ns   89600000
BM_WindowsUUID               46 ns         46 ns   15448276
BM_BoostUUID                230 ns        230 ns    2986667
BM_MyUUIDToString            70 ns         70 ns    8960000
BM_BoostUUIDToString        130 ns        131 ns    5600000
Press any key to continue . . .
```
