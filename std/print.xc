fn length_recursive(u64 s, u64 len) -> u64 new_len {
    if (deref8(s)) {
        new_len = length_recursive(s + 1, len + 1);
    } else {
        new_len = len;
    }
}

fn length(u64 s) -> u64 len {
    len = length_recursive(s, 0);
}

fn std_print(u64 str) -> u64 ret {
    ret = std_syscall(1, 0, str, length(str));
}

