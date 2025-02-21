#!/bin/sh
# Copyright 2023 Google LLC
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

################################## ARM NEON ###################################
tools/xngen src/qs16-qs8-vcvt/neon.c.in -D BATCH_TILE=8  -o src/qs16-qs8-vcvt/gen/qs16-qs8-vcvt-neon-x8.c &
tools/xngen src/qs16-qs8-vcvt/neon.c.in -D BATCH_TILE=16 -o src/qs16-qs8-vcvt/gen/qs16-qs8-vcvt-neon-x16.c &
tools/xngen src/qs16-qs8-vcvt/neon.c.in -D BATCH_TILE=32 -o src/qs16-qs8-vcvt/gen/qs16-qs8-vcvt-neon-x32.c &

#################################### Scalar ###################################
tools/xngen src/qs16-qs8-vcvt/scalar.c.in -D BATCH_TILE=1 -o src/qs16-qs8-vcvt/gen/qs16-qs8-vcvt-scalar-x1.c &
tools/xngen src/qs16-qs8-vcvt/scalar.c.in -D BATCH_TILE=2 -o src/qs16-qs8-vcvt/gen/qs16-qs8-vcvt-scalar-x2.c &
tools/xngen src/qs16-qs8-vcvt/scalar.c.in -D BATCH_TILE=4 -o src/qs16-qs8-vcvt/gen/qs16-qs8-vcvt-scalar-x4.c &

################################## Unit tests #################################
tools/generate-vcvt-test.py --spec test/qs16-qs8-vcvt.yaml --output test/qs16-qs8-vcvt.cc &

wait
