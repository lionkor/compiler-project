fn main() -> i64 ret {
    i64 str;
    str = "Hello!\n";
    ret = std_syscall(1,0, str, str-8);
}

