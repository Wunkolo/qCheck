# qCheck [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**Work in progress**

A **q**uick alternative to cksfv for generating and verifying CRC32C checksum files.

---
```
% inxi -IC
CPU:       Topology: 10-Core model: Intel Core i9-7900X bits: 64 type: MT MCP L2 cache: 13.8 MiB 
           Speed: 1201 MHz min/max: 1200/4500 MHz Core speeds (MHz): 1: 1201 2: 1200 3: 1200 4: 1201 5: 1201 6: 1200 7: 1197 
           8: 1201 9: 1201 10: 1201 11: 1201 12: 1201 13: 1201 14: 1201 15: 1200 16: 1201 17: 1200 18: 1200 19: 1200 20: 1200 
Info:      Processes: 350 Uptime: 9d 14h 45m Memory: 62.51 GiB used: 19.88 GiB (31.8%) Init: systemd Shell: vim inxi: 3.0.37 

% hyperfine --runs 10 -i '../qCheck -c TestCheck.sfv' 'rhash -c TestCheck.sfv' 'cksfv -g TestCheck.sfv'
Benchmark #1: ../qCheck -c TestCheck.sfv
  Time (mean ± σ):      1.494 s ±  0.198 s    [User: 5.140 s, System: 0.817 s]
  Range (min … max):    1.282 s …  1.740 s    10 runs

Benchmark #2: rhash -c TestCheck.sfv
  Time (mean ± σ):      7.374 s ±  0.015 s    [User: 5.769 s, System: 1.598 s]
  Range (min … max):    7.352 s …  7.400 s    10 runs
Benchmark #3: cksfv -g TestCheck.sfv
  Time (mean ± σ):     21.376 s ±  0.663 s    [User: 19.848 s, System: 1.512 s]
  Range (min … max):   21.084 s … 23.260 s    10 runs
Summary
  '../qCheck -c TestCheck.sfv' ran
    4.94 ± 0.65 times faster than 'rhash -c TestCheck.sfv'
   14.31 ± 1.94 times faster than 'cksfv -g TestCheck.sfv'
```
