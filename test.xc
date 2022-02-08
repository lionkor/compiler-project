use std/print;

fn main() -> i64 ret {
    ret = 0;
    i64 str;
    str = "Hello!, sweet world\n";
    std_syscall(1, 0, str, deref(str-8));
}

