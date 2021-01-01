import os
import logging

from contextlib import contextmanager


logger = logging.getLogger(__name__)


@contextmanager
def pidfile(filename):
    try:
        logger.debug(f"creating pid-file: {filename}")
        with open(filename, "w") as f:
            f.write(str(os.getpid()))
        yield
    finally:
        logger.debug(f"deleting pid-file: {filename}")
        os.unlink(filename)
