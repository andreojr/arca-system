PYTHON := .venv/bin/python

.DEFAULT_GOAL := sync
.PHONY: sync enroll delete

sync:
	$(PYTHON) -m scripts.sync_rtc

enroll:
	$(PYTHON) -m scripts.enroll

delete:
	$(PYTHON) -m scripts.delete
