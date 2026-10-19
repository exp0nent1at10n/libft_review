
#include "libft.h"

size_t	ft_strlen(const char *s)
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
