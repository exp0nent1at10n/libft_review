#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BUF_BYTES	16
#define POOL_BYTES	64
#define LEN_CNT		3

typedef size_t __attribute__((__may_alias__))	alias_size_t;

static inline __attribute__((always_inline)) size_t	ft_strlen(const char *s)
{
	size_t			count;

	count = 0;
	while (*s)
	{
		s++;
		count++;
	}
	return (count);
}

static inline __attribute__((always_inline)) size_t	ft_strlen_size_t(const char *s)
{
	const char		*start;
	const size_t	*wide_s;
	size_t			ones;
	size_t			highs;

	start = s;
	while ((uintptr_t)s % (uintptr_t) sizeof(size_t))
	{
		if (!*s)
			return (s - start);
		s++;
	}
	ones = SIZE_MAX / 0xFF;
	highs = ones * 0x80;
	wide_s = (const size_t *)s;
	while (!((*wide_s - ones) & ~(*wide_s) & highs))
		wide_s++;
	s = (const char *)wide_s;
	while (*s)
		s++;
	return (s - start);
}

static inline __attribute__((always_inline)) size_t	ft_strlen_may_alias(const char *s)
{
	const char			*start;
	const alias_size_t	*wide_s;
	size_t				ones;
	size_t				highs;

	start = s;
	while ((uintptr_t)s % (uintptr_t) sizeof(size_t))
	{
		if (!*s)
			return (s - start);
		s++;
	}
	ones = SIZE_MAX / 0xFF;
	highs = ones * 0x80;
	wide_s = (const alias_size_t *)s;
	while (!((*wide_s - ones) & ~(*wide_s) & highs))
		wide_s++;
	s = (const char *)wide_s;
	while (*s)
		s++;
	return (s - start);
}

static void	reset_pool(char *pool)
{
	volatile char	*vp;
	int				i;

	vp = pool;
	for (i = 0; i < POOL_BYTES - 1; i++)
		vp[i] = 'A';
	vp[POOL_BYTES - 1] = 0;
}

static inline __attribute__((always_inline)) void	make_string(unsigned char *b, int n)
{
	int	i;

	for (i = 0; i < BUF_BYTES; i++)
		b[i] = i < n ? (unsigned char)('A' + i % 26) : 0;
}

static inline __attribute__((always_inline)) void	write_char(char *buf, int n)
{
	unsigned char	b[BUF_BYTES];
	int				i;

	make_string(b, n);
	for (i = 0; i < BUF_BYTES; i++)
		buf[i] = (char)b[i];
}

static inline __attribute__((always_inline)) void	write_uint32(uint32_t *buf, int n)
{
	unsigned char	b[BUF_BYTES];
	uint32_t		v;
	int				i;

	make_string(b, n);
	for (i = 0; i < BUF_BYTES / 4; i++)
	{
		memcpy(&v, b + 4 * i, 4);
		buf[i] = v;
	}
}

static inline __attribute__((always_inline)) void	write_uint64(uint64_t *buf, int n)
{
	unsigned char	b[BUF_BYTES];
	uint64_t		v;
	int				i;

	make_string(b, n);
	for (i = 0; i < BUF_BYTES / 8; i++)
	{
		memcpy(&v, b + 8 * i, 8);
		buf[i] = v;
	}
}

static __attribute__((noipa)) void	strlen_no_optimize_char(char *pool,
	const int *lens, int cnt, int out[3][LEN_CNT])
{
	char	*buf = __builtin_assume_aligned(pool, 8);
	int		k;

	for (k = 0; k < cnt; k++)
	{
		write_char(buf, lens[k]);
		out[0][k] = (int)ft_strlen(buf);
	}
	for (k = 0; k < cnt; k++)
	{
		write_char(buf, lens[k]);
		out[1][k] = (int)ft_strlen_size_t(buf);
	}
	for (k = 0; k < cnt; k++)
	{
		write_char(buf, lens[k]);
		out[2][k] = (int)ft_strlen_may_alias(buf);
	}
}

static __attribute__((noipa)) void	strlen_no_optimize_uint32(char *pool,
	const int *lens, int cnt, int out[3][LEN_CNT])
{
	uint32_t	*buf = __builtin_assume_aligned(pool, 8);
	int			k;

	for (k = 0; k < cnt; k++)
	{
		write_uint32(buf, lens[k]);
		out[0][k] = (int)ft_strlen((const char *)buf);
	}
	for (k = 0; k < cnt; k++)
	{
		write_uint32(buf, lens[k]);
		out[1][k] = (int)ft_strlen_size_t((const char *)buf);
	}
	for (k = 0; k < cnt; k++)
	{
		write_uint32(buf, lens[k]);
		out[2][k] = (int)ft_strlen_may_alias((const char *)buf);
	}
}

static __attribute__((noipa)) void	strlen_no_optimize_uint64(char *pool,
	const int *lens, int cnt, int out[3][LEN_CNT])
{
	uint64_t	*buf = __builtin_assume_aligned(pool, 8);
	int			k;

	for (k = 0; k < cnt; k++)
	{
		write_uint64(buf, lens[k]);
		out[0][k] = (int)ft_strlen((const char *)buf);
	}
	for (k = 0; k < cnt; k++)
	{
		write_uint64(buf, lens[k]);
		out[1][k] = (int)ft_strlen_size_t((const char *)buf);
	}
	for (k = 0; k < cnt; k++)
	{
		write_uint64(buf, lens[k]);
		out[2][k] = (int)ft_strlen_may_alias((const char *)buf);
	}
}

static void	print_result(const char *writer, const int *lens, int out[3][LEN_CNT])
{
	int	k;

	printf("[%s 書き込み]\n", writer);
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
