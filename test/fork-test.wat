(module
  ;; Import YOS syscalls
  (import "env" "yos_fork"        (func $yos_fork        (result i32)))
  (import "env" "yos_fork_result" (func $yos_fork_result (result i32)))
  (import "env" "yos_getpid"      (func $yos_getpid      (result i32)))
  (import "env" "yos_getppid"     (func $yos_getppid     (result i32)))
  (import "env" "yos_exit"        (func $yos_exit        (param i32)))
  (import "env" "yos_wait"        (func $yos_wait        (param i32) (result i32)))
  (import "env" "yos_write"       (func $yos_write       (param i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  ;; Scratch buffer at offset 1024 for building output lines
  ;; String constants at offset 256

  ;; itoa: write decimal i32 to memory at $offset, return length
  (func $itoa (param $val i32) (param $offset i32) (result i32)
    (local $len i32)
    (local $tmp i32)
    (local $i i32)
    (local $j i32)
    (local $swap i32)

    (local.set $tmp (local.get $val))

    (if (i32.eqz (local.get $tmp))
      (then
        (i32.store8 (local.get $offset) (i32.const 48))
        (return (i32.const 1))
      )
    )

    (local.set $len (i32.const 0))
    (block $done
      (loop $loop
        (br_if $done (i32.eqz (local.get $tmp)))
        (i32.store8
          (i32.add (local.get $offset) (local.get $len))
          (i32.add (i32.const 48) (i32.rem_u (local.get $tmp) (i32.const 10))))
        (local.set $tmp (i32.div_u (local.get $tmp) (i32.const 10)))
        (local.set $len (i32.add (local.get $len) (i32.const 1)))
        (br $loop)
      )
    )

    (local.set $i (i32.const 0))
    (local.set $j (i32.sub (local.get $len) (i32.const 1)))
    (block $rdone
      (loop $rloop
        (br_if $rdone (i32.ge_u (local.get $i) (local.get $j)))
        (local.set $swap
          (i32.load8_u (i32.add (local.get $offset) (local.get $i))))
        (i32.store8
          (i32.add (local.get $offset) (local.get $i))
          (i32.load8_u (i32.add (local.get $offset) (local.get $j))))
        (i32.store8
          (i32.add (local.get $offset) (local.get $j))
          (local.get $swap))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (local.set $j (i32.sub (local.get $j) (i32.const 1)))
        (br $rloop)
      )
    )

    (local.get $len)
  )

  ;; strcpy: copy $len bytes from $src to $dst, return dst + len
  (func $strcpy (param $dst i32) (param $src i32) (param $len i32) (result i32)
    (local $i i32)
    (local.set $i (i32.const 0))
    (block $done
      (loop $loop
        (br_if $done (i32.ge_u (local.get $i) (local.get $len)))
        (i32.store8
          (i32.add (local.get $dst) (local.get $i))
          (i32.load8_u (i32.add (local.get $src) (local.get $i))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $loop)
      )
    )
    (i32.add (local.get $dst) (local.get $len))
  )

  ;; write_num: write itoa at $dst, return dst + len
  (func $write_num (param $dst i32) (param $val i32) (result i32)
    (local $len i32)
    (local.set $len (call $itoa (local.get $val) (local.get $dst)))
    (i32.add (local.get $dst) (local.get $len))
  )

  ;; println: write buffer at 1024, length = $end - 1024, append newline, flush
  (func $println (param $end i32)
    (i32.store8 (local.get $end) (i32.const 10)) ;; '\n'
    (drop (call $yos_write
      (i32.const 1)
      (i32.const 1024)
      (i32.sub (i32.add (local.get $end) (i32.const 1)) (i32.const 1024))))
  )

  ;; String data
  (data (i32.const 256) "starting fork test")                ;; 18 bytes
  (data (i32.const 280) "forked child, child_pid=")          ;; 24 bytes
  (data (i32.const 310) "child exited with code=")           ;; 23 bytes
  (data (i32.const 340) "hello from forked child! pid=")    ;; 29 bytes
  (data (i32.const 370) " ppid=")                            ;; 6 bytes
  (data (i32.const 380) "done")                              ;; 4 bytes

  (func $_start (export "_start")
    (local $fork_res i32)
    (local $child_pid i32)
    (local $child_exit i32)
    (local $pos i32)

    ;; Are we a forked child?
    (local.set $fork_res (call $yos_fork_result))

    (if (i32.eqz (local.get $fork_res))
      (then
        ;; === CHILD ===
        ;; "hello from forked child! pid=<N> ppid=<N>"
        (local.set $pos (call $strcpy (i32.const 1024) (i32.const 340) (i32.const 29)))
        (local.set $pos (call $write_num (local.get $pos) (call $yos_getpid)))
        (local.set $pos (call $strcpy (local.get $pos) (i32.const 370) (i32.const 6)))
        (local.set $pos (call $write_num (local.get $pos) (call $yos_getppid)))
        (call $println (local.get $pos))

        (call $yos_exit (i32.const 42))
        (return)
      )
    )

    ;; === PARENT ===
    ;; "starting fork test"
    (local.set $pos (call $strcpy (i32.const 1024) (i32.const 256) (i32.const 18)))
    (call $println (local.get $pos))

    ;; Fork
    (local.set $child_pid (call $yos_fork))

    ;; "forked child, child_pid=<N>"
    (local.set $pos (call $strcpy (i32.const 1024) (i32.const 280) (i32.const 24)))
    (local.set $pos (call $write_num (local.get $pos) (local.get $child_pid)))
    (call $println (local.get $pos))

    ;; Wait for child
    (local.set $child_exit (call $yos_wait (local.get $child_pid)))

    ;; "child exited with code=<N>"
    (local.set $pos (call $strcpy (i32.const 1024) (i32.const 310) (i32.const 23)))
    (local.set $pos (call $write_num (local.get $pos) (local.get $child_exit)))
    (call $println (local.get $pos))

    ;; "done"
    (local.set $pos (call $strcpy (i32.const 1024) (i32.const 380) (i32.const 4)))
    (call $println (local.get $pos))

    (call $yos_exit (i32.const 0))
  )
)
