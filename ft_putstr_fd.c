/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_putstr_fd.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: smukai <smukai@student.42tokyo.jp>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/22 20:33:45 by smukai            #+#    #+#             */
/*   Updated: 2026/10/06 18:15:12 by smukai           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "libft.h"

size_t	ft_strlen(const char *s);

void	ft_putstr_fd(char *s, int fd)
{
	size_t	length_remains;
	ssize_t	written;

	length_remains = ft_strlen((const char *)s);
	while (length_remains > 0)
	{
		written = write(fd, s, length_remains);
		if (written < 0)
			return ;
		s += written;
		length_remains -= written;
	}
}
