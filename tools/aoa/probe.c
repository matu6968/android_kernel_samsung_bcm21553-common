/* GPL-2.0: freestanding ARM EABI probe; no dependency on the phone's libc. */
static char buffer[16384];
static long call3(long nr, long a, long b, long c)
{
	register long r0 __asm__("r0") = a;
	register long r1 __asm__("r1") = b;
	register long r2 __asm__("r2") = c;
	register long r7 __asm__("r7") = nr;
	__asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
	return r0;
}
static void number(long n)
{
	char out[32];
	int i = 0, j;
	if (n < 0) { call3(4, 1, (long)"-", 1); n = -n; }
	do { /* Avoid division / libc helpers. */
		long q = 0, rem = n;
		unsigned long bit = 1, d = 10;
		while (d <= (unsigned long)n >> 1) { d <<= 1; bit <<= 1; }
		while (bit) { if ((unsigned long)rem >= d) { rem -= d; q += bit; } bit >>= 1; d >>= 1; }
		out[i++] = '0' + rem; n = q;
	} while (n);
	for (j = i-1; j >= 0; j--) call3(4, 1, (long)&out[j], 1);
	call3(4, 1, (long)"\n", 1);
}
long probe_main(long *stack)
{
	long fd = call3(5, (long)"/dev/usb_accessory", 2, 0);
	long ret;
	unsigned i, j;
	char **argv = (char **)(stack+1);
	if (fd < 0) { number(fd); return 1; }
	if (stack[0] > 1 && argv[1][0] == 'w') {
		for (i = 0; i < 16; i++) {
			ret = call3(4, fd, (long)buffer, sizeof(buffer));
			if (ret < 0) { number(ret); call3(6, fd, 0, 0); return 0; }
		}
		return 1;
	}
	if (stack[0] > 1 && argv[1][0] == 'r') {
		ret = call3(3, fd, (long)buffer, sizeof(buffer));
		number(ret);
		call3(6, fd, 0, 0);
		return ret == -19 ? 0 : 1; /* ENODEV after disconnect */
	}
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 256; j++) buffer[j] = 0;
		ret = call3(54, fd, 0x41004d01UL+i, (long)buffer);
		if (ret < 0 || ret > 256) { number(ret); return 1; }
		call3(4, 1, (long)buffer, ret > 0 ? ret-1 : 0);
		call3(4, 1, (long)"\n", 1);
	}
	ret = call3(54, fd, 0x4d07, 0);
	number(ret);
	call3(6, fd, 0, 0);
	return 0;
}
__asm__(".global _start\n_start:\n mov r0, sp\n bl probe_main\n mov r7, #1\n svc 0\n");
