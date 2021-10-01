

coverage:
	mkdir -p build/coverage
	gcc -I. -g -fprofile-arcs -ftest-coverage bcachefs/bcachefs_tests.c bcachefs/bcachefs.c -lgcov -o build/bcachefs_tests
	./build/bcachefs_tests testdata/mini_bcachefs.img
	mv *.gcda *.gcno build/coverage/ 
	gcovr build/coverage/

