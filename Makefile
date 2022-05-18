default: proj2

proj2: proj2.c
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic -pthread proj2.c -o proj2

clean:
	rm -f proj2