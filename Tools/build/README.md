## Build scripts

TODO

## Android debug

```
> qemu-arm-static -L /usr/arm-linux-gnueabihf -g 1234 ./Dist/Minic3/minic_dev_android -analyze "start" 20 -minOutputLevel 0
# in another terminal
> gdb-multiarch -q --nh -ex 'set architecture arm' -ex 'file ./Dist/Minic3/minic_dev_android' -ex 'target remote localhost:1234'
```
