PYTHON := .venv/bin/python

.DEFAULT_GOAL := status
.PHONY: status sync enroll delete

status:
	$(PYTHON) -m scripts.status

sync:
	$(PYTHON) -m scripts.sync_rtc

enroll:
	$(PYTHON) -m scripts.enroll

delete:
	$(PYTHON) -m scripts.delete
