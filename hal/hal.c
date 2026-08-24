int arch_cpu_init(void);

int hal_init(void)
{
	return arch_cpu_init();
}
