#ifndef FT_STRLENS_H
# define FT_STRLENS_H
# include <stddef.h>
# include <stdint.h>

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

#endif
