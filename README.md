# qCheck [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A **q**uick alternative to cksfv for generating and verifying CRC32C checksum files(`.sfv`).

---

qCheck is about **x4** to **x15** times faster than alternatives such as
`rhash` and `cksfv` and reduces algorithmic overhead to being almost
entirely bounded by device IO!
It is recommended to run this program on a fast NVME Solid State Drive to fully
saturate read speed.

---

## Install

### Manual
```
$ git clone https://github.com/Wunkolo/qCheck.git qCheck/src
$ cmake -B qCheck/build -S qCheck/src -DCMAKE_BUILD_TYPE=Release
$ cmake --build qCheck/build --config Release
# install -Dm+x "qCheck/build/qCheck" "/usr/local/bin/qCheck"
```

### Packages

[ArchLinux](https://aur.archlinux.org/packages/qcheck-git)


## Benchmark

Specs:

```
% inxi -IC
CPU:       Topology: 10-Core model: Intel Core i9-7900X bits: 64 type: MT MCP L2 cache: 13.8 MiB 
           Speed: 1201 MHz min/max: 1200/4500 MHz Core speeds (MHz): 1: 1201 2: 1200 3: 1200 4: 1201 5: 1201 6: 1200 7: 1197 
           8: 1201 9: 1201 10: 1201 11: 1201 12: 1201 13: 1201 14: 1201 15: 1200 16: 1201 17: 1200 18: 1200 19: 1200 20: 1200 
Info:      Processes: 350 Uptime: 9d 14h 45m Memory: 62.51 GiB used: 19.88 GiB (31.8%) Init: systemd Shell: vim inxi: 3.0.37 

% sudo nvme list
Node             SN                   Model                                    Namespace Usage                      Format           FW Rev  
---------------- -------------------- ---------------------------------------- --------- -------------------------- ---------------- --------
<snip>
/dev/nvme1n1     S~~~N~~~9~~~7~~      Samsung SSD 970 EVO 500GB                1         341.05  GB / 500.11  GB    512   B +  0 B   2B2QEXE7
```

Verifying an .sfv file(10GB folder full of +200MB files)
```
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

Generating a .sfv file(10GB folder full of +200MB files)
```
% hyperfine --runs 10 "rhash SampleFiles/*" "./qCheck SampleFiles/*" "cksfv SampleFiles/*"
Benchmark #1: rhash SampleFiles/*
  Time (mean ± σ):      6.234 s ±  0.145 s    [User: 4.798 s, System: 1.424 s]
  Range (min … max):    6.133 s …  6.632 s    10 runs

Benchmark #2: ./qCheck SampleFiles/*
  Time (mean ± σ):      1.435 s ±  0.105 s    [User: 5.146 s, System: 0.610 s]
  Range (min … max):    1.289 s …  1.590 s    10 runs

Benchmark #3: cksfv SampleFiles/*
  Time (mean ± σ):     20.896 s ±  0.074 s    [User: 19.611 s, System: 1.271 s]
  Range (min … max):   20.778 s … 20.996 s    10 runs

Summary
  './qCheck SampleFiles/*' ran
    4.34 ± 0.33 times faster than 'rhash SampleFiles/*'
   14.56 ± 1.07 times faster than 'cksfv SampleFiles/*'
```
