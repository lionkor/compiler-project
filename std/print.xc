fn std_print(u64 str) -> u64 ret {
    ret = std_syscall(1, 0, str, deref(str-8));
}
