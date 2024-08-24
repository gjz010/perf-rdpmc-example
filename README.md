# Example using perf_event_open/perfmon(libpfm)/rdpmc together

## Usage

1. Environment setup.
    - Just use `shell.nix`.
    - Note: if you are using a new CPU (like me using Alder Lake), you need a master-branch libpfm as in `shell.nix`!
2. Build.
    - `make`
3. List all events.
    - `./showevtinfo` is bundled.
    - Checkout if you do really have `L2_REQUEST.REFERENCES`. Example related section from my device:
```
IDX	 : 1251999776
PMU name : adl_glc (Intel AlderLake GoldenCove (P-Core))
Name     : L2_REQUEST
Equiv	 : L2_RQSTS
Flags    : None
Desc     : Demand Data Read miss L2 cache
Code     : 0x24
Umask-00 : 0xe4 : PMU : [ALL_CODE_RD] : None : L2 code requests
Umask-01 : 0xe1 : PMU : [ALL_DEMAND_DATA_RD] : None : Demand Data Read access L2 cache
Umask-02 : 0x27 : PMU : [ALL_DEMAND_MISS] : None : Demand requests that miss L2 cache
Umask-03 : 0xf0 : PMU : [ALL_HWPF] : None : TBD
Umask-04 : 0xe2 : PMU : [ALL_RFO] : None : RFO requests to L2 cache.
Umask-05 : 0xc4 : PMU : [CODE_RD_HIT] : None : L2 cache hits when fetching instructions, code reads.
Umask-06 : 0x24 : PMU : [CODE_RD_MISS] : None : L2 cache misses when fetching instructions
Umask-07 : 0xc1 : PMU : [DEMAND_DATA_RD_HIT] : None : Demand Data Read requests that hit L2 cache
Umask-08 : 0x21 : PMU : [DEMAND_DATA_RD_MISS] : None : Demand Data Read miss L2 cache
Umask-09 : 0x30 : PMU : [HWPF_MISS] : None : TBD
Umask-10 : 0x3f : PMU : [MISS] : None : Read requests with true-miss in L2 cache. [This event is alias to L2_REQUEST.MISS]
Umask-11 : 0xff : PMU : [REFERENCES] : None : All accesses to L2 cache [This event is alias to L2_REQUEST.ALL]
Umask-12 : 0xc2 : PMU : [RFO_HIT] : None : RFO requests that hit L2 cache.
Umask-13 : 0x22 : PMU : [RFO_MISS] : None : RFO requests that miss L2 cache
Umask-14 : 0xc8 : PMU : [SWPF_HIT] : None : SW prefetch requests that hit L2 cache.
Umask-15 : 0x28 : PMU : [SWPF_MISS] : None : SW prefetch requests that miss L2 cache.
Modif-00 : 0x00 : PMU : [k] : monitor at priv level 0 (boolean)
Modif-01 : 0x01 : PMU : [u] : monitor at priv level 1, 2, 3 (boolean)
Modif-02 : 0x02 : PMU : [e] : edge level (may require counter-mask >= 1) (boolean)
Modif-03 : 0x03 : PMU : [i] : invert (boolean)
Modif-04 : 0x04 : PMU : [c] : counter-mask in range [0-255] (integer)
Modif-05 : 0x07 : PMU : [intx] : monitor only inside transactional memory region (boolean)
Modif-06 : 0x08 : PMU : [intxcp] : do not count occurrences inside aborted transactional memory region (boolean)
```
4. Run `./test_rdpmc`, or:
```
perf stat -e l2_request.all ./test_rdpmc
```