NAME	= strlen_demo
CC		= gcc
CFLAGS	= -Wall -Wextra
SRCS	= main.c strlen_no_optimize.c
MODES	= O0 O2 O3 nsa

O0:		OPT = -O0
O2:		OPT = -O2
O3:		OPT = -O3
nsa:	OPT = -O2 -fno-strict-aliasing

all:
	@for m in $(MODES); do $(MAKE) --no-print-directory $$m || exit 1; done

$(MODES):
	@echo "== $(OPT)"
	@$(CC) $(CFLAGS) $(OPT) $(SRCS) -o $(NAME)
	@./$(NAME)

clean:
	rm -f *.o *.s dump_*.txt

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all $(MODES) clean fclean re
