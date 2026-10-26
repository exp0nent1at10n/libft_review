#include <stdio.h>
#include "strlen_demo.h"

// Fill pool with 'A' (NUL at the end) via volatile so a stale word load sees no zero byte.
static void	reset_pool(char *pool)
{
	volatile char	*vp;
	int				i;

	vp = pool;
	for (i = 0; i < POOL_BYTES - 1; i++)
		vp[i] = 'A';
	vp[POOL_BYTES - 1] = 0;
}

static void	print_result(const char *writer, const int *lens, int out[3][LEN_CNT])
{
	int	k;

	printf("[%s written]\n", writer);
	for (k = 0; k < LEN_CNT; k++)
		printf("N=%d: ft_strlen=%d  ft_strlen_size_t=%d  (may_alias=%d)\n",
			lens[k], out[0][k], out[1][k], out[2][k]);
}

int	main(void)
{
	static char			pool[POOL_BYTES] __attribute__((aligned(64)));
	static const int	lens[LEN_CNT] = {7, 8, 9};
	int					out[3][LEN_CNT];

	reset_pool(pool);
	strlen_no_optimize_char(pool, lens, LEN_CNT, out);
	print_result("char", lens, out);
	reset_pool(pool);
	strlen_no_optimize_uint32(pool, lens, LEN_CNT, out);
	print_result("uint32_t", lens, out);
	reset_pool(pool);
	strlen_no_optimize_uint64(pool, lens, LEN_CNT, out);
	print_result("uint64_t", lens, out);
	return (0);
}
