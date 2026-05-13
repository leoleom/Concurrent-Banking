# Concurrent Banking

## Group Members

- **Leona Mae Blancaflor**
- **Chrystie Rae A. Sajorne**

---

## Overview

A concurrent banking system implemented in C that simulates deposits, withdrawals, transfers, and balance checks in a multi‑threaded environment. The system demonstrates synchronization, deadlock prevention, buffer pool resource management, and transaction scheduling using a global timer.

---

## Compilation Instructions

### Prerequisites
- GCC compiler (C99 standard)
- Make build tool
- POSIX-compliant system

### Building the Project

```bash
cd Concurrent-Banking
make clean    # Remove previous builds
make debug    
make          # Compile the project
```
---

## Usage Instructions

### Basic Syntax
```bash
./banksim --accounts=<FILE> --trace=<FILE> [--tick-ms=<VALUE>] [--verbose]
```

### Command-Line Arguments

| Argument | Required | Description |
|----------|----------|-------------|
| `--accounts=<FILE>` | Yes | Input file with account definitions |
| `--input=<FILE>` | Yes| Input file with transaction trace |
| `--tick-ms=<VALUE>` | No | Tick interval in milliseconds (default: 100) |
| `--verbose` | No | 	Print detailed transaction logs (otherwise only metrics and buffer pool report are shown) |

### Input File Format

Each line defines an account with two space‑separated values:
```
<ACCOUNT_ID> <INITIAL_BALANCE>
```

**Example file (`tests/accounts.txt`):**
```
0   10000
1   25000
10  50000
20  30000
```

### Trace File Format

Each line defines an account with two space‑separated values:
```
T<ID> <START_TICK> <OPERATION> <AccountID> <TargetAccount> <Amount>  
```

**Example file (`tests/trace.txt`):**
```
# Transaction 1: Simple deposit
T1  0  DEPOSIT   10  5000

# Transaction 2: Withdrawal
T2  1  WITHDRAW  10  2000

# Transaction 3: Transfer (can deadlock with T4)
T3  2  TRANSFER  10  20  3000

# Transaction 4: Concurrent transfer in opposite direction
T4  2  TRANSFER  20  10  1500

# Transaction 5: Balance inquiry
T5  5  BALANCE   10
```
---
## Implemented Features

1. **Concurrent Transactions**
    - Each transaction runs in its own pthread.

2. **Operations Supported**
   - Deposit
   - Withdraw
   - Transfer
   - Balance Check

3. **Deadlock Prevention**
   - Locks acquired in ascending account ID order to break circular wait.

4. **Buffer Pool Report**
   - Pool size (5 slots)
   - Total loads and unloads
   - Peak pool usage
   - Blocked Operations

5. **Verbose Logging**
   - Optional detailed transaction logs with --verbose.

6. **Metrics Reporting:**
   - Transaction wait ticks
   - Commit/abort counts
   - Conservation check (initial vs. final total balance)
   - Average wait time
   - Throughput

## Known Limitations

1. **Single Bank Instance:** Only one bank is simulated at a time.

2. **Fixed Buffer Pool Size:** Limited to 5 slots; may block if heavily contended.

3. **No Deadlock Detection Mode:** Only prevention strategy is implemented.

4. **No I/O Simulation:** Transactions only model CPU‑bound operations.

5. **No Context Switch Cost:** Lock acquisition and release assumed instantaneous.
## Project Structure

---
```
Concurrent Banking/
├── src/                    # Implementation files
│   ├── bank.c             # Entry point with argument parsing
│   ├── buffer_pool.c        # Core scheduling engine
│   ├── lock_mgr.c            # FCFS algorithm implementation
│   ├── main.c             # SJF algorithm implementation
│   ├── metrics.c            # STCF algorithm implementation
│   ├── parser.c              # Round Robin algorithm implementation
│   ├── timer.c            # MLFQ algorithm implementation
│   ├── transaction.c         # Process management
│   ├── utils.c            # Min heap data structure
├── include/               # Header files
│   ├── bank.h       # Scheduler interface
│   ├── buffer_pool.h         # Process structures
│   ├── lock_mgr.h           # Gantt chart interface
│   ├── metrics.h         # Metrics calculation interface
│   ├── parser.h            # Heap interface
│   ├── timer.h          # Heap interface
│   ├── transaction.h
    └── utils.h
├── tests/                           #  Initial account balances
│   ├── acounts.txt                  # Test input files
│   ├── trace_abort.txt              # Test 1
│   ├── trace_buffer.sh              # Test 2    
│   ├── trace_deadlock.txt           # Test 3  
|   ├── trace_readers.txt            # Test 4 
|   ├── trace_simple.txt             # Test 5
│   └── trace.txt                    # Additional test                  
├── docs/                     # Documentation
│   └── design.md         # design justification
├── Makefile              # Build configuration
└── README.md            
```
---

## Cleaning Up

To remove compiled files and binaries:
```bash
make clean
```

## Automatest test

```bash
make test
```
