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
