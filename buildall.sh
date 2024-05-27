#!/bin/bash
find . -type f \( -name '*.o' -or -name '*.d' \) -exec rm {} \; && make CIRCLESTDLIBHOME=/home/jack/Downloads/circle-arch/raspi1
find . -type f \( -name '*.o' -or -name '*.d' \) -exec rm {} \; && make CIRCLESTDLIBHOME=/home/jack/Downloads/circle-arch/raspi2
find . -type f \( -name '*.o' -or -name '*.d' \) -exec rm {} \; && make CIRCLESTDLIBHOME=/home/jack/Downloads/circle-arch/raspi3-32
find . -type f \( -name '*.o' -or -name '*.d' \) -exec rm {} \; && make CIRCLESTDLIBHOME=/home/jack/Downloads/circle-arch/raspi3-64
find . -type f \( -name '*.o' -or -name '*.d' \) -exec rm {} \; && make CIRCLESTDLIBHOME=/home/jack/Downloads/circle-arch/raspi4-32
find . -type f \( -name '*.o' -or -name '*.d' \) -exec rm {} \; && make CIRCLESTDLIBHOME=/home/jack/Downloads/circle-arch/raspi4-64