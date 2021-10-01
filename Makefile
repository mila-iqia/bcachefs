

coverage:
	mkdir -p build/coverage
	gcc -I. -g -fprofile-arcs -ftest-coverage tests/benzina/bcachefs/bcachefs_tests.c bcachefs/bcachefs.c -lgcov -o build/bcachefs_tests
	./build/bcachefs_tests testdata/mini_bcachefs.img
	mv *.gcda *.gcno build/coverage/ 
	gcovr -r . -e tests build/coverage/
	gcovr -r . -e tests build/coverage/ --html -o build/coverage/report.html

