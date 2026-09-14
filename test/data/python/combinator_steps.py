import os
from sys import argv


class Base:
    pass


class Widget(Base):
    """A widget."""

    def __init__(self, name):
        self.name = name

    def test_render(self):
        for i in range(3):
            print(i)

    def undocumented(self):
        return 1


def main():
    """Entry point."""
    try:
        os.getcwd()
    except OSError:
        pass


def test_helper():
    x = 1
    return x


def calls_nothing():
    return 2
