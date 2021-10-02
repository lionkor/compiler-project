fn add(i64 a, i64 b) -> i64 c {
    c = a + b;
}

fn main(i64 a) -> i64 ret {
    a = "Hello, 'World'\n";
    ret = syscall(1, 0, a, 16);
}

