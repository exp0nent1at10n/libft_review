#include <string.h>
#include "strlen_demo.h"
#include "ft_strlens.h"

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

__attribute__((noipa)) void	strlen_no_optimize_char(char *pool,
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

__attribute__((noipa)) void	strlen_no_optimize_uint32(char *pool,
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

__attribute__((noipa)) void	strlen_no_optimize_uint64(char *pool,
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
