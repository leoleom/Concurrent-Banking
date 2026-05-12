# Banking System Design

## 1. Analysis

This project implements a concurrent banking system where multiple transactions operate on shared account records, simulating a multi‑threaded environment in which deposits, withdrawals, transfers, and balance checks are executed in parallel.  

### Concurrency Risks
- Threads may access the same account at the same time.
- Without synchronization, this can lead to inconsistent balances.
- Operations must behave atomically and preserve account consistency.

### Deadlock Risk
- Transfers require locks on two accounts.
- If threads acquire locks in different orders, a circular wait can arise.
- The design must avoid such cycles.

### Resource Constraints
- A buffer pool simulates limited memory.
- Accounts must be loaded into the pool before use and released afterward.

---

## 2. Architecture

### Core Components
- **bank.c**: Manages account storage and balance updates.
- **transaction.c**: Runs transaction operations in separate threads.
- **lock_mgr.c**: Coordinates account locking and ordering.
- **buffer_pool.c**: Handles loading/unloading accounts in limited slots.
- **timer.c**: Advances a global tick counter for scheduling.
- **main.c**: Initializes components and launches transaction threads.

### Concurrency Design
- Each transaction executes in its own pthread.
- The system uses a global tick counter to pace transaction start times.
- Account access is protected by locks and buffer pool control.

---

## 3. Deadlock Strategy Choice

### Selected Strategy
The system uses **deadlock prevention** by enforcing a fixed lock order.

### Why this strategy?
- It avoids the complexity of deadlock detection and rollback.
- It provides predictable behavior under concurrent transfers.
- It is sufficient for the bounded account set in this project.
- It keeps the implementation simpler and easier to verify.

### Which Coffman condition is broken?
The design breaks **Circular Wait**, the fourth Coffman condition.

### How it prevents deadlock
All account locks are acquired in ascending account ID order. This means a transaction holding a lower-ID account will only wait for a higher-ID account, and never vice versa. Because all threads follow the same ordering, cycles cannot form.

---

## 4. Buffer Pool Integration

We implemented **load/unload per operation** based on analysis of the codebase and system constraints.

### Loading policy
Accounts are loaded into the buffer pool on a per-operation basis. Before any banking action, the transaction requests the account be loaded.

### Unload policy
After the operation finishes, the account is immediately unloaded. This happens after success or failure, so buffer slots are freed promptly.

#### Implementation Details

**In `execute_transaction()` (src/transaction.c)**, each operation explicitly manages buffer pool lifecycle:

```c
// Example: DEPOSIT operation
Account *acc = bank_find_account(g_bank, op->account_id);
buffer_pool_load(g_pool, op->account_id, acc);    // Load before operation
deposit(op->account_id, op->amount_centavos);      // Execute operation
buffer_pool_unload(g_pool, op->account_id);        // Unload after operation
```

For **TRANSFER operations** (requiring two accounts):
```c
buffer_pool_load(g_pool, op->account_id, src);              // Load source
buffer_pool_load(g_pool, op->target_account, dst);          // Load destination
// ... execute transfer ...
buffer_pool_unload(g_pool, op->account_id);                 // Unload source
buffer_pool_unload(g_pool, op->target_account);             // Unload destination
```

#### Buffer Pool Implementation (src/buffer_pool.c)

The buffer pool uses a **producer-consumer pattern** with semaphores:

- **Fixed size**: `BUFFER_POOL_SIZE = 5` slots (defined in buffer_pool.h)
- **Synchronization**: 
  - `empty_slots` semaphore: tracks available slots for loading
  - `full_slots` semaphore: tracks loaded accounts ready for access
  - `pool_lock` mutex: protects critical sections during load/unload


### Full-pool handling
If every pool slot is occupied, the transaction waits until a slot becomes free. The pool uses semaphores to block and resume transactions safely.

### Performance Justification

#### Why Per-Operation Loading Works Well

1. **Bounded Buffer Pool Size**: With only 5 slots and typically 1-4 accounts per operation, the pool rarely saturates. This minimizes blocking and provides fair resource allocation across concurrent transactions.

2. **Reduced Contention**: By releasing accounts immediately after operations, we minimize the time other transactions are blocked waiting for slots. This improves throughput in high-contention scenarios.

3. **Deadlock Prevention**: Load/unload per operation works naturally with our deadlock prevention mechanisms:
   - For TRANSFER operations, we load both source and destination before executing, ensuring atomicity
   - We release both slots together after completion
   - The ordered lock acquisition (source first, destination second) combined with per-operation loading prevents deadlock cycles

4. **Memory Efficiency**: Accounts are only held in the buffer pool for the duration of their operation, not for the entire transaction. This allows other transactions to access the same accounts quickly.

### Correctness Justification

1. **Atomicity**: Each operation loads its required account(s), executes atomically, then unloads. The lock manager ensures mutual exclusion during execution.

2. **Isolation**: The lock manager isolates concurrent operations on the same account. Buffer pool operations are protected by `pool_lock` mutex, ensuring no race conditions during load/unload.

3. **Consistency**: Accounts are never in an inconsistent state in the buffer pool:
   - On abort: we unload the account even if the operation failed (see WITHDRAW/TRANSFER abort paths in src/transaction.c)
   - On commit: we've already released the account, so the persistent bank state is consistent

4. **Simplicity**: Per-operation loading is easier to reason about and verify than alternatives. The coupling between operation execution and buffer pool management is explicit and localized to each operation handler.

---

## 5. Reader-Writer Lock Performance

### Lock type chosen
The system uses `pthread_rwlock_t` for account synchronization.

---

## 6. Timer Thread Design

### Why a separate timer thread is necessary
A dedicated timer thread advances a global tick counter and allows transactions to begin at scheduled ticks.

### What breaks without the timer
If transactions ran one after the other, the system would not exercise true concurrent behavior. Lock contention, buffer pool blocking, and concurrent scheduling would not be tested meaningfully.

### How the timer enables true concurrency testing
- Transactions wait for their assigned start tick.
- The timer broadcasts tick updates to waiting threads.
- This creates controlled overlap among transaction executions.
- Varying the tick interval changes how much transactions overlap, making concurrency behavior observable.

---

## 7. Summary

The final design supports concurrent banking operations with:
- thread-based transaction execution,
- deadlock prevention through fixed lock ordering,
- buffer pool resource management with semaphores,
- and timer-based scheduling for reproducible concurrency.

This approach balances correctness and performance while keeping the implementation grounded in the actual codebase.





