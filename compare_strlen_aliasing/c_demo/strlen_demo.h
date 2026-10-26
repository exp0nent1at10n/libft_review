#ifndef STRLEN_DEMO_H
# define STRLEN_DEMO_H
# include <stddef.h>
# include <stdint.h>

# define BUF_BYTES	16
# define POOL_BYTES	64
# define LEN_CNT	3

void	strlen_no_optimize_char(char *pool, const int *lens, int cnt,
			int out[3][LEN_CNT]);
void	strlen_no_optimize_uint32(char *pool, const int *lens, int cnt,
			int out[3][LEN_CNT]);
void	strlen_no_optimize_uint64(char *pool, const int *lens, int cnt,
			int out[3][LEN_CNT]);

#endif
