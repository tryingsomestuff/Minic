## Build scripts

Builds made through these scripts identify themselves with the base version,
the six-character Git commit, and an optional `-dirty` suffix (for example,
`3.47-af45d8-dirty`). The dirty marker covers tracked worktree/index changes
and untracked files under `Source`; generated build outputs in submodules are
ignored. Builds outside a Git checkout report `unknown` as the commit.

## Android debug

```
> qemu-arm-static -L /usr/arm-linux-gnueabihf -g 1234 ./Dist/Minic3/minic_dev_android -analyze "start" 20 -minOutputLevel 0
# in another terminal
> gdb-multiarch -q --nh -ex 'set architecture arm' -ex 'file ./Dist/Minic3/minic_dev_android' -ex 'target remote localhost:1234'
```
