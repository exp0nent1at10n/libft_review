#include "libft.h"
#include <fcntl.h>
#include <string.h>
#include <limits.h>

int	main(void)
{
	size_t	buff_size = (size_t)INT_MAX + 42;
	char	*buf;
	int		fd;

	buf = malloc(buff_size);
	if (buf == NULL)
		return (1);
	memset(buf, 'A', buff_size - 1);
	buf[buff_size - 1] = '\0';
	fd = open("/dev/null", O_WRONLY);
	if (fd == -1)
		return (free(buf), 1);
	ft_putstr_fd(buf, fd);
	close(fd);
	free(buf);
	return (0);
}
