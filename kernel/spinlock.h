// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
#ifdef LAB_LOCK
  int nts;
  int n;
#endif
};

#ifdef LAB_LOCK
// Reader-writer lock.
struct rwspinlock {
  struct spinlock l;  // Internal spinlock to serialize state updates
  int readers;           // Number of active readers
  int writer;            // 1 if a writer is active, 0 otherwise
  int pending_writers;   // Number of writers waiting to acquire
  char *name;            // Name for debugging
};
#endif