#!/bin/bash
./build/vampire --mode fo2 tests/tptp_raw/Problems/KRS/KRS026+1.p > out.txt 2>&1
grep -A 5 -B 5 "S2 invariant violation" out.txt
