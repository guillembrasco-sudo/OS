#ifndef KERNEL_RCU_H
#define KERNEL_RCU_H

void rcu_init(void);
void rcu_read_lock(void);
void rcu_read_unlock(void);
void synchronize_rcu(void);

#endif