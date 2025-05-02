# uC-VFS Main Repo

## Run
``` bash
cmake -DCMAKE_TOOLCHAIN_FILE=./toolchain-arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug -DBOARD=mps2_an385 -DCONFIG_FS=1 -S . -B build
make -C build 
make -C build run
```