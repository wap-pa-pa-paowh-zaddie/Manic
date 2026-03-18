all: controller probe

controller: controller.c common.h
	gcc -Wall -Wextra -std=c11 -o controller controller.c

probe: probe.c common.h
	gcc -Wall -Wextra -std=c11 -o probe probe.c

clean:
	rm -f controller probe