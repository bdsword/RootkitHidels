# 作業程式碼講解 & 重點整理

## 構想
想要攔截ls的輸出，我們可以從ls用到的底層system call下手，首先用strace看一下ls用到了哪些system call

> $ strace ls

根據輸出結果，與[這篇文章](http://www.cppblog.com/momoxiao/archive/2010/04/04/111594.html)的分析顯示，我們可以替換[getdents64](http://lxr.free-electrons.com/source/include/linux/syscalls.h?v=3.10#L565) system call來達成我們的目的。


## 整體流程

1. 由sidt指令，取得IDT位址
2. 由IDT算出0x80中斷的handler位址
3. 從handler開頭開始掃描找出0xff, 0x14, 0x85這三個opcode一起出現的位置，其偏移+3的位置即為syscall table的位址[後述](#Get_syscall_table_address_30)
4. 將syscall table中存放__getdents64__的項目，其中的值替換成__我們的getdents64__


## 注意重點

1. 在替換syscall之前，要先將cr0暫存器的16 bit([WP Bit](http://turbochaos.blogspot.tw/2013/09/linux-rootkits-101-1-of-3.html))清空，否則會引發Page fault
2. [GCC Inline ASM](http://blog.csdn.net/slvher/article/details/8864996)的使用
3. [IDT結構](http://wiki.osdev.org/Interrupt_Descriptor_Table#Structure)的意義


## Get syscall table address

獲得System call table的位址的方法有很多種，可以參考這個[網址](http://webcache.googleusercontent.com/search?q=cache%3AA6ys0KVxD7sJ%3Awww.linuxeden.com%2Fforum%2Fviewthread.php%3Faction%3Dprintable%26tid%3D62339+&cd=2&hl=zh-TW&ct=clnk&gl=tw)，我們採用的是第三種，或者是[這邊](https://memset.wordpress.com/2011/01/20/syscall-hijacking-dynamically-obtain-syscall-table-address-kernel-2-6-x/)也有說明幾種方式


## 程式碼說明

### 函數解說

1. [hook_getdents64](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L92): 將getdents64替換成我們自己製作的[my_sys_getdents64](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L47)
2. [unhook_getdents64](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L125): 將getdents64從我們自己製作的假function換回原本的getdents64
3. [get_syscall_table_address](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L34): 將int 0x80 handler的起始位址作為參數，回傳syscall table的位址
4. [get_wp_bit](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L72): 取得cr0的第16 bit([WP Bit](http://turbochaos.blogspot.tw/2013/09/linux-rootkits-101-1-of-3.html))
5. [set_wp_bit](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L82): 設定cr0的第16 bit([WP Bit](http://turbochaos.blogspot.tw/2013/09/linux-rootkits-101-1-of-3.html))
6. [my_sys_getdents64](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L4): 我們自己製作的假function，用來將[target_file](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L13)從回傳的結果中過濾掉

### 結構體說明

1. [18-21](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L18-L21): 定義IDTR的結構體，[sidt](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L98)指令會將IDTR的內容儲存於這個結構中
2. [L23-L29](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L23-L29): 定義IDT中的一個項目(entry)的結構體，int 0x80即位於IDT偏移8*0x80的位置，我們透過該位置的此entry結構體，算出int 0x80 handler的起始位址

### Hook程式碼解說

1. Linux module從[module_init()](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L136)傳入的函數開始執行；module被卸載的時候，會從[module_exit()](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L137)傳入的函數做清除的動作
2. [L98](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L98)使用inline asm語法呼叫sidt指令，將IDTR的內容存到全域變數idtr中
3. [L101](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L101)由idtr的值算出int 0x80中斷的handler的起始位址
4. [L108](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L108)將int 0x80 handler的起始位址傳入[get_syscall_table_address](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L34)函數，取得syscall table的位址
5. [get_syscall_table_address](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L34)的原理: 請參考上面的[文章](http://webcache.googleusercontent.com/search?q=cache%3AA6ys0KVxD7sJ%3Awww.linuxeden.com%2Fforum%2Fviewthread.php%3Faction%3Dprintable%26tid%3D62339+&cd=2&hl=zh-TW&ct=clnk&gl=tw)，基本上就是從起始位址開始尋找0xff, 0x14, 0x85這三個hex連續出現的位置，該位置偏移+3的地方即為syscall table
6. [L111](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L111)我們將原本的getdents64存在全域函數指標變數[original_sys_getdents64](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L45)中
7. [L113-L114](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L113-L114)取得當前的cr0 WP Bit並將其存起來，修改完syscall table之後，要將其恢復。之後清空WP Bit
8. [L116](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L116)替換系統的getdents64為我們自己製作的假getdents64
9. [L118](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L118)恢復WP Bit

### Unhook程式碼說明

1. [L127-L128](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L127-L128)取得當前的cr0 WP Bit並將其存起來，修改完syscall table之後，要將其恢復。之後清空WP Bit
2. [L130](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L130)將系統原本的getdents64復原
3. [L132](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L132)恢復WP Bit

### 假getdents64函數([my_sys_getdents64](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L47))說明

1. [L48](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L48)呼叫原本系統的getdents64函數，因為我們只是要過濾掉[target_file](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L13)，因此還是靠原本的函數幫我們做一些底層的呼叫，我們僅僅從它的回傳值做過濾
2. [L58-L68](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L58-L68)for迴圈將回傳的dirent(其實是一個[struct linux_dirent64的array](http://lxr.free-electrons.com/source/include/linux/dirent.h?v=3.10#L4))遍歷，檢查每一項的d_name是否為[target_file](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L13)，如果是的話，則跳過這一個entry，[將下一個entry之後的整段buffer移到現在這個entry的位置，蓋掉目前的這個entry，並將buffer大小減去d_reclen](https://github.com/bdsword/RootkitHidels/blob/master/rootkit_hidels.c#L63-L64)
