# KXN

**KXN** is a minimal 8bit stack-based virtual machine with a fully addressable 64Kb memory, a custom ISA, assembler, and a Tiny C-like compiler.
It has supports basic arithmetic, logic, control flow, memory, and SDL2-based graphics operations.
The project is inspired by [uxn](https://100r.co/site/uxn.html) and [paper computing](https://wiki.xxiivv.com/site/paper_computing.html) and I do recommend everyone to check them out.

## Build

```bash
make                # Build all components
sudo make install   # Install binaries system-wide (/usr/local/bin by default)
sudo make uninstall # Remove installed binaries
make clean          # Remove compiled binaries
```

---

## Binaries

| Binary  | Description               |
| ------- | ------------------------- |
| `kxn`   | The KXN virtual machine   |
| `kxasm` | Assembler for the KXN ISA |
| `tinyc` | Tiny C-like compiler      |

---

## ISA (Instruction Set Architecture)

| Opcode     | Hex  | Description                   |
| ---------- | ---- | ----------------------------- |
| NOP        | 0x00 | Do nothing                    |
| HALT       | 0x01 | Stop execution                |
| PUSH       | 0x02 | Push 8-bit immediate value    |
| POP        | 0x03 | Pop top value                 |
| DUP        | 0x04 | Duplicate top value           |
| SWAP       | 0x05 | Swap top two values           |
| ADD        | 0x06 | Pop 2, push a + b             |
| SUB        | 0x07 | Pop 2, push a - b             |
| MUL        | 0x08 | Pop 2, push a \* b            |
| DIV        | 0x09 | Pop 2, push a / b             |
| MOD        | 0x0A | Pop 2, push a % b             |
| NEG        | 0x0B | Pop 1, push -a                |
| AND        | 0x0C | Pop 2, push a & b             |
| OR         | 0x0D | Pop 2, push a \| b            |
| XOR        | 0x0E | Pop 2, push a ^ b             |
| NOT        | 0x0F | Pop 1, push \~a               |
| SHL        | 0x10 | Pop 2, push a << b            |
| SHR        | 0x11 | Pop 2, push a >> b            |
| EQ         | 0x12 | Pop 2, push 1 if equal else 0 |
| NEQ        | 0x13 | Pop 2, push 1 if not equal    |
| GT         | 0x14 | Pop 2, push 1 if a > b        |
| LT         | 0x15 | Pop 2, push 1 if a < b        |
| GTE        | 0x16 | Pop 2, push 1 if a >= b       |
| LTE        | 0x17 | Pop 2, push 1 if a <= b       |
| LOAD       | 0x18 | Push value at address         |
| STORE      | 0x19 | Pop value, store to address   |
| LOAD\_IND  | 0x1A | Pop addr, push value at addr  |
| STORE\_IND | 0x1B | Pop addr, pop val, store val  |
| JMP        | 0x1C | Jump unconditionally          |
| JZ         | 0x1D | Pop, if zero, jump            |
| JNZ        | 0x1E | Pop, if not zero, jump        |
| CALL       | 0x1F | Call subroutine               |
| RET        | 0x20 | Return from subroutine        |
| SYS        | 0x21 | Perform syscall with ID imm8  |

---

## IO Calls

| ID   | Description                            |
| ---- | -------------------------------------- |
| 0x00 | Exit program                           |
| 0x01 | Print character (pop ASCII)            |
| 0x02 | Read character from stdin, push ASCII  |
| 0x10 | Draw pixel (pop x, y, color)           |
| 0x11 | Draw line (pop x1, y1, x2, y2, color)  |
| 0x12 | Fill rectangle (pop x, y, w, h, color) |
| 0x13 | Refresh display buffer                 |
| 0x20 | Poll keyboard, push 1 if key available |
| 0x21 | Get key, push ASCII code               |
| 0x22 | Poll mouse, push 1 if mouse event      |
| 0x23 | Get mouse X, push lo, hi               |
| 0x24 | Get mouse Y, push lo, hi               |
| 0x25 | Get mouse buttons, push 8-bit flags    |

---

## Examples
I have included a basic example, once I add more once I update the documentation and improve the language.

---
