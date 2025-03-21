# ttr-converter
Converter that converts TCP data to RS485 data, hereinafter referred to as TTR

# build and run

## build
```shell
rm -rf build
mkdir build
cd build
cmake ..
make -j$(sysctl -n hw.logicalcpu)
```

## run
```shell
./tcp_to_rs485
```