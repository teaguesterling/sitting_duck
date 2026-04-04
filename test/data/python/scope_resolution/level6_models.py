# Level 6: Cross-file import target
# This file is imported by level6_cross_file.py

class User:
    def __init__(self, name, email):
        self.name = name
        self.email = email

    def display_name(self):
        return self.name.title()

    def is_valid(self):
        return "@" in self.email


def create_user(name, email):
    return User(name, email)


DEFAULT_ADMIN = "admin@example.com"
