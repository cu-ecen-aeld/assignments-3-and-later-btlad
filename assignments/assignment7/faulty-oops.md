### Analysis of kernel oops

#### Command to cause Oops message

```sh
root@qemuarm64:~# echo "hello_world" > /dev/faulty
```
#### The reason for what happened

```sh
[  893.699335] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
```
```sh
[  893.702317] Mem abort info:
[  893.716695]   ESR = 0x0000000096000045
[  893.718607]   EC = 0x25: DABT (current EL), IL = 32 bits
[  893.724265]   SET = 0, FnV = 0
[  893.732409]   EA = 0, S1PTW = 0
[  893.735550]   FSC = 0x05: level 1 translation fault
[  893.737828] Data abort info:
[  893.740397]   ISV = 0, ISS = 0x00000045
[  893.747463]   CM = 0, WnR = 1
[  893.749087] user pgtable: 4k pages, 39-bit VAs, pgdp=00000000436f5000
[  893.752343] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
[  893.783745] Internal error: Oops: 96000045 [#1] PREEMPT SMP
[  893.786107] Modules linked in: faulty(O) hello(O) scull(O)
[  893.796495] CPU: 3 PID: 351 Comm: sh Tainted: G           O      5.15.91-yocto-standard #1
[  893.799296] Hardware name: linux,dummy-virt (DT)
[  893.800586] pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
```

#### Where in code did it occure ?
  - module **faulty**
  - 0x18 bytes into the function *faulty_write*, which is 0x20 bytes long

```sh
[  893.801503] pc : faulty_write+0x18/0x20 [faulty]
```

#### The state of the processor's registers at the event of an error

```sh
[  893.812211] lr : vfs_write+0xf8/0x29c
[  893.812674] sp : ffffffc0096e3d80
[  893.813008] x29: ffffffc0096e3d80 x28: ffffff8003e23600 x27: 0000000000000000
[  893.814380] x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
[  893.815328] x23: 0000000000000000 x22: ffffffc0096e3df0 x21: 000000556d4ebac0
[  893.816137] x20: ffffff80036b2800 x19: 000000000000000c x18: 0000000000000000
[  893.816987] x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
[  893.818861] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[  893.819672] x11: 0000000000000000 x10: 0000000000000000 x9 : ffffffc008264b0c
[  893.826725] x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
[  893.827513] x5 : 0000000000000001 x4 : ffffffc000b6c000 x3 : ffffffc0096e3df0
[  893.828266] x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
```
#### Call trace
```sh
[  893.829260] Call trace:
[  893.829856]  faulty_write+0x18/0x20 [faulty]
[  893.830271]  ksys_write+0x70/0x100
[  893.830536]  __arm64_sys_write+0x24/0x30
[  893.830765]  invoke_syscall+0x5c/0x130
[  893.830942]  el0_svc_common.constprop.0+0x4c/0x100
[  893.831282]  do_el0_svc+0x4c/0xb4
[  893.831545]  el0_svc+0x28/0x80
[  893.831743]  el0t_64_sync_handler+0xa4/0x130
[  893.832026]  el0t_64_sync+0x1a0/0x1a4
[  893.832629] Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
[  893.833893] ---[ end trace c3410d8ac998da08 ]---
Segmentation fault
```
