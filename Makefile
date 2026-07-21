PYTHON ?= python3

.PHONY: test reference-avm milestone-0

test:
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

reference-avm:
	sh scripts/setup-avm.sh

milestone-0: reference-avm test
	$(PYTHON) tools/run_m0_matrix.py