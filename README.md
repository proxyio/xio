==== compile and test ====

	$ cd build
	$ cmake .. && make && make test
	or
	$ cmake .. && make && ./bin/utest to see more test details

==== gcov ====

	$ cd build
	$ cmake .. && make && make test

	$ lcov -d ../ -c -o pio.cov
	$ genhtml -o piocov pio.cov

	$ gnome-open ./apicov/index.html


==== rpm ====

	$ cd rpm
	$ ./package-pio.sh
