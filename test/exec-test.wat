(module
  (import "env" "yos_fork"        (func $yos_fork        (result i32)))
  (import "env" "yos_fork_result" (func $yos_fork_result (result i32)))
  (import "env" "yos_getpid"      (func $yos_getpid      (result i32)))
  (import "env" "yos_exit"        (func $yos_exit        (param i32)))
  (import "env" "yos_wait"        (func $yos_wait        (param i32) (result i32)))
  (import "env" "yos_write"       (func $yos_write       (param i32 i32 i32) (result i32)))
  (import "env" "yos_exec"        (func $yos_exec        (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  ;; String data
  (data (i32.const 256) "parent: forking then child will exec hello.wasm\n")  ;; 48
  (data (i32.const 310) "parent: forked child pid=")                           ;; 25
  (data (i32.const 340) "parent: child exited with code=")                     ;; 31
  (data (i32.const 380) "child: about to exec hello.wasm\n")                   ;; 32
  (data (i32.const 420) "hello.wasm")                                          ;; 10
  (data (i32.const 440) "child: exec failed!\n")                               ;; 20
  (data (i32.const 470) "parent: done\n")                                      ;; 13

  (func $itoa (param $val i32) (param $offset i32) (result i32)
    (local $len i32) (local $tmp i32) (local $i i32) (local $j i32) (local $swap i32)
    (local.set $tmp (local.get $val))
    (if (i32.eqz (local.get $tmp)) (then
      (i32.store8 (local.get $offset) (i32.const 48))
      (return (i32.const 1))))
    (local.set $len (i32.const 0))
    (block $done (loop $loop
      (br_if $done (i32.eqz (local.get $tmp)))
      (i32.store8 (i32.add (local.get $offset) (local.get $len))
        (i32.add (i32.const 48) (i32.rem_u (local.get $tmp) (i32.const 10))))
      (local.set $tmp (i32.div_u (local.get $tmp) (i32.const 10)))
      (local.set $len (i32.add (local.get $len) (i32.const 1)))
      (br $loop)))
    (local.set $i (i32.const 0))
    (local.set $j (i32.sub (local.get $len) (i32.const 1)))
    (block $rdone (loop $rloop
      (br_if $rdone (i32.ge_u (local.get $i) (local.get $j)))
      (local.set $swap (i32.load8_u (i32.add (local.get $offset) (local.get $i))))
      (i32.store8 (i32.add (local.get $offset) (local.get $i))
        (i32.load8_u (i32.add (local.get $offset) (local.get $j))))
      (i32.store8 (i32.add (local.get $offset) (local.get $j)) (local.get $swap))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (local.set $j (i32.sub (local.get $j) (i32.const 1)))
      (br $rloop)))
    (local.get $len))

  (func $strcpy (param $dst i32) (param $src i32) (param $len i32) (result i32)
    (local $i i32)
    (block $done (loop $loop
      (br_if $done (i32.ge_u (local.get $i) (local.get $len)))
      (i32.store8 (i32.add (local.get $dst) (local.get $i))
        (i32.load8_u (i32.add (local.get $src) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br $loop)))
    (i32.add (local.get $dst) (local.get $len)))

  (func $write_num (param $dst i32) (param $val i32) (result i32)
    (local $len i32)
    (local.set $len (call $itoa (local.get $val) (local.get $dst)))
    (i32.add (local.get $dst) (local.get $len)))

  (func $println (param $end i32)
    (i32.store8 (local.get $end) (i32.const 10))
    (drop (call $yos_write (i32.const 1) (i32.const 1024)
      (i32.sub (i32.add (local.get $end) (i32.const 1)) (i32.const 1024)))))

  (func $_start (export "_start")
    (local $child_pid i32)
    (local $child_exit i32)
    (local $pos i32)

    ;; Check if forked child
    (if (i32.eqz (call $yos_fork_result))
      (then
        ;; === CHILD: exec hello.wasm ===
        (drop (call $yos_write (i32.const 1) (i32.const 380) (i32.const 32)))
        (drop (call $yos_exec (i32.const 420) (i32.const 10)))
        ;; If exec returns, it failed
        (drop (call $yos_write (i32.const 1) (i32.const 440) (i32.const 20)))
        (call $yos_exit (i32.const 1))
        (return)
      )
    )

    ;; === PARENT ===
    (drop (call $yos_write (i32.const 1) (i32.const 256) (i32.const 48)))

    (local.set $child_pid (call $yos_fork))

    ;; "parent: forked child pid=<N>"
    (local.set $pos (call $strcpy (i32.const 1024) (i32.const 310) (i32.const 25)))
    (local.set $pos (call $write_num (local.get $pos) (local.get $child_pid)))
    (call $println (local.get $pos))

    ;; Wait
    (local.set $child_exit (call $yos_wait (local.get $child_pid)))

    ;; "parent: child exited with code=<N>"
    (local.set $pos (call $strcpy (i32.const 1024) (i32.const 340) (i32.const 31)))
    (local.set $pos (call $write_num (local.get $pos) (local.get $child_exit)))
    (call $println (local.get $pos))

    (drop (call $yos_write (i32.const 1) (i32.const 470) (i32.const 13)))
    (call $yos_exit (i32.const 0))
  )
)
